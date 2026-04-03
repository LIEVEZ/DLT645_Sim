#include "ProtocolController.h"
#include "Utils/Logger.h"
#include <QDebug>
#include <algorithm>

ProtocolController::ProtocolController(QObject *parent)
    : QObject(parent),
      m_meterModel(new MeterModel(this)),
      m_serialController(nullptr),
      m_tcpController(nullptr),
      m_protocolParser(new ProtocolParser(m_meterModel, this)),
      m_autoResponseEnabled(true),
      m_responseDelay(100),
      m_hasMeter(false),
      m_responseTimer(new QTimer(this)),
      m_lastRequestMeterModel(nullptr)
{
    connect(m_protocolParser, &ProtocolParser::frameParsed, this, &ProtocolController::onProtocolFrameParsed);
    connect(m_protocolParser, &ProtocolParser::parseError, this, &ProtocolController::onProtocolParseError);

    m_responseTimer->setSingleShot(true);
    connect(m_responseTimer, &QTimer::timeout, this, &ProtocolController::onResponseTimerTimeout);
}

ProtocolController::~ProtocolController()
{
    if (m_responseTimer)
    {
        m_responseTimer->stop();
    }
}

MeterModel *ProtocolController::meterModel() const
{
    return m_meterModel;
}

void ProtocolController::setMeterModel(MeterModel *meterModel)
{
    m_meterModel = meterModel;
    m_protocolParser->setMeterModel(meterModel);
}

void ProtocolController::registerMeterModel(const QString &address, MeterModel *meterModel)
{
    if (address.isEmpty() || !meterModel)
    {
        return;
    }

    m_meterModelsByAddress.insert(address, meterModel);
}

void ProtocolController::unregisterMeterModel(const QString &address)
{
    if (address.isEmpty())
    {
        return;
    }

    MeterModel *removedModel = m_meterModelsByAddress.take(address);
    if (m_lastRequestMeterModel == removedModel)
    {
        m_lastRequestMeterModel = nullptr;
        m_lastRequestFrame = ProtocolFrame();
        m_responseTimer->stop();
    }
}

void ProtocolController::setSerialController(SerialController *controller)
{
    if (m_serialController)
    {
        disconnect(m_serialController, nullptr, this, nullptr);
    }

    m_serialController = controller;

    if (m_serialController)
    {
        connect(m_serialController, &SerialController::dataReceived, this, &ProtocolController::onSerialDataReceived);
        connect(m_serialController, &SerialController::dataSent, this, &ProtocolController::onSerialDataSent);
        connect(m_serialController, &SerialController::connectionStateChanged, this, &ProtocolController::onSerialConnectionStateChanged);
    }
}

void ProtocolController::setTcpController(TcpController *controller)
{
    if (m_tcpController)
    {
        disconnect(m_tcpController, nullptr, this, nullptr);
    }

    m_tcpController = controller;

    if (m_tcpController)
    {
        connect(m_tcpController, &TcpController::dataReceived, this, &ProtocolController::onTcpDataReceived);
        connect(m_tcpController, &TcpController::dataSent, this, &ProtocolController::onTcpDataSent);
        connect(m_tcpController, &TcpController::connectionStateChanged, this, &ProtocolController::onTcpConnectionStateChanged);
    }
}

