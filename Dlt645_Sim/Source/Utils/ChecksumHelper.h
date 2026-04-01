#ifndef CHECKSUMHELPER_H
#define CHECKSUMHELPER_H

#include <QByteArray>

/**
 * @brief 校验和计算工具类
 *
 * 用于DLT645-2007协议中的校验和计算与验证
 */
class ChecksumHelper
{
public:
    /**
     * @brief 计算校验和（累加和）
     * @param data 数据字节数组
     * @return 校验和（单字节）
     */
    static quint8 calculateChecksum(const QByteArray &data);

    /**
     * @brief 验证校验和
     * @param data 包含校验和的完整数据
     * @return 是否有效
     */
    static bool verifyChecksum(const QByteArray &data);

    /**
     * @brief 计算CRC16校验（可选，用于扩展）
     * @param data 数据字节数组
     * @return CRC16校验值
     */
    static quint16 calculateCRC16(const QByteArray &data);

private:
    ChecksumHelper() = default;
};

#endif // CHECKSUMHELPER_H
