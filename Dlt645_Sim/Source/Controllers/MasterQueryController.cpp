#include "MasterQueryController.h"

#include "Utils/Logger.h"

MasterQueryController::MasterQueryController(QObject *parent)
    : QObject(parent),
      m_serialPort(new QSerialPort(this)),
      m_protocolParser(new ProtocolParser(nullptr, this)),
      m_timeoutTimer(new QTimer(this)),
      m_status(MasterQueryStatus::Idle)
{
    qRegisterMetaType<MasterQueryStatus>("MasterQueryStatus");
    qRegisterMetaType<MasterQueryResult>("MasterQueryResult");

    m_timeoutTimer->setSingleShot(true);

    connect(m_serialPort, &QSerialPort::readyRead, this, &MasterQueryController::onReadyRead);
    connect(m_serialPort,
            QOverload<QSerialPort::SerialPortError>::of(&QSerialPort::errorOccurred),
            this,
            &MasterQueryController::onSerialError);
    connect(m_timeoutTimer, &QTimer::timeout, this, &MasterQueryController::onRequestTimeout);
}

MasterQueryController::~MasterQueryController()
{
    closePort();
}

QList<QSerialPortInfo> MasterQueryController::getAvailablePorts()
{
    return QSerialPortInfo::availablePorts();
}

bool MasterQueryController::openPort(const MasterSerialConfig &config)
{
    if (m_serialPort->isOpen())
    {
        closePort();
    }

    m_config = config;
    m_receiveBuffer.clear();
    clearPendingRequest();

    m_serialPort->setPortName(config.portName);
    m_serialPort->setBaudRate(config.baudRate);
    m_serialPort->setDataBits(config.dataBits);
    m_serialPort->setParity(config.parity);
    m_serialPort->setStopBits(config.stopBits);
    m_serialPort->setFlowControl(config.flowControl);

    if (!m_serialPort->open(QIODevice::ReadWrite))
    {
        const QString errorText = m_serialPort->errorString();
        LOG_ERROR(QString("Master query open serial failed: %1").arg(errorText));
        emit noticeGenerated(errorText);
        return false;
    }

    LOG_INFO(QString("Master query serial opened: %1 @ %2")
                 .arg(config.portName)
                 .arg(config.baudRate));
    emit sessionStateChanged(true);
    return true;
}

void MasterQueryController::closePort()
{
    if (!m_serialPort->isOpen())
    {
        return;
    }

    if (m_status == MasterQueryStatus::WaitingResponse || m_status == MasterQueryStatus::Sending)
    {
        failPendingRequest(MasterQueryStatus::Error, QString::fromUtf8("串口已关闭，请求被中止。"));
    }

    m_timeoutTimer->stop();
    m_receiveBuffer.clear();
    m_serialPort->close();
    setStatus(MasterQueryStatus::Idle);
    emit sessionStateChanged(false);
}

bool MasterQueryController::isConnected() const
{
    return m_serialPort->isOpen();
}

MasterSerialConfig MasterQueryController::currentConfig() const
{
    return m_config;
}

MasterQueryStatus MasterQueryController::currentStatus() const
{
    return m_status;
}

MasterQueryResult MasterQueryController::pendingRequest() const
{
    return m_pendingResult;
}

