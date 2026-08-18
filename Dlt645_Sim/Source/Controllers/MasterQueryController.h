#ifndef MASTERQUERYCONTROLLER_H
#define MASTERQUERYCONTROLLER_H

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>

#include "Services/ProtocolParser.h"

struct MasterSerialConfig
{
    QString portName;
    qint32 baudRate;
    QSerialPort::DataBits dataBits;
    QSerialPort::Parity parity;
    QSerialPort::StopBits stopBits;
    QSerialPort::FlowControl flowControl;

    MasterSerialConfig()
        : portName("COM1"),
          baudRate(2400),
          dataBits(QSerialPort::Data8),
          parity(QSerialPort::EvenParity),
          stopBits(QSerialPort::OneStop),
          flowControl(QSerialPort::NoFlowControl)
    {
    }
};

struct MasterRequestSpec
{
    QString meterAddress;
    QString diCode;
    bool includePrefixFE = true;
    int timeoutMs = 3000;
    bool rawSend = false;
    bool broadcastReadAddress = false; // 广播读地址：地址域使用 6×0xAA 通配，应答地址为电表真实地址
    bool writeData = false;
    QString writeValue;
    bool remoteControl = false;
    quint8 remoteControlCommand = 0;
    QByteArray remoteControlParameters;
    QString remoteControlPermission;
    QString remoteControlPassword;
    QString remoteControlOperatorCode;

    bool isReadAddressRequest() const
    {
        return !rawSend && !writeData && !remoteControl && diCode.trimmed().isEmpty();
    }

    bool isWriteDataRequest() const
    {
        return !rawSend && writeData;
    }

    bool isRemoteControlRequest() const
    {
        return !rawSend && remoteControl;
    }

    bool isRawSendRequest() const
    {
        return rawSend;
    }
};

enum class MasterQueryStatus
{
    Idle,
    Sending,
    WaitingResponse,
    Completed,
    Timeout,
    Error
};

struct MasterQueryResult
{
    MasterQueryStatus status = MasterQueryStatus::Idle;
    MasterRequestSpec requestSpec;
    QByteArray rawRequest;
    QByteArray rawResponse;
    ProtocolFrame requestFrame;
    ProtocolFrame responseFrame;
    QString errorText;
    QDateTime timestamp;
    QString responseMeterAddress; // 读地址应答中解析出的电表真实地址（广播查询时用于回填）
};

Q_DECLARE_METATYPE(MasterQueryStatus)
Q_DECLARE_METATYPE(MasterQueryResult)

class MasterQueryController : public QObject
{
    Q_OBJECT

public:
    explicit MasterQueryController(QObject *parent = nullptr);
    ~MasterQueryController();

    static QList<QSerialPortInfo> getAvailablePorts();

    bool openPort(const MasterSerialConfig &config);
    void closePort();
    bool isConnected() const;

    MasterSerialConfig currentConfig() const;
    MasterQueryStatus currentStatus() const;
    MasterQueryResult pendingRequest() const;

    bool sendReadRequest(const MasterRequestSpec &spec);

    /**
     * @brief 发送广播读通信地址请求（地址域 6×0xAA，用于发现未知电表地址）
     * @param includePrefixFE 是否包含前导 FE
     * @param timeoutMs 等待应答超时时间
     * @return 是否已写入串口并进入等待应答状态
     */
    bool sendBroadcastReadAddressRequest(bool includePrefixFE = true, int timeoutMs = 3000);

    /**
     * @brief 直接发送原始 HEX 报文（不构建协议帧，不做地址/DI 校验）
     * @param rawBytes 待发送的原始字节
     * @param timeoutMs 等待应答超时时间
     * @return 是否已写入串口并进入等待应答状态
     */
    bool sendRawFrame(const QByteArray &rawBytes, int timeoutMs = 3000);

signals:
    void sessionStateChanged(bool connected);
    void requestStarted(const MasterQueryResult &result);
    void responseReceived(const MasterQueryResult &result);
    void requestFailed(const MasterQueryResult &result);
    void noticeGenerated(const QString &message);

private slots:
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);
    void onRequestTimeout();

private:
    enum class FrameExtractState
    {
        NoFrame,
        ParsedFrame,
        InvalidFrame
    };

    ProtocolFrame buildRequestFrame(const MasterRequestSpec &spec) const;
    QByteArray buildRawFrame(const ProtocolFrame &frame, bool includePrefixFE) const;
    FrameExtractState tryTakeNextFrame(ProtocolFrame &frame, QByteArray &rawFrame, QString &errorText);
    bool validateResponseFrame(const ProtocolFrame &frame, QString &errorText) const;
    void setStatus(MasterQueryStatus status);
    void clearPendingRequest();
    void failPendingRequest(MasterQueryStatus status, const QString &errorText, const QByteArray &rawResponse = QByteArray());

    QSerialPort *m_serialPort;
    ProtocolParser *m_protocolParser;
    QTimer *m_timeoutTimer;
    MasterSerialConfig m_config;
    MasterQueryStatus m_status;
    MasterQueryResult m_pendingResult;
    QByteArray m_receiveBuffer;
};

#endif // MASTERQUERYCONTROLLER_H
