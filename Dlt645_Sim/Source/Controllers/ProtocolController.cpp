#include "ProtocolController.h"
#include "Utils/Logger.h"
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
      m_responseTimer(new QTimer(this))
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

    // 移除队列中属于该电表的待响应请求
    QQueue<PendingResponse> remaining;
    for (const PendingResponse &pending : m_pendingResponses)
    {
        if (pending.meterModel != removedModel)
        {
            remaining.enqueue(pending);
        }
    }
    m_pendingResponses = remaining;
    if (m_pendingResponses.isEmpty())
    {
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
    emit rawDataReceived(data);
    if (data.isEmpty())
    {
        return;
    }

    m_receiveBuffer.append(data);


    ProtocolFrame frame;
    QByteArray rawFrame;
    while (tryTakeNextReceivedFrame(frame, rawFrame))
    {
        emit frameReceived(frame, rawFrame);

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
                requestMeterModel->setDataValue("0x04000401", newAddress);
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

bool ProtocolController::tryTakeNextReceivedFrame(ProtocolFrame &frame, QByteArray &rawFrame)
{
    auto trimBufferForFrame = [this]() -> bool
    {
        while (!m_receiveBuffer.isEmpty())
        {
            const int first68Index = m_receiveBuffer.indexOf(static_cast<char>(0x68));
            if (first68Index < 0)
            {
                int trailingFECount = 0;
                for (int i = m_receiveBuffer.size() - 1; i >= 0; --i)
                {
                    if (static_cast<quint8>(m_receiveBuffer[i]) != 0xFE)
                    {
                        break;
                    }
                    ++trailingFECount;
                }

                if (trailingFECount == m_receiveBuffer.size())
                {
                    if (m_receiveBuffer.size() > 4)
                    {
                        m_receiveBuffer = m_receiveBuffer.right(4);
                    }
                }
                else if (trailingFECount > 0)
                {
                    const int invalidCount = m_receiveBuffer.size() - trailingFECount;
                    LOG_WARNING(QString("Discarding %1 invalid byte(s) before DLT645 prefix bytes").arg(invalidCount));
                    m_receiveBuffer = m_receiveBuffer.right(qMin(trailingFECount, 4));
                }
                else
                {
                    LOG_WARNING(QString("No DLT645 start flag found, clearing %1 invalid byte(s)").arg(m_receiveBuffer.size()));
                    m_receiveBuffer.clear();
                }
                return false;
            }

            int framePrefixIndex = first68Index;
            while (framePrefixIndex > 0 &&
                   static_cast<quint8>(m_receiveBuffer[framePrefixIndex - 1]) == 0xFE)
            {
                --framePrefixIndex;
            }

            if (framePrefixIndex > 0)
            {
                LOG_WARNING(QString("Discarding %1 invalid leading byte(s) before DLT645 frame").arg(framePrefixIndex));
                m_receiveBuffer.remove(0, framePrefixIndex);
                continue;
            }

            return true;
        }

        return false;
    };

    while (trimBufferForFrame())
    {
        int startIndex = 0;
        while (startIndex < m_receiveBuffer.size() &&
               static_cast<quint8>(m_receiveBuffer[startIndex]) == 0xFE)
        {
            ++startIndex;
        }

        if (startIndex >= m_receiveBuffer.size())
        {
            return false;
        }

        constexpr int minimumHeaderSize = 10; // 68 + 地址6 + 68 + 控制码 + 长度
        if (m_receiveBuffer.size() < startIndex + minimumHeaderSize)
        {
            return false;
        }

        if (static_cast<quint8>(m_receiveBuffer[startIndex + 7]) != 0x68)
        {
            LOG_WARNING(QString("Invalid DLT645 second start flag, resynchronizing buffer"));
            m_receiveBuffer.remove(0, startIndex + 1);
            continue;
        }

        const int dataLength = static_cast<quint8>(m_receiveBuffer[startIndex + 9]);
        const int expectedFrameSize = startIndex + 12 + dataLength;
        if (m_receiveBuffer.size() < expectedFrameSize)
        {
            return false;
        }

        rawFrame = m_receiveBuffer.left(expectedFrameSize);
        frame = m_protocolParser->parseFrame(rawFrame);
        if (!frame.isValid() || !frame.checksumValid)
        {
            LOG_WARNING(QString("Failed to parse complete DLT645 frame (invalid structure or checksum), resynchronizing buffer"));
            m_receiveBuffer.remove(0, startIndex + 1);
            continue;
        }

        m_receiveBuffer.remove(0, expectedFrameSize);
        return true;
    }

    return false;
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
    const QByteArray frameData = ProtocolParser::frameToRawBytes(frame, m_meterModel->getIncludePrefixFE());

    if (sendTransportData(frameData))
    {
        emit frameSent(frame, frameData);
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
    const QByteArray frameData = ProtocolParser::frameToRawBytes(frame, m_meterModel->getIncludePrefixFE());

    if (sendTransportData(frameData))
    {
        emit frameSent(frame, frameData);
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
    // LOG_DEBUG(QString("Serial data received: %1 bytes").arg(data.size()));
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
    if (m_pendingResponses.isEmpty())
    {
        return;
    }

    const PendingResponse pending = m_pendingResponses.dequeue();
    const ProtocolFrame &requestFrame = pending.frame;
    MeterModel *meterModel = pending.meterModel;

    if (meterModel)
    {
        const QString diCode = ProtocolParser::extractDIFromDataField(requestFrame.dataField);
        m_protocolParser->setMeterModel(meterModel);

        if ((requestFrame.controlCode & 0x7F) == static_cast<quint8>(ControlCode::ReadData))
        {
            if (meterModel->isDataItemSupported(diCode))
            {
                QVariant value = meterModel->getDataValue(diCode);
                ProtocolFrame response = m_protocolParser->buildReadResponse(requestFrame, value, diCode);
                sendResponse(response, meterModel);
                emit dataRead(diCode, value);
            }
            else
            {
                ProtocolFrame response = m_protocolParser->buildErrorResponse(requestFrame, 0x01);
                sendResponse(response, meterModel);
                emit protocolError(0x01, QStringLiteral("当前电表不支持数据项: %1").arg(diCode));
            }
        }
        else if ((requestFrame.controlCode & 0x7F) == static_cast<quint8>(ControlCode::WriteData))
        {
            ProtocolFrame response = m_protocolParser->buildWriteResponse(requestFrame, true);
            sendResponse(response, meterModel);
            emit dataWritten(diCode, QVariant(), true);
        }
    }

    // 队列中仍有待处理请求时，继续调度下一个响应
    scheduleNextResponse();
}

void ProtocolController::processReadRequest(const ProtocolFrame &frame, MeterModel *meterModel)
{
    if (!m_autoResponseEnabled)
    {
        return;
    }

    m_pendingResponses.enqueue({frame, meterModel});
    scheduleNextResponse();
}

void ProtocolController::processWriteRequest(const ProtocolFrame &frame, MeterModel *meterModel)
{
    LOG_INFO(QString("Processing write request: %1").arg(frame.toString()));

    QString diCode = ProtocolParser::extractDIFromDataField(frame.dataField);
    if (diCode == "0x04000401" || diCode == "0x04000010" || diCode == "0x04000011")
    {
        verifyPassword(frame, meterModel);
        return;
    }

    const bool hasLegacyPayloadLength =
        frame.dataField.size() > 5 && static_cast<quint8>(frame.dataField[4]) == frame.dataField.size() - 5;
    const int valueOffset = hasLegacyPayloadLength ? 5 : 12;
    QByteArray valueBytes = frame.dataField.mid(valueOffset);
    QVariant value;

    std::reverse(valueBytes.begin(), valueBytes.end());
    for (int i = 0; i < valueBytes.size(); ++i)
    {
        quint8 byte = static_cast<quint8>(valueBytes[i]);
        byte -= 0x33;
        valueBytes[i] = byte;
    }

    if (diCode == "0x04000401")
    {
        value = ProtocolParser::bytesToAddress(valueBytes);
    }
    else if (diCode == "0x04000701" ||
             diCode == "0x04000702" ||
             diCode == "0x04000703" ||
             diCode == "0x04000704" ||
             diCode == "0x04000705")
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
        if (diCode == "0x04000401")
        {
            meterModel->setMeterAddress(value.toString());
        }

        LOG_INFO(QString("Data written: %1 = %2").arg(diCode).arg(value.toString()));

        if (m_autoResponseEnabled)
        {
            m_pendingResponses.enqueue({frame, meterModel});
            scheduleNextResponse();
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
    const QByteArray responseData =
        ProtocolParser::frameToRawBytes(response,
                                        responseMeterModel && responseMeterModel->getIncludePrefixFE());

    if (sendTransportData(responseData))
    {
        emit frameSent(response, responseData);
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
    m_pendingResponses.clear();
}

void ProtocolController::scheduleNextResponse()
{
    if (!m_autoResponseEnabled || m_pendingResponses.isEmpty())
    {
        return;
    }

    if (!m_responseTimer->isActive())
    {
        m_responseTimer->start(m_responseDelay);
    }
}
