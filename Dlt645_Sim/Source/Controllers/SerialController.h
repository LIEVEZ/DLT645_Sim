#ifndef SERIALCONTROLLER_H
#define SERIALCONTROLLER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>

/**
 * @brief 串口配置结构
 */
struct SerialConfig
{
    QString portName;                     // 串口名称（如"COM3"）
    qint32 baudRate;                      // 波特率
    QSerialPort::DataBits dataBits;       // 数据位
    QSerialPort::Parity parity;           // 校验位
    QSerialPort::StopBits stopBits;       // 停止位
    QSerialPort::FlowControl flowControl; // 流控

    SerialConfig()
        : portName("COM1"), baudRate(2400), dataBits(QSerialPort::Data8), parity(QSerialPort::EvenParity), stopBits(QSerialPort::OneStop), flowControl(QSerialPort::NoFlowControl)
    {
    }
};

/**
 * @brief 串口通信控制器
 *
 * 负责串口的打开、关闭、数据收发
 */
class SerialController : public QObject
{
    Q_OBJECT

public:
    explicit SerialController(QObject *parent = nullptr);
    ~SerialController();

    /**
     * @brief 获取可用串口列表
     * @return 串口信息列表
     */
    static QList<QSerialPortInfo> getAvailablePorts();

    /**
     * @brief 打开串口
     * @param config 串口配置
     * @return 是否成功
     */
    bool openPort(const SerialConfig &config);

    /**
     * @brief 关闭串口
     */
    void closePort();

    /**
     * @brief 发送数据
     * @param data 要发送的数据
     * @return 是否成功
     */
    bool sendData(const QByteArray &data);

    /**
     * @brief 检查串口是否已打开
     * @return 是否已打开
     */
    bool isOpen() const;

    /**
     * @brief 获取当前配置
     * @return 串口配置
     */
    SerialConfig getConfig() const;

signals:
    /**
     * @brief 数据接收信号
     * @param data 接收到的数据
     */
    void dataReceived(const QByteArray &data);

    /**
     * @brief 数据发送信号
     * @param data 发送的数据
     */
    void dataSent(const QByteArray &data);

    /**
     * @brief 连接状态改变信号
     * @param connected 是否已连接
     */
    void connectionStateChanged(bool connected);

    /**
     * @brief 错误信号
     * @param errorString 错误描述
     */
    void errorOccurred(const QString &errorString);

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    QSerialPort *m_serialPort;
    SerialConfig m_config;
    QByteArray m_receiveBuffer;
};

#endif // SERIALCONTROLLER_H
