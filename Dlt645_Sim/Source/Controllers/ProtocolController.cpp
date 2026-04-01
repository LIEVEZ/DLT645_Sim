#include "ProtocolController.h"
#include "Utils/Logger.h"
#include <QDebug>
#include <algorithm>

ProtocolController::ProtocolController(QObject *parent)
    : QObject(parent), m_meterModel(new MeterModel(this)), m_serialController(nullptr), m_protocolParser(new ProtocolParser(m_meterModel, this)), m_autoResponseEnabled(true), m_responseDelay(100), m_hasMeter(false), m_responseTimer(new QTimer(this))
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

void ProtocolController::processReceivedData(const QByteArray &data)
{
    m_receiveBuffer.append(data);

    qDebug() << "processReceivedData data:" << data.toHex();
    // 尝试解析帧，循环处理缓冲区中的所有完整帧
    while (!m_receiveBuffer.isEmpty())
    {
        // 查找第一个有效的起始符位置（跳过前导码0xFE）
        int startIndex = 0;
        while (startIndex < m_receiveBuffer.size() &&
               static_cast<quint8>(m_receiveBuffer[startIndex]) == 0xFE)
        {
            startIndex++;
        }

        // 如果缓冲区中没有有效的起始符，清除缓冲区
        if (startIndex >= m_receiveBuffer.size())
        {
            m_receiveBuffer.clear();
            break;
        }

        // 如果起始符不是0x68，说明数据无效，清除缓冲区
        if (static_cast<quint8>(m_receiveBuffer[startIndex]) != 0x68)
        {
            LOG_WARNING(QString("Invalid data in buffer, clearing. First byte: 0x%1")
                            .arg(static_cast<quint8>(m_receiveBuffer[0]), 2, 16, QChar('0')));
            m_receiveBuffer.clear();
            break;
        }

        // 尝试解析从起始符开始的数据
        QByteArray frameData = m_receiveBuffer.mid(startIndex);
        ProtocolFrame frame = m_protocolParser->parseFrame(frameData);

        if (frame.isValid())
        {
            // 计算实际接收的帧大小（包括前导码）
            // frame.parsedSize 是实际解析的字节数（不包括前导码）
            int actualFrameSize = startIndex + frame.parsedSize;

            // 发送原始数据信号（只包含当前帧，包括前导码）
            // 从缓冲区中提取当前帧的完整数据（从0开始，包括前导码）
            QByteArray rawData = m_receiveBuffer.left(actualFrameSize);
            emit rawDataReceived(rawData);

            // 清除已解析的数据
            m_receiveBuffer = m_receiveBuffer.mid(actualFrameSize);

            emit frameReceived(frame);

            // 处理请求帧
            quint8 controlCode = frame.controlCode & 0x7F; // 清除最高位
            if (controlCode == static_cast<quint8>(ControlCode::ReadData))
            {
                processReadRequest(frame);
            }
            else if (controlCode == static_cast<quint8>(ControlCode::WriteData))
            {
                processWriteRequest(frame);
            }
            else if (controlCode == static_cast<quint8>(ControlCode::ReadAddress))
            {
                // 读地址请求
                QByteArray addressBytes = ProtocolParser::addressToBytes(m_meterModel->getMeterAddress());
                ProtocolFrame response = m_protocolParser->buildReadResponse(frame, addressBytes);
                sendResponse(response);
            }
            else if (controlCode == static_cast<quint8>(ControlCode::WriteAddress))
            {
                // 写地址请求
                QString newAddress = ProtocolParser::bytesToAddress(frame.dataField.mid(5, 6));
                if (ProtocolParser::isValidAddress(newAddress))
                {
                    m_meterModel->setMeterAddress(newAddress);
                    ProtocolFrame response = m_protocolParser->buildWriteResponse(frame, true);
                    sendResponse(response);
                }
                else
                {
                    ProtocolFrame response = m_protocolParser->buildErrorResponse(frame, 0x03); // 数据无效
                    sendResponse(response);
                }
            }
        }
        else
        {
            // 解析失败，检查是否数据不完整
            // DLT645-2007最小帧大小：起始符(1) + 地址(6) + 起始符2(1) + 控制码(1) + 长度(1) + 校验和(1) + 结束符(1) = 12字节
            int minFrameSize = 12;

            if (m_receiveBuffer.size() - startIndex < minFrameSize)
            {
                // 数据不完整，等待更多数据
                break;
            }
            else
            {
                // 数据足够但解析失败，可能是损坏的数据，清除缓冲区
                LOG_WARNING(QString("Failed to parse frame with sufficient data, clearing buffer"));
                m_receiveBuffer.clear();
                break;
            }
        }
    }
}