void ProtocolController::processReceivedData(const QByteArray &data)
{
    m_receiveBuffer.append(data);

    qDebug() << "processReceivedData data:" << data.toHex();
    while (!m_receiveBuffer.isEmpty())
    {
        int startIndex = 0;
        while (startIndex < m_receiveBuffer.size() &&
               static_cast<quint8>(m_receiveBuffer[startIndex]) == 0xFE)
        {
            startIndex++;
        }

        if (startIndex >= m_receiveBuffer.size())
        {
            m_receiveBuffer.clear();
            break;
        }

        if (static_cast<quint8>(m_receiveBuffer[startIndex]) != 0x68)
        {
            LOG_WARNING(QString("Invalid data in buffer, clearing. First byte: 0x%1")
                            .arg(static_cast<quint8>(m_receiveBuffer[0]), 2, 16, QChar('0')));
            m_receiveBuffer.clear();
            break;
        }

        const QByteArray frameData = m_receiveBuffer.mid(startIndex);
        const ProtocolFrame frame = m_protocolParser->parseFrame(frameData);

        if (!frame.isValid())
        {
            const int minFrameSize = 12;
            if (m_receiveBuffer.size() - startIndex < minFrameSize)
            {
                break;
            }

            LOG_WARNING(QString("Failed to parse frame with sufficient data, clearing buffer"));
            m_receiveBuffer.clear();
            break;
        }

        const int actualFrameSize = startIndex + frame.parsedSize;
        const QByteArray rawData = m_receiveBuffer.left(actualFrameSize);
        emit rawDataReceived(rawData);

        m_receiveBuffer = m_receiveBuffer.mid(actualFrameSize);
        emit frameReceived(frame);

        const quint8 controlCode = frame.controlCode & 0x7F;
        const bool needsAddressValidation =
            controlCode == static_cast<quint8>(ControlCode::ReadData) ||
            controlCode == static_cast<quint8>(ControlCode::WriteData) ||
            controlCode == static_cast<quint8>(ControlCode::ReadAddress) ||
            controlCode == static_cast<quint8>(ControlCode::WriteAddress);

        if (needsAddressValidation && !shouldRespondToAddress(frame.address))
        {
            continue;
        }

        MeterModel *requestMeterModel = meterModelForAddress(frame.address);
        if (needsAddressValidation && !requestMeterModel)
        {
            const QString requestAddress = ProtocolParser::bytesToAddress(frame.address);
            LOG_WARNING(QString("No meter model found for address, ignoring request: %1").arg(requestAddress));
            continue;
        }

        if (controlCode == static_cast<quint8>(ControlCode::ReadData))
        {
            processReadRequest(frame, requestMeterModel);
        }
        else if (controlCode == static_cast<quint8>(ControlCode::WriteData))
        {
            processWriteRequest(frame, requestMeterModel);
        }
        else if (controlCode == static_cast<quint8>(ControlCode::ReadAddress))
        {
            m_protocolParser->setMeterModel(requestMeterModel);
            QByteArray addressBytes = ProtocolParser::addressToBytes(requestMeterModel->getMeterAddress());
            ProtocolFrame response = m_protocolParser->buildReadResponse(frame, addressBytes);
            sendResponse(response, requestMeterModel);
        }
        else if (controlCode == static_cast<quint8>(ControlCode::WriteAddress))
        {
            const QString newAddress = ProtocolParser::bytesToAddress(frame.dataField.mid(5, 6));
            if (ProtocolParser::isValidAddress(newAddress))
            {
                requestMeterModel->setMeterAddress(newAddress);
                requestMeterModel->setDataValue("0x04000000", newAddress);
                ProtocolFrame response = m_protocolParser->buildWriteResponse(frame, true);
                sendResponse(response, requestMeterModel);
            }
            else
            {
                ProtocolFrame response = m_protocolParser->buildErrorResponse(frame, 0x03);
                sendResponse(response, requestMeterModel);
            }
        }
    }
}

void ProtocolController::sendReadRequest(const QString &diCode)
{
    if (!hasActiveTransport())
    {
        LOG_WARNING("Cannot send read request: no transport connection is open");
        return;
    }
    if (!m_meterModel)
    {
        LOG_WARNING("Cannot send read request: no current meter selected");
        return;
    }

    QByteArray address = ProtocolParser::addressToBytes(m_meterModel->getMeterAddress());
    ProtocolFrame frame = m_protocolParser->buildReadRequest(address, diCode, m_meterModel->getIncludePrefixFE());

    QByteArray frameData = frame.toByteArray();
    if (m_meterModel->getIncludePrefixFE())
    {
        frameData.prepend(0xFE);
        frameData.prepend(0xFE);
        frameData.prepend(0xFE);
        frameData.prepend(0xFE);
    }

    if (sendTransportData(frameData))
    {
        emit frameSent(frame);
        LOG_INFO(QString("Sent read request for DI: %1").arg(diCode));
    }
}

void ProtocolController::sendWriteRequest(const QString &diCode, const QVariant &value)
{
    if (!hasActiveTransport())
    {
        LOG_WARNING("Cannot send write request: no transport connection is open");
        return;
    }
    if (!m_meterModel)
    {
        LOG_WARNING("Cannot send write request: no current meter selected");
        return;
    }

    QByteArray address = ProtocolParser::addressToBytes(m_meterModel->getMeterAddress());
    ProtocolFrame frame = m_protocolParser->buildWriteRequest(address, diCode, value, m_meterModel->getIncludePrefixFE());

    QByteArray frameData = frame.toByteArray();
    if (m_meterModel->getIncludePrefixFE())
    {
        frameData.prepend(0xFE);
        frameData.prepend(0xFE);
        frameData.prepend(0xFE);
        frameData.prepend(0xFE);
    }

    if (sendTransportData(frameData))
    {
        emit frameSent(frame);
        LOG_INFO(QString("Sent write request for DI: %1, value: %2").arg(diCode).arg(value.toString()));
    }
}