bool MasterQueryController::sendReadRequest(const MasterRequestSpec &spec)
{
    if (!isConnected())
    {
        failPendingRequest(MasterQueryStatus::Error, QString::fromUtf8("串口尚未连接，无法发送请求。"));
        return false;
    }

    if (m_status == MasterQueryStatus::Sending || m_status == MasterQueryStatus::WaitingResponse)
    {
        failPendingRequest(MasterQueryStatus::Error, QString::fromUtf8("当前已有请求正在处理中，请等待完成后重试。"));
        return false;
    }

    if (!ProtocolParser::isValidAddress(spec.meterAddress))
    {
        failPendingRequest(MasterQueryStatus::Error, QString::fromUtf8("目标电表地址必须是 12 位数字。"));
        return false;
    }

    if (!spec.isReadAddressRequest() && spec.diCode.trimmed().isEmpty())
    {
        failPendingRequest(MasterQueryStatus::Error, QString::fromUtf8("请选择要读取的数据标识。"));
        return false;
    }

    m_receiveBuffer.clear();
    m_timeoutTimer->stop();

    m_pendingResult = MasterQueryResult();
    m_pendingResult.status = MasterQueryStatus::Sending;
    m_pendingResult.requestSpec = spec;
    m_pendingResult.requestFrame = buildRequestFrame(spec);
    m_pendingResult.rawRequest = buildRawFrame(m_pendingResult.requestFrame, spec.includePrefixFE);
    m_pendingResult.timestamp = QDateTime::currentDateTime();

    const qint64 bytesWritten = m_serialPort->write(m_pendingResult.rawRequest);
    if (bytesWritten != m_pendingResult.rawRequest.size())
    {
        failPendingRequest(MasterQueryStatus::Error, QString::fromUtf8("请求发送失败，串口写入字节数不完整。"));
        return false;
    }

    m_serialPort->flush();

    setStatus(MasterQueryStatus::Sending);
    emit requestStarted(m_pendingResult);

    m_pendingResult.status = MasterQueryStatus::WaitingResponse;
    setStatus(MasterQueryStatus::WaitingResponse);
    m_timeoutTimer->start(spec.timeoutMs > 0 ? spec.timeoutMs : 3000);

    return true;
}

void MasterQueryController::onReadyRead()
{
    const QByteArray data = m_serialPort->readAll();
    if (data.isEmpty())
    {
        return;
    }

    m_receiveBuffer.append(data);

    if (m_status != MasterQueryStatus::WaitingResponse)
    {
        emit noticeGenerated(QString::fromUtf8("收到未关联请求的串口数据：%1").arg(QString(data.toHex().toUpper())));
        return;
    }

    ProtocolFrame frame;
    QByteArray rawFrame;
    QString errorText;

    while (true)
    {
        const FrameExtractState state = tryTakeNextFrame(frame, rawFrame, errorText);
        if (state == FrameExtractState::NoFrame)
        {
            return;
        }

        if (state == FrameExtractState::InvalidFrame)
        {
            failPendingRequest(MasterQueryStatus::Error,
                               errorText.isEmpty() ? QString::fromUtf8("收到校验失败或格式异常的应答帧。") : errorText,
                               rawFrame);
            return;
        }

        QString validationError;
        if (!validateResponseFrame(frame, validationError))
        {
            emit noticeGenerated(validationError);
            continue;
        }

        m_timeoutTimer->stop();
        MasterQueryResult result = m_pendingResult;
        result.status = MasterQueryStatus::Completed;
        result.responseFrame = frame;
        result.rawResponse = rawFrame;
        result.timestamp = QDateTime::currentDateTime();
        setStatus(MasterQueryStatus::Completed);
        clearPendingRequest();
        setStatus(MasterQueryStatus::Idle);
        emit responseReceived(result);
        return;
    }
}

void MasterQueryController::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
    {
        return;
    }

    const QString errorText = m_serialPort->errorString();
    LOG_ERROR(QString("Master query serial error: %1").arg(errorText));

    if (error == QSerialPort::ResourceError ||
        error == QSerialPort::PermissionError ||
        error == QSerialPort::OpenError)
    {
        failPendingRequest(MasterQueryStatus::Error, errorText);

        if (m_serialPort->isOpen())
        {
            m_serialPort->close();
        }

        emit sessionStateChanged(false);
        setStatus(MasterQueryStatus::Idle);
        return;
    }

    emit noticeGenerated(errorText);
}

void MasterQueryController::onRequestTimeout()
{
    failPendingRequest(MasterQueryStatus::Timeout, QString::fromUtf8("等待电表响应超时。"));
}

ProtocolFrame MasterQueryController::buildRequestFrame(const MasterRequestSpec &spec) const
{
    const QByteArray addressBytes = ProtocolParser::addressToBytes(spec.meterAddress);
    if (spec.isReadAddressRequest())
    {
        return m_protocolParser->buildReadAddressRequest(addressBytes, spec.includePrefixFE);
    }

    return m_protocolParser->buildReadRequest(addressBytes, spec.diCode, spec.includePrefixFE);
}

QByteArray MasterQueryController::buildRawFrame(const ProtocolFrame &frame, bool includePrefixFE) const
{
    return ProtocolParser::frameToRawBytes(frame, includePrefixFE);
}