void ProtocolController::sendReadRequest(const QString &diCode)
{
    if (!m_serialController || !m_serialController->isOpen())
    {
        LOG_WARNING("Cannot send read request: serial port not open");
        return;
    }

    QByteArray address = ProtocolParser::addressToBytes(m_meterModel->getMeterAddress());
    ProtocolFrame frame = m_protocolParser->buildReadRequest(address, diCode, m_meterModel->getIncludePrefixFE());

    QByteArray frameData = frame.toByteArray();
    if (m_meterModel->getIncludePrefixFE())
    {
        // DLT645-2007协议要求四个0xFE前导码
        frameData.prepend(0xFE);
        frameData.prepend(0xFE);
        frameData.prepend(0xFE);
        frameData.prepend(0xFE);
    }
    if (m_serialController->sendData(frameData))
    {
        emit frameSent(frame);
        LOG_INFO(QString("Sent read request for DI: %1").arg(diCode));
    }
}

void ProtocolController::sendWriteRequest(const QString &diCode, const QVariant &value)
{
    if (!m_serialController || !m_serialController->isOpen())
    {
        LOG_WARNING("Cannot send write request: serial port not open");
        return;
    }

    QByteArray address = ProtocolParser::addressToBytes(m_meterModel->getMeterAddress());
    ProtocolFrame frame = m_protocolParser->buildWriteRequest(address, diCode, value, m_meterModel->getIncludePrefixFE());

    QByteArray frameData = frame.toByteArray();
    if (m_meterModel->getIncludePrefixFE())
    {
        // DLT645-2007协议要求四个0xFE前导码
        frameData.prepend(0xFE);
        frameData.prepend(0xFE);
        frameData.prepend(0xFE);
        frameData.prepend(0xFE);
    }

    if (m_serialController->sendData(frameData))
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

void ProtocolController::onSerialDataReceived(const QByteArray &data)
{
    LOG_DEBUG(QString("Serial data received: %1 bytes").arg(data.size()));
    processReceivedData(data);
}

void ProtocolController::onSerialDataSent(const QByteArray &data)
{
    LOG_DEBUG(QString("Serial data sent: %1 bytes").arg(data.size()));

    // 发送原始数据信号（包含前导码）
    emit rawDataSent(data);
}

void ProtocolController::onSerialConnectionStateChanged(bool connected)
{
    LOG_INFO(QString("Serial connection state changed: %1").arg(connected ? "Connected" : "Disconnected"));
    if (!connected)
    {
        m_receiveBuffer.clear();
        m_responseTimer->stop();
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
    // 发送应答
    if (m_lastRequestFrame.isValid())
    {
        QString diCode = ProtocolParser::extractDIFromDataField(m_lastRequestFrame.dataField);
        qDebug() << "===diCode:" << diCode;
        if ((m_lastRequestFrame.controlCode & 0x7F) == static_cast<quint8>(ControlCode::ReadData))
        {
            // 读数据应答
            QVariant value = m_meterModel->getDataValue(diCode);
            qDebug() << "===value:" << value;
            ProtocolFrame response = m_protocolParser->buildReadResponse(m_lastRequestFrame, value, diCode);
            sendResponse(response);
            emit dataRead(diCode, value);
        }
        else if ((m_lastRequestFrame.controlCode & 0x7F) == static_cast<quint8>(ControlCode::WriteData))
        {
            // 写数据应答
            ProtocolFrame response = m_protocolParser->buildWriteResponse(m_lastRequestFrame, true);
            sendResponse(response);
            emit dataWritten(diCode, QVariant(), true);
        }
    }
}

void ProtocolController::processReadRequest(const ProtocolFrame &frame)
{
    // LOG_INFO(QString("Processing read request: %1").arg(frame.toString()));

    // 检查是否有电表节点
    if (!m_hasMeter)
    {
        LOG_WARNING("No meter node, ignoring read request");
        return;
    }

    // 验证地址
    QString requestAddress = ProtocolParser::bytesToAddress(frame.address);
    QString meterAddress = m_meterModel->getMeterAddress();

    if (requestAddress != meterAddress)
    {
        LOG_WARNING(QString("Address mismatch: request=%1, meter=%2").arg(requestAddress).arg(meterAddress));
        ProtocolFrame response = m_protocolParser->buildErrorResponse(frame, 0x09); // 电表异常
        sendResponse(response);
        return;
    }

    // 保存请求帧
    m_lastRequestFrame = frame;

    // 延迟应答
    if (m_autoResponseEnabled)
    {
        m_responseTimer->start(m_responseDelay);
    }
}

void ProtocolController::processWriteRequest(const ProtocolFrame &frame)
{
    LOG_INFO(QString("Processing write request: %1").arg(frame.toString()));

    // 检查是否有电表节点
    if (!m_hasMeter)
    {
        LOG_WARNING("No meter node, ignoring write request");
        return;
    }

    // 验证地址
    QString requestAddress = ProtocolParser::bytesToAddress(frame.address);
    QString meterAddress = m_meterModel->getMeterAddress();

    if (requestAddress != meterAddress)
    {
        LOG_WARNING(QString("Address mismatch: request=%1, meter=%2").arg(requestAddress).arg(meterAddress));
        ProtocolFrame response = m_protocolParser->buildErrorResponse(frame, 0x09); // 电表异常
        sendResponse(response);
        return;
    }

    // 提取数据标识和值
    QString diCode = ProtocolParser::extractDIFromDataField(frame.dataField);

    // 检查是否需要密码验证
    if (diCode == "0x04000000" || diCode == "0x04000010" || diCode == "0x04000011")
    {
        // 需要密码验证
        verifyPassword(frame);
        return;
    }

    // 提取数据值
    QByteArray valueBytes = frame.dataField.mid(5); // 跳过DI码和长度
    QVariant value;

    // DLT645协议：数据域需要倒序处理
    std::reverse(valueBytes.begin(), valueBytes.end());

        for (int i = 0; i < valueBytes.size(); ++i)
    {
        quint8 byte = static_cast<quint8>(valueBytes[i]);
        byte -= 0x33;
        valueBytes[i] = byte;
    }

    // 根据DI码解析值
    if (diCode == "0x04000000")
    {
        // 电表地址
        value = ProtocolParser::bytesToAddress(valueBytes);
    }
    else if (diCode == "0x04000001")
    {
        // 波特率
        if (valueBytes.size() >= 1)
        {
            int baudRate = valueBytes[0];
            value = baudRate;
        }
    }
    else
    {
        // 数值类型
        if (valueBytes.size() >= 4)
        {
            double numValue = BCDHelper::bcdToDecimal(valueBytes, 2);
            value = numValue;
        }
    }

    // 更新数据
    if (m_meterModel->setDataValue(diCode, value))
    {
        LOG_INFO(QString("Data written: %1 = %2").arg(diCode).arg(value.toString()));

        // 保存请求帧
        m_lastRequestFrame = frame;

        // 延迟应答
        if (m_autoResponseEnabled)
        {
            m_responseTimer->start(m_responseDelay);
        }
    }
    else
    {
        LOG_WARNING(QString("Failed to write data: %1").arg(diCode));
        ProtocolFrame response = m_protocolParser->buildErrorResponse(frame, 0x03); // 数据无效
        sendResponse(response);
    }
}

void ProtocolController::sendResponse(const ProtocolFrame &response)
{
    if (!m_serialController || !m_serialController->isOpen())
    {
        LOG_WARNING("Cannot send response: serial port not open");
        return;
    }

    QByteArray responseData = response.toByteArray();
    if (m_meterModel->getIncludePrefixFE())
    {
        // DLT645-2007协议要求四个0xFE前导码
        responseData.prepend(0xFE);
        responseData.prepend(0xFE);
        responseData.prepend(0xFE);
        responseData.prepend(0xFE);
    }

    if (m_serialController->sendData(responseData))
    {
        emit frameSent(response);
        // LOG_INFO(QString("Sent response: %1").arg(response.toString()));
    }
}

void ProtocolController::verifyPassword(const ProtocolFrame &frame)
{
    // 简化处理：假设密码总是正确
    // 实际应用中应该验证密码
    ProtocolFrame response = m_protocolParser->buildWriteResponse(frame, true);
    sendResponse(response);
}