void ProtocolController::setAutoResponseEnabled(bool enabled)
{
    m_autoResponseEnabled = enabled;
}

void ProtocolController::setResponseDelay(int delay)
{
    m_responseDelay = delay;
}

void ProtocolController::setHasMeter(bool hasMeter)
{
    m_hasMeter = hasMeter;
}

void ProtocolController::setKnownMeterAddresses(const QStringList &addresses)
{
    m_knownMeterAddresses = addresses;
    m_knownMeterAddresses.removeAll(QString());
    m_knownMeterAddresses.removeDuplicates();
}

void ProtocolController::onSerialDataReceived(const QByteArray &data)
{
    LOG_DEBUG(QString("Serial data received: %1 bytes").arg(data.size()));
    processReceivedData(data);
}

void ProtocolController::onSerialDataSent(const QByteArray &data)
{
    LOG_DEBUG(QString("Serial data sent: %1 bytes").arg(data.size()));
    emit rawDataSent(data);
}

void ProtocolController::onSerialConnectionStateChanged(bool connected)
{
    LOG_INFO(QString("Serial connection state changed: %1").arg(connected ? "Connected" : "Disconnected"));
    if (!connected)
    {
        resetTransportState();
    }
}

void ProtocolController::onTcpDataReceived(const QByteArray &data)
{
    LOG_DEBUG(QString("TCP data received: %1 bytes").arg(data.size()));
    processReceivedData(data);
}

void ProtocolController::onTcpDataSent(const QByteArray &data)
{
    LOG_DEBUG(QString("TCP data sent: %1 bytes").arg(data.size()));
    emit rawDataSent(data);
}

void ProtocolController::onTcpConnectionStateChanged(bool connected)
{
    LOG_INFO(QString("TCP connection state changed: %1").arg(connected ? "Connected" : "Disconnected"));
    if (!connected)
    {
        resetTransportState();
    }
}

void ProtocolController::onProtocolFrameParsed(const ProtocolFrame &frame)
{
    LOG_DEBUG(QString("Protocol frame parsed: %1").arg(frame.toString()));
}

void ProtocolController::onProtocolParseError(const QString &errorMessage)
{
    LOG_ERROR(QString("Protocol parse error: %1").arg(errorMessage));
    emit protocolError(0xFF, errorMessage);
}

void ProtocolController::onResponseTimerTimeout()
{
    if (!m_lastRequestFrame.isValid() || !m_lastRequestMeterModel)
    {
        return;
    }

    const QString diCode = ProtocolParser::extractDIFromDataField(m_lastRequestFrame.dataField);
    qDebug() << "===diCode:" << diCode;
    m_protocolParser->setMeterModel(m_lastRequestMeterModel);

    if ((m_lastRequestFrame.controlCode & 0x7F) == static_cast<quint8>(ControlCode::ReadData))
    {
        QVariant value = m_lastRequestMeterModel->getDataValue(diCode);
        qDebug() << "===value:" << value;
        ProtocolFrame response = m_protocolParser->buildReadResponse(m_lastRequestFrame, value, diCode);
        sendResponse(response, m_lastRequestMeterModel);
        emit dataRead(diCode, value);
    }
    else if ((m_lastRequestFrame.controlCode & 0x7F) == static_cast<quint8>(ControlCode::WriteData))
    {
        ProtocolFrame response = m_protocolParser->buildWriteResponse(m_lastRequestFrame, true);
        sendResponse(response, m_lastRequestMeterModel);
        emit dataWritten(diCode, QVariant(), true);
    }
}

void ProtocolController::processReadRequest(const ProtocolFrame &frame, MeterModel *meterModel)
{
    m_lastRequestFrame = frame;
    m_lastRequestMeterModel = meterModel;

    if (m_autoResponseEnabled)
    {
        m_responseTimer->start(m_responseDelay);
    }
}

