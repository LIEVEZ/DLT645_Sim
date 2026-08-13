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
    quint8 startFlag = 0;       // 起始符 (0x68)
    QByteArray address;         // 地址域 (6字节)
    quint8 startFlag2 = 0;      // 起始符2 (0x68)
    quint8 controlCode = 0;     // 控制码
    quint8 dataLength = 0;      // 数据域长度
    QByteArray dataField;       // 数据域
    quint8 checksum = 0;        // 校验和
    quint8 endFlag = 0;         // 结束符 (0x16)
    int parsedSize = 0;         // 实际解析的字节数（不包括前导码）
    bool checksumValid = false; // 校验和是否通过（仅 parseFrame 接收解析时设置）

    bool isValid() const;
    QByteArray toByteArray() const;
    QString toString() const;
};

/**
 * @brief 控制码枚举
 *
 * 取值依据 DL/T 645-2007 控制码功能表（应答时最高位置 1，异常应答再置 D6）。
 */
enum class ControlCode : quint8
{
    // 广播校时
    BroadcastTime = 0x08,

    // 读数据
    ReadData = 0x11,

    // 读后续数据
    ReadNextData = 0x12,

    // 读通信地址
    ReadAddress = 0x13,

    // 写数据
    WriteData = 0x14,

    // 写通信地址
    WriteAddress = 0x15,

    // 冻结命令
    FreezeData = 0x16,

    // 更改通信速率
    ChangeBaudRate = 0x17,

    // 修改密码
    ChangePassword = 0x18,

    // 最大需量清零
    ClearDemand = 0x19,

    // 电表清零
    ClearMeter = 0x1A,

    // 事件清零
    ClearEvent = 0x1B,

    // 远程控制（跳合闸、报警、保电）
    RemoteControl = 0x1C,

    // 厂商扩展：读认证状态（应答 0xAA）
    ReadAuthStatus = 0x2A,

    // 厂商扩展：写认证状态（应答 0xAF，无密码/操作者代码）
    WriteAuthStatus = 0x2F
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
     * @brief 构建写数据请求帧（标准格式：DI + 权限 + 密码 + 操作者代码 + 数据）
     * @param address 电表地址
     * @param diCode 数据标识
     * @param value 数据值
     * @param permission 权限级别（2位数字）
     * @param password 密码（6位数字）
     * @param operatorCode 操作者代码（8位数字）
     * @param includePrefixFE 是否包含前导FE
     * @return 协议帧
     */
    ProtocolFrame buildWriteRequest(const QByteArray &address,
                                    const QString &diCode,
                                    const QVariant &value,
                                    const QString &permission,
                                    const QString &password,
                                    const QString &operatorCode,
                                    bool includePrefixFE = true);

    /**
     * @brief 构建远程控制请求帧
     * @param address 电表地址
     * @param command 命令码（跳闸 0x1A、合闸 0x1B 等）
     * @param commandParameters N2 保留字节和 N3~N8 时间，共 7 字节；不足部分补 0
     * @param permission 权限级别（2位数字）
     * @param password 密码（6位数字）
     * @param operatorCode 操作者代码（8位数字）
     * @param includePrefixFE 是否包含前导FE
     * @return 协议帧
     */
    ProtocolFrame buildRemoteControlRequest(const QByteArray &address,
                                            quint8 command,
                                            const QByteArray &commandParameters,
                                            const QString &permission,
                                            const QString &password,
                                            const QString &operatorCode,
                                            bool includePrefixFE = true);

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
    /**
     * @brief 认证状态厂商扩展控制码（0x2A/0x2F 及其应答）是否使用“长度域比实际数据域少 1 字节”规则
     */
    static bool usesAuthStatusLengthQuirk(quint8 controlCode);

    /**
     * @brief 根据控制码与长度域得到实际数据域字节数
     * 认证状态读写：长度域 > 0 时实际长度 = 长度域 + 1（写成功应答 L=0 除外）
     */
    static int effectiveDataFieldLength(quint8 controlCode, quint8 lengthField);

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
