#ifndef PROTOCOLCONTROLLER_H
#define PROTOCOLCONTROLLER_H

#include <QObject>
#include <QMap>
#include <QQueue>
#include <QStringList>
#include <QTimer>
#include "Models/MeterModel.h"
#include "Services/ProtocolParser.h"
#include "Controllers/SerialController.h"
#include "Controllers/TcpController.h"

/**
 * @brief 协议控制器
 *
 * 负责协调串口通信和协议解析，处理DLT645-2007协议的请求和应答
 */
class ProtocolController : public QObject
{
    Q_OBJECT

public:
    explicit ProtocolController(QObject *parent = nullptr);
    ~ProtocolController();

    MeterModel *meterModel() const;
    void setMeterModel(MeterModel *meterModel);
    void registerMeterModel(const QString &address, MeterModel *meterModel);
    void unregisterMeterModel(const QString &address);

    /**
     * @brief 设置串口控制器
     * @param controller 串口控制器
     */
    void setSerialController(SerialController *controller);
    void setTcpController(TcpController *controller);

    /**
     * @brief 处理接收到的数据
     * @param data 接收到的数据
     */
    void processReceivedData(const QByteArray &data);

    /**
     * @brief 启用/禁用自动应答
     * @param enabled 是否启用
     */
    void setAutoResponseEnabled(bool enabled);

    /**
     * @brief 设置应答延迟
     * @param delay 延迟时间（毫秒）
     */
    void setResponseDelay(int delay);

    /**
     * @brief 设置是否有电表节点
     * @param hasMeter 是否有电表节点
     */
    void setHasMeter(bool hasMeter);

    /**
     * @brief 设置当前已创建的电表地址列表
     * @param addresses 电表地址列表
     */
    void setKnownMeterAddresses(const QStringList &addresses);

signals:
    /**
     * @brief 数据读取信号
     * @param diCode 数据标识
     * @param value 数据值
     */
    void dataRead(const QString &diCode, const QVariant &value);

    /**
     * @brief 数据写入信号
     * @param diCode 数据标识
     * @param value 数据值
     * @param success 是否成功
     */
    void dataWritten(const QString &diCode, const QVariant &value, bool success);

    /**
     * @brief 协议错误信号
     * @param errorCode 错误码
     * @param errorMessage 错误消息
     */
    void protocolError(quint8 errorCode, const QString &errorMessage);

    /**
     * @brief 帧接收信号
     * @param frame 接收到的帧
     */
    void frameReceived(const ProtocolFrame &frame, const QByteArray &rawFrame);

    /**
     * @brief 帧发送信号
     * @param frame 发送的帧
     */
    void frameSent(const ProtocolFrame &frame, const QByteArray &rawFrame);

    /**
     * @brief 原始数据接收信号（包含前导码）
     * @param data 接收到的原始数据
     */
    void rawDataReceived(const QByteArray &data);

    /**
     * @brief 原始数据发送信号（包含前导码）
     * @param data 发送的原始数据
     */
    void rawDataSent(const QByteArray &data);

private slots:
    void onSerialDataReceived(const QByteArray &data);
    void onSerialDataSent(const QByteArray &data);
    void onSerialConnectionStateChanged(bool connected);
    void onTcpDataReceived(const QByteArray &data);
    void onTcpDataSent(const QByteArray &data);
    void onTcpConnectionStateChanged(bool connected);
    void onProtocolFrameParsed(const ProtocolFrame &frame);
    void onProtocolParseError(const QString &errorMessage);
    void onResponseTimerTimeout();

private:
    void processReadRequest(const ProtocolFrame &frame, MeterModel *meterModel);
    void processWriteRequest(const ProtocolFrame &frame, MeterModel *meterModel);
    void processWriteAuthStatusRequest(const ProtocolFrame &frame, MeterModel *meterModel);
    void sendResponse(const ProtocolFrame &response, MeterModel *meterModel = nullptr);
    void verifyPassword(const ProtocolFrame &frame, MeterModel *meterModel);
    bool tryTakeNextReceivedFrame(ProtocolFrame &frame, QByteArray &rawFrame);
    bool shouldRespondToAddress(const QByteArray &addressBytes) const;
    MeterModel *meterModelForAddress(const QByteArray &addressBytes) const;
    bool hasActiveTransport() const;
    bool sendTransportData(const QByteArray &data);
    void resetTransportState();
    void scheduleNextResponse();

    struct PendingResponse
    {
        ProtocolFrame frame;
        MeterModel *meterModel = nullptr;
    };

    MeterModel *m_meterModel;
    SerialController *m_serialController;
    TcpController *m_tcpController;
    ProtocolParser *m_protocolParser;
    bool m_autoResponseEnabled;
    int m_responseDelay;
    bool m_hasMeter;
    QStringList m_knownMeterAddresses;
    QTimer *m_responseTimer;
    QQueue<PendingResponse> m_pendingResponses;
    QMap<QString, MeterModel *> m_meterModelsByAddress;
    QByteArray m_receiveBuffer;
};

#endif // PROTOCOLCONTROLLER_H
