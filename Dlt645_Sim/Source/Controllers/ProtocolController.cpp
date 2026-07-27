#include "ProtocolController.h"
#include "Models/DataItem.h"
#include "Models/StatusWordBitCatalog.h"
#include "Utils/DLT645DataHelper.h"
#include "Utils/Logger.h"
#include "Utils/ChecksumHelper.h"
#include <algorithm>

namespace
{

QString statusWordDiCode(int index)
{
    return QStringLiteral("0x040005%1")
        .arg(index, 2, 16, QChar('0'))
        .toUpper()
        .replace(QStringLiteral("0X"), QStringLiteral("0x"));
}

bool isKnownRemoteControlCommand(quint8 command)
{
    switch (command)
    {
    case 0x1A: // 跳闸
    case 0x1B: // 合闸
    case 0x1C: // 直接合闸
    case 0x2A: // 报警
    case 0x2B: // 报警解除
    case 0x3A: // 保电
    case 0x3B: // 保电解除
        return true;
    default:
        return false;
    }
}

QString remoteControlCommandLabel(quint8 command)
{
    switch (command)
    {
    case 0x1A:
        return QStringLiteral("跳闸");
    case 0x1B:
        return QStringLiteral("合闸");
    case 0x1C:
        return QStringLiteral("直接合闸");
    case 0x2A:
        return QStringLiteral("报警");
    case 0x2B:
        return QStringLiteral("报警解除");
    case 0x3A:
        return QStringLiteral("保电");
    case 0x3B:
        return QStringLiteral("保电解除");
    default:
        return QStringLiteral("未知命令(0x%1)").arg(command, 2, 16, QChar('0')).toUpper();
    }
}

void syncStatusWordBlock(MeterModel *meterModel)
{
    if (!meterModel)
    {
        return;
    }

    QVariantList block;
    for (int i = 1; i <= 7; ++i)
    {
        block.append(meterModel->getDataValue(statusWordDiCode(i)));
    }
    meterModel->setDataValue(QStringLiteral("0x040005FF"), block);
}

bool applyRemoteControlToStatusWord3(MeterModel *meterModel, quint8 command)
{
    if (!meterModel)
    {
        return false;
    }

    const QString statusWord3Code = statusWordDiCode(3);
    quint16 word = static_cast<quint16>(meterModel->getDataValue(statusWord3Code).toUInt());

    StatusWordBitField singleBit;
    singleBit.bitWidth = 1;

    auto setBit = [&](int offset, int value) {
        singleBit.bitOffset = offset;
        word = StatusWordBitCatalog::applyFieldValue(word, singleBit, value);
    };

    switch (command)
    {
    case 0x1A: // 跳闸：继电器状态/命令状态 -> 断
        setBit(4, 1);
        setBit(6, 1);
        break;
    case 0x1B: // 合闸：继电器状态/命令状态 -> 通
    case 0x1C: // 直接合闸：继电器状态/命令状态 -> 通
        setBit(4, 0);
        setBit(6, 0);
        break;
    case 0x2A: // 报警
        setBit(7, 1);
        break;
    case 0x2B: // 报警解除
        setBit(7, 0);
        break;
    case 0x3A: // 保电：无独立状态位，先保持现状并应答成功
    case 0x3B: // 保电解除
        break;
    default:
        return false;
    }

    if (!meterModel->setDataValue(statusWord3Code, word))
    {
        return false;
    }
    syncStatusWordBlock(meterModel);
    return true;
}


bool isBroadcastAddress(const QByteArray &addressBytes)
{
    if (addressBytes.size() != 6)
    {
        return false;
    }

    for (int i = 0; i < addressBytes.size(); ++i)
    {
        if (static_cast<quint8>(addressBytes.at(i)) != 0xAA)
        {
            return false;
        }
    }

    return true;
}

void finalizeResponseAddress(ProtocolFrame &response, MeterModel *meterModel)
{
    if (!meterModel)
    {
        return;
    }

    const QByteArray meterAddressBytes = ProtocolParser::addressToBytes(meterModel->getMeterAddress());
    if (meterAddressBytes.size() != 6 || response.address == meterAddressBytes)
    {
        return;
    }

    response.address = meterAddressBytes;
    response.checksum = 0;
    const QByteArray frameData = response.toByteArray();
    response.checksum = ChecksumHelper::calculateChecksum(frameData.left(frameData.size() - 2));
}
} // namespace

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
            controlCode == static_cast<quint8>(ControlCode::ReadAuthStatus) ||
            controlCode == static_cast<quint8>(ControlCode::WriteAuthStatus) ||
            controlCode == static_cast<quint8>(ControlCode::ReadAddress) ||
            controlCode == static_cast<quint8>(ControlCode::WriteAddress) ||
            controlCode == static_cast<quint8>(ControlCode::RemoteControl);

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

        if (controlCode == static_cast<quint8>(ControlCode::ReadData) ||
            controlCode == static_cast<quint8>(ControlCode::ReadAuthStatus))
        {
            processReadRequest(frame, requestMeterModel);
        }
        else if (controlCode == static_cast<quint8>(ControlCode::WriteData))
        {
            processWriteRequest(frame, requestMeterModel);
        }
        else if (controlCode == static_cast<quint8>(ControlCode::WriteAuthStatus))
        {
            processWriteAuthStatusRequest(frame, requestMeterModel);
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
            // 0x15 数据域即 6 字节新地址（BCD 低位在前，传输时整体 +0x33）
            const QString newAddress = ProtocolParser::bytesToAddress(
                DLT645DataHelper::subtract33(frame.dataField.left(6)));
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
        else if (controlCode == static_cast<quint8>(ControlCode::RemoteControl))
        {
            processRemoteControlRequest(frame, requestMeterModel);
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

        const quint8 controlCode = static_cast<quint8>(m_receiveBuffer[startIndex + 8]);
        const quint8 dataLengthField = static_cast<quint8>(m_receiveBuffer[startIndex + 9]);
        const int actualDataLength = ProtocolParser::effectiveDataFieldLength(controlCode, dataLengthField);
        const int expectedFrameSize = startIndex + 12 + actualDataLength;
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

        if ((requestFrame.controlCode & 0x7F) == static_cast<quint8>(ControlCode::ReadData) ||
            (requestFrame.controlCode & 0x7F) == static_cast<quint8>(ControlCode::ReadAuthStatus))
        {
            if (diCode.startsWith(QStringLiteral("0x03")) && !meterModel->getEventRecordPresent())
            {
                ProtocolFrame response = m_protocolParser->buildErrorResponse(requestFrame, 0x02);
                sendResponse(response, meterModel);
                emit protocolError(0x02, QStringLiteral("当前电表事件记录不存在: %1").arg(diCode));
            }
            else if (diCode.startsWith(QStringLiteral("0x03")) && !meterModel->isDataItemSupported(diCode))
            {
                ProtocolFrame response = m_protocolParser->buildErrorResponse(requestFrame, 0x02);
                sendResponse(response, meterModel);
                emit protocolError(0x02, QStringLiteral("当前电表未启用该事件记录: %1").arg(diCode));
            }
            else if (meterModel->isDataItemSupported(diCode))
            {
                QVariant value = meterModel->getDataValue(diCode);
                ProtocolFrame response = m_protocolParser->buildReadResponse(requestFrame, value, diCode);
                sendResponse(response, meterModel);
                emit dataRead(diCode, value);
            }
            else
            {
                // 数据项不支持：返回错误码 0x02（无请求数据）
                // 适用场景：单相表查询 B/C 相数据、查询不存在的数据标识等
                ProtocolFrame response = m_protocolParser->buildErrorResponse(requestFrame, 0x02);
                sendResponse(response, meterModel);
                emit protocolError(0x02, QStringLiteral("当前电表不支持数据项: %1").arg(diCode));
            }
        }
        else if ((requestFrame.controlCode & 0x7F) == static_cast<quint8>(ControlCode::WriteData) ||
                 (requestFrame.controlCode & 0x7F) == static_cast<quint8>(ControlCode::WriteAuthStatus) ||
                 (requestFrame.controlCode & 0x7F) == static_cast<quint8>(ControlCode::RemoteControl))
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

void ProtocolController::processWriteAuthStatusRequest(const ProtocolFrame &frame, MeterModel *meterModel)
{
    LOG_INFO(QString("Processing auth-status write request: %1").arg(frame.toString()));

    const QString diCode = ProtocolParser::extractDIFromDataField(frame.dataField);
    if (diCode != QStringLiteral("0x1C0104CD"))
    {
        LOG_WARNING(QString("Unsupported DI for auth-status write control 0x2F: %1").arg(diCode));
        ProtocolFrame response = m_protocolParser->buildErrorResponse(frame, 0x02);
        sendResponse(response, meterModel);
        emit protocolError(0x02, QStringLiteral("认证状态写控制码仅支持数据项 0x1C0104CD"));
        return;
    }

    // 厂商扩展写帧：DI(4) + 数据(1)，无权限/密码/操作者代码
    QByteArray valueBytes = frame.dataField.mid(4);
    for (int i = 0; i < valueBytes.size(); ++i)
    {
        valueBytes[i] = static_cast<char>(static_cast<quint8>(valueBytes[i]) - 0x33);
    }

    QVariant value = 0;
    if (!valueBytes.isEmpty())
    {
        value = static_cast<int>(static_cast<quint8>(valueBytes.at(0)));
    }

    if (meterModel && meterModel->setDataValue(diCode, value))
    {
        LOG_INFO(QString("Auth status written: %1 = 0x%2")
                     .arg(diCode)
                     .arg(value.toUInt(), 2, 16, QChar('0')));

        if (m_autoResponseEnabled)
        {
            m_pendingResponses.enqueue({frame, meterModel});
            scheduleNextResponse();
        }

        emit dataWritten(diCode, value, true);
    }
    else
    {
        LOG_WARNING(QString("Failed to write auth status: %1").arg(diCode));
        ProtocolFrame response = m_protocolParser->buildErrorResponse(frame, 0x03);
        sendResponse(response, meterModel);
        emit dataWritten(diCode, value, false);
    }
}

void ProtocolController::processRemoteControlRequest(const ProtocolFrame &frame, MeterModel *meterModel)
{
    LOG_INFO(QStringLiteral("Processing remote-control request: %1").arg(frame.toString()));

    if (!meterModel)
    {
        LOG_WARNING(QStringLiteral("Remote control ignored: meter model is null"));
        return;
    }

    const QByteArray decoded = DLT645DataHelper::subtract33(frame.dataField);
    // PA(1) + 密码(3) + 操作者代码(4) + 命令码(1) [+ 保留/参数]
    if (decoded.size() < 9)
    {
        LOG_WARNING(QStringLiteral("Remote control data too short: %1 bytes").arg(decoded.size()));
        ProtocolFrame response = m_protocolParser->buildErrorResponse(frame, 0x01);
        sendResponse(response, meterModel);
        emit protocolError(0x01, QStringLiteral("远程控制数据域长度不足"));
        return;
    }

    const quint8 command = static_cast<quint8>(decoded.at(8));
    if (!isKnownRemoteControlCommand(command))
    {
        LOG_WARNING(QStringLiteral("Unsupported remote-control command: 0x%1")
                        .arg(command, 2, 16, QChar('0'))
                        .toUpper());
        ProtocolFrame response = m_protocolParser->buildErrorResponse(frame, 0x01);
        sendResponse(response, meterModel);
        emit protocolError(0x01, QStringLiteral("不支持的远程控制命令: 0x%1")
                                         .arg(command, 2, 16, QChar('0'))
                                         .toUpper());
        return;
    }

    if (!applyRemoteControlToStatusWord3(meterModel, command))
    {
        LOG_WARNING(QStringLiteral("Failed to apply remote-control command to status word 3"));
        ProtocolFrame response = m_protocolParser->buildErrorResponse(frame, 0x03);
        sendResponse(response, meterModel);
        emit protocolError(0x03, QStringLiteral("远程控制状态更新失败"));
        return;
    }

    const quint16 statusWord3 = static_cast<quint16>(meterModel->getDataValue(statusWordDiCode(3)).toUInt());
    LOG_INFO(QStringLiteral("Remote control applied: %1, statusWord3=0x%2")
                 .arg(remoteControlCommandLabel(command))
                 .arg(statusWord3, 4, 16, QChar('0'))
                 .toUpper());

    if (m_autoResponseEnabled)
    {
        m_pendingResponses.enqueue({frame, meterModel});
        scheduleNextResponse();
    }
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

    // 标准写数据帧数据域：DI(4) + 权限PA(1) + 密码(3) + 操作者代码(4) + 数据
    constexpr int kWriteValueOffset = 12;
    QByteArray valueBytes = frame.dataField.mid(kWriteValueOffset);
    QVariant value;

    std::reverse(valueBytes.begin(), valueBytes.end());
    for (int i = 0; i < valueBytes.size(); ++i)
    {
        quint8 byte = static_cast<quint8>(valueBytes[i]);
        byte -= 0x33;
        valueBytes[i] = byte;
    }

    if (diCode == "0x04000701" ||
        diCode == "0x04000702" ||
        diCode == "0x04000703" ||
        diCode == "0x04000704" ||
        diCode == "0x04000705" ||
        diCode == QStringLiteral("0x1C0104CD"))
    {
        if (valueBytes.size() >= 1)
        {
            value = static_cast<int>(static_cast<quint8>(valueBytes[0]));
        }
    }
    else if (!valueBytes.isEmpty())
    {
        // 按数据项 format 解码写入值，保证小数位与读路径对称
        // （旧实现固定按 2 位小数，写电压/电流等会存错值）
        QString format;
        int decimalPlaces = 0;
        if (meterModel)
        {
            const DataItem dataItem = meterModel->getDataItemRaw(diCode);
            format = dataItem.format;
            decimalPlaces = dataItem.decimalPlaces;
        }

        const QString decoded = DLT645DataHelper::decodePayloadByFormat(valueBytes, format, decimalPlaces);
        bool numericOk = false;
        const double numericValue = decoded.toDouble(&numericOk);
        if (numericOk)
        {
            value = numericValue;
        }
        else if (!decoded.isEmpty())
        {
            value = decoded;
        }
        else if (valueBytes.size() >= 4)
        {
            value = BCDHelper::bcdToDecimal(valueBytes, decimalPlaces);
        }
    }

    if (meterModel && meterModel->setDataValue(diCode, value))
    {
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
    ProtocolFrame responseToSend = response;
    finalizeResponseAddress(responseToSend, responseMeterModel);
    const QByteArray responseData =
        ProtocolParser::frameToRawBytes(responseToSend,
                                        responseMeterModel && responseMeterModel->getIncludePrefixFE());

    if (sendTransportData(responseData))
    {
        emit frameSent(responseToSend, responseData);
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

    if (isBroadcastAddress(addressBytes))
    {
        return true;
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
    if (isBroadcastAddress(addressBytes))
    {
        if (!m_knownMeterAddresses.isEmpty())
        {
            const QString &firstAddress = m_knownMeterAddresses.first();
            if (m_meterModelsByAddress.contains(firstAddress))
            {
                return m_meterModelsByAddress.value(firstAddress);
            }
        }

        return m_meterModel;
    }

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