MasterQueryController::FrameExtractState MasterQueryController::tryTakeNextFrame(ProtocolFrame &frame,
                                                                                 QByteArray &rawFrame,
                                                                                 QString &errorText)
{
    rawFrame.clear();
    errorText.clear();

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
                    m_receiveBuffer = m_receiveBuffer.right(qMin(trailingFECount, 4));
                }
                else
                {
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

        constexpr int minimumHeaderSize = 10;
        if (m_receiveBuffer.size() < startIndex + minimumHeaderSize)
        {
            return FrameExtractState::NoFrame;
        }

        if (static_cast<quint8>(m_receiveBuffer[startIndex + 7]) != 0x68)
        {
            m_receiveBuffer.remove(0, startIndex + 1);
            continue;
        }

        const quint8 controlCode = static_cast<quint8>(m_receiveBuffer[startIndex + 8]);
        const quint8 dataLengthField = static_cast<quint8>(m_receiveBuffer[startIndex + 9]);
        const int actualDataLength = ProtocolParser::effectiveDataFieldLength(controlCode, dataLengthField);
        const int expectedFrameSize = startIndex + 12 + actualDataLength;
        if (m_receiveBuffer.size() < expectedFrameSize)
        {
            return FrameExtractState::NoFrame;
        }

        rawFrame = m_receiveBuffer.left(expectedFrameSize);
        frame = m_protocolParser->parseFrame(rawFrame);
        m_receiveBuffer.remove(0, expectedFrameSize);

        if (!frame.isValid() || !frame.checksumValid)
        {
            errorText = QString::fromUtf8("收到校验失败或格式异常的完整帧。");
            return FrameExtractState::InvalidFrame;
        }

        return FrameExtractState::ParsedFrame;
    }

    return FrameExtractState::NoFrame;
}

bool MasterQueryController::validateResponseFrame(const ProtocolFrame &frame, QString &errorText) const
{
    // 只比较低6位的控制码（bit0-5），忽略 bit7（应答标志）和 bit6（异常标志）
    const quint8 requestControlCode = m_pendingResult.requestFrame.controlCode & 0x3F;
    const quint8 responseControlCode = frame.controlCode & 0x3F;
    const bool isResponseFrame = (frame.controlCode & 0x80) != 0;

    if (!isResponseFrame)
    {
        errorText = QString::fromUtf8("收到请求帧而非应答帧，已忽略。");
        return false;
    }

    if (requestControlCode != responseControlCode)
    {
        errorText = QString::fromUtf8("收到控制码不匹配的应答帧，已忽略。");
        return false;
    }

    if (frame.address != m_pendingResult.requestFrame.address)
    {
        errorText = QString::fromUtf8("收到地址不匹配的应答帧，已忽略。");
        return false;
    }

    if (!m_pendingResult.requestSpec.isReadAddressRequest())
    {
        // 检查是否为异常应答（控制码 bit6=1）
        const bool isErrorResponse = (frame.controlCode & 0x40) != 0;

        if (isErrorResponse)
        {
            // 异常应答帧的数据域只有 1 字节错误码，这是正常的
            if (frame.dataField.size() != 1)
            {
                errorText = QString::fromUtf8("收到异常应答帧但数据域长度异常，已忽略。");
                return false;
            }
            // 异常应答帧不需要检查 DI 匹配，直接返回成功
            return true;
        }

        // 正常应答帧的检查
        if (frame.dataField.size() < 4)
        {
            errorText = QString::fromUtf8("收到数据域长度不足的应答帧，已忽略。");
            return false;
        }

        if (ProtocolParser::extractDIFromDataField(frame.dataField) != m_pendingResult.requestSpec.diCode)
        {
            errorText = QString::fromUtf8("收到 DI 不匹配的应答帧，已忽略。");
            return false;
        }
    }

    return true;
}

void MasterQueryController::setStatus(MasterQueryStatus status)
{
    m_status = status;
}

void MasterQueryController::clearPendingRequest()
{
    m_timeoutTimer->stop();
    m_pendingResult = MasterQueryResult();
}

void MasterQueryController::failPendingRequest(MasterQueryStatus status, const QString &errorText, const QByteArray &rawResponse)
{
    MasterQueryResult result = m_pendingResult;
    result.status = status;
    result.errorText = errorText;
    result.rawResponse = rawResponse;
    result.timestamp = QDateTime::currentDateTime();

    setStatus(status);
    clearPendingRequest();
    setStatus(MasterQueryStatus::Idle);
    emit requestFailed(result);
}
