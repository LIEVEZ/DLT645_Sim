#ifndef METERPASSWORDGENERATOR_H
#define METERPASSWORDGENERATOR_H

#include <QByteArray>
#include <QString>

/**
 * @brief 电表动态密码生成工具
 *
 * 按既有 Java 实现计算六位电表密码：表号 BCD 与密钥循环异或后，
 * 分别对奇偶字节组计算 CRC-16/X25，再转换并截取结果。
 */
class MeterPasswordGenerator
{
public:
    /**
     * @brief 生成电表密码
     * @param meterAddress 12 位数字电表地址。
     * @param key 用于循环异或的 UTF-8 密钥，不能为空。
     * @param password 成功时返回六位密码。
     * @param errorMessage 失败原因，可为 nullptr。
     * @return 生成成功返回 true，否则返回 false。
     */
    static bool generate(const QString &meterAddress,
                         const QString &key,
                         QString *password,
                         QString *errorMessage = nullptr);

private:
    static quint16 calculateCrc16X25(const QByteArray &data);
    static QByteArray meterAddressToBcd(const QString &meterAddress);
};

#endif // METERPASSWORDGENERATOR_H
