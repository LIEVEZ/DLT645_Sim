#ifndef PROTOCOLPARSER_H
#define PROTOCOLPARSER_H

#include <QObject>
#include <QByteArray>
#include "Utils/BCDHelper.h"
#include "Utils/ChecksumHelper.h"
#include "Models/MeterModel.h"

/**
 * @brief DLT645-2007协议帧结构
 */
struct ProtocolFrame
{
    quint8 startFlag;     // 起始符 (0x68)
    QByteArray address;   // 地址域 (6字节)
    quint8 startFlag2;    // 起始符2 (0x68)
    quint8 controlCode;   // 控制码
    quint8 dataLength;    // 数据域长度
    QByteArray dataField; // 数据域
    quint8 checksum;      // 校验和
    quint8 endFlag;       // 结束符 (0x16)
    int parsedSize;       // 实际解析的字节数（不包括前导码）

    bool isValid() const;
    QByteArray toByteArray() const;
    QString toString() const;
};

/**
 * @brief 控制码枚举
 */
enum class ControlCode : quint8
{
    // 读数据
    ReadData = 0x11,

    // 读后续数据
    ReadNextData = 0x12,

    // 读后续数据响应
    ReadNextDataResponse = 0x92,

    // 写数据
    WriteData = 0x14,

    // 读通信地址
    ReadAddress = 0x13,

    // 写通信地址
    WriteAddress = 0x15,

    // 广播校时
    BroadcastTime = 0x16,

    // 写设备地址
    WriteDeviceAddress = 0x17,

    // 冻结命令
    FreezeData = 0x18,

    // 读冻结数据
    ReadFreezeData = 0x19,

    // 更改通信速率
    ChangeBaudRate = 0x1A,

    // 修改密码
    ChangePassword = 0x1B,

    // 最大需量清零
    ClearDemand = 0x1C,

    // 电表清零
    ClearMeter = 0x1D,

    // 事件清零
    ClearEvent = 0x1E
};

/**
 * @brief 协议解析服务类
 *
 * 负责DLT645-2007协议帧的解析和构建
 */
class ProtocolParser : public QObject
{
    Q_OBJECT

public:
    explicit ProtocolParser(MeterModel *meterModel, QObject *parent = nullptr);
    ~ProtocolParser();
    void setMeterModel(MeterModel *meterModel);

    /**
     * @brief 解析接收到的数据帧
     * @param data 接收到的字节数组
     * @return 解析后的协议帧
     */
    ProtocolFrame parseFrame(const QByteArray &data);

    /**
     * @brief 构建读数据请求帧
     * @param address 电表地址
     * @param diCode 数据标识
     * @param includePrefixFE 是否包含前导FE
     * @return 协议帧
     */
    ProtocolFrame buildReadRequest(const QByteArray &address, const QString &diCode, bool includePrefixFE = true);

    /**
     * @brief 构建读取通信地址请求帧
     * @param address 电表地址
     * @param includePrefixFE 是否包含前导FE
     * @return 协议帧
     */
    ProtocolFrame buildReadAddressRequest(const QByteArray &address, bool includePrefixFE = true);

    /**
     * @brief 构建写数据请求帧
     * @param address 电表地址
     * @param diCode 数据标识
     * @param value 数据值
     * @param includePrefixFE 是否包含前导FE
     * @return 协议帧
     */
    ProtocolFrame buildWriteRequest(const QByteArray &address, const QString &diCode, const QVariant &value, bool includePrefixFE = true);

    /**
     * @brief 构建读数据应答帧
     * @param requestFrame 请求帧
     * @param dataValue 数据值
     * @param diCode 数据标识
     * @return 应答帧
     */
    ProtocolFrame buildReadResponse(const ProtocolFrame &requestFrame, const QVariant &dataValue, const QString &diCode = QString());

    /**
     * @brief 构建写数据应答帧
     * @param requestFrame 请求帧
     * @param success 是否成功
     * @return 应答帧
     */
    ProtocolFrame buildWriteResponse(const ProtocolFrame &requestFrame, bool success);

    /**
     * @brief 构建异常应答帧
     * @param requestFrame 请求帧
     * @param errorCode 错误码
     * @return 异常应答帧
     */
    ProtocolFrame buildErrorResponse(const ProtocolFrame &requestFrame, quint8 errorCode);

    /**
     * @brief 从数据域中提取数据标识
     * @param dataField 数据域
     * @return 数据标识字符串
     */
    static QString extractDIFromDataField(const QByteArray &dataField);

    /**
     * @brief 将数据标识转换为字节数组
     * @param diCode 数据标识字符串（如"0x00000100"）
     * @return 字节数组（4字节）
     */
    static QByteArray diCodeToBytes(const QString &diCode);

    /**
     * @brief 将字节数组转换为数据标识
     * @param bytes 字节数组（4字节）
     * @return 数据标识字符串
     */
    static QString bytesToDI(const QByteArray &bytes);

    /**
     * @brief 验证地址格式
     * @param address 地址字符串（12位数字）
     * @return 是否有效
     */
    static bool isValidAddress(const QString &address);

    /**
     * @brief 将地址字符串转换为字节数组
     * @param address 地址字符串（12位数字）
     * @return 字节数组（6字节BCD码）
     */
    static QByteArray addressToBytes(const QString &address);

    /**
     * @brief 将地址字节数组转换为字符串
     * @param bytes 地址字节数组（6字节BCD码）
     * @return 地址字符串
     */
    static QString bytesToAddress(const QByteArray &bytes);

    /**
     * @brief 将协议帧转换为实际收发使用的原始字节流
     * @param frame 协议帧
     * @param includePrefixFE 是否包含4个前导FE
     * @return 原始字节流
     */
    static QByteArray frameToRawBytes(const ProtocolFrame &frame, bool includePrefixFE);

signals:
    /**
     * @brief 帧解析完成信号
     * @param frame 解析后的帧
     */
    void frameParsed(const ProtocolFrame &frame);

    /**
     * @brief 解析错误信号
     * @param errorMessage 错误消息
     */
    void parseError(const QString &errorMessage);

private:
    MeterModel *m_meterModel; // 电表模型指针

    /**
     * @brief 处理数据值，根据格式转换为字节数组
     * @param value 数据值
     * @param format 数据格式
     * @return 转换后的字节数组
     */
    QByteArray processDataValue(const QVariant &value, const QString &format);
};

#endif // PROTOCOLPARSER_H