void ProtocolController::processWriteRequest(const ProtocolFrame &frame, MeterModel *meterModel)
{
    LOG_INFO(QString("Processing write request: %1").arg(frame.toString()));

    QString diCode = ProtocolParser::extractDIFromDataField(frame.dataField);
    if (diCode == "0x04000000" || diCode == "0x04000010" || diCode == "0x04000011")
    {
        verifyPassword(frame, meterModel);
        return;
    }

    QByteArray valueBytes = frame.dataField.mid(5);
    QVariant value;

    std::reverse(valueBytes.begin(), valueBytes.end());
    for (int i = 0; i < valueBytes.size(); ++i)
    {
        quint8 byte = static_cast<quint8>(valueBytes[i]);
        byte -= 0x33;
        valueBytes[i] = byte;
    }

    if (diCode == "0x04000000")
    {
        value = ProtocolParser::bytesToAddress(valueBytes);
    }
    else if (diCode == "0x04000001")
    {
        if (valueBytes.size() >= 1)
        {
            value = static_cast<int>(static_cast<quint8>(valueBytes[0]));
        }
    }
    else if (valueBytes.size() >= 4)
    {
        value = BCDHelper::bcdToDecimal(valueBytes, 2);
    }

    if (meterModel && meterModel->setDataValue(diCode, value))
    {
        if (diCode == "0x04000000")
        {
            meterModel->setMeterAddress(value.toString());
        }

        LOG_INFO(QString("Data written: %1 = %2").arg(diCode).arg(value.toString()));

        m_lastRequestFrame = frame;
        m_lastRequestMeterModel = meterModel;
        if (m_autoResponseEnabled)
        {
            m_responseTimer->start(m_responseDelay);
        }
    }
    else
    {
        LOG_WARNING(QString("Failed to write data: %1").arg(diCode));
        ProtocolFrame response = m_protocolParser->buildErrorResponse(frame, 0x03);
        sendResponse(response, meterModel);
    }
}

void ProtocolController::sendResponse(const ProtocolFrame &response, MeterModel *meterModel)
{
    if (!hasActiveTransport())
    {
        LOG_WARNING("Cannot send response: no transport connection is open");
        return;
    }

    MeterModel *responseMeterModel = meterModel ? meterModel : m_meterModel;
    QByteArray responseData = response.toByteArray();
    if (responseMeterModel && responseMeterModel->getIncludePrefixFE())
    {
        responseData.prepend(0xFE);
        responseData.prepend(0xFE);
        responseData.prepend(0xFE);
        responseData.prepend(0xFE);
    }

    if (sendTransportData(responseData))
    {
        emit frameSent(response);
    }
}

void ProtocolController::verifyPassword(const ProtocolFrame &frame, MeterModel *meterModel)
{
    ProtocolFrame response = m_protocolParser->buildWriteResponse(frame, true);
    sendResponse(response, meterModel);
}

bool ProtocolController::shouldRespondToAddress(const QByteArray &addressBytes) const
{
    if (!m_hasMeter || m_knownMeterAddresses.isEmpty())
    {
        LOG_WARNING("No created meter address, ignoring request");
        return false;
    }

    const QString requestAddress = ProtocolParser::bytesToAddress(addressBytes);
    if (!m_knownMeterAddresses.contains(requestAddress))
    {
        LOG_WARNING(QString("Unknown meter address, ignoring request: %1").arg(requestAddress));
        return false;
    }

    return true;
}

MeterModel *ProtocolController::meterModelForAddress(const QByteArray &addressBytes) const
{
    const QString requestAddress = ProtocolParser::bytesToAddress(addressBytes);
    qDebug() << "===requestAddress:" << requestAddress;
    if (m_meterModelsByAddress.contains(requestAddress))
    {
        return m_meterModelsByAddress.value(requestAddress);
    }

    if (m_meterModel && m_meterModel->getMeterAddress() == requestAddress)
    {
        return m_meterModel;
    }

    return nullptr;
}

bool ProtocolController::hasActiveTransport() const
{
    return (m_serialController && m_serialController->isOpen()) ||
           (m_tcpController && m_tcpController->isOpen());
}

bool ProtocolController::sendTransportData(const QByteArray &data)
{
    if (m_serialController && m_serialController->isOpen())
    {
        return m_serialController->sendData(data);
    }

    if (m_tcpController && m_tcpController->isOpen())
    {
        return m_tcpController->sendData(data);
    }

    return false;
}

void ProtocolController::resetTransportState()
{
    m_receiveBuffer.clear();
    m_responseTimer->stop();
    m_lastRequestMeterModel = nullptr;
    m_lastRequestFrame = ProtocolFrame();
}
