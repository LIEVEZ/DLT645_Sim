#ifndef BCDHELPER_H
#define BCDHELPER_H

#include <QByteArray>
#include <QString>
#include <QVariant>

/**
 * @brief BCD码转换工具类
 *
 * 用于DLT645-2007协议中的BCD码与十进制数之间的转换
 */
class BCDHelper
{
public:
    /**
     * @brief 将BCD码转换为十进制数
     * @param bcdData BCD码字节数组
     * @param decimalPlaces 小数位数
     * @return 转换后的十进制数值
     */
    static qreal bcdToDecimal(const QByteArray &bcdData, int decimalPlaces = 0);

    /**
     * @brief 将十进制数转换为BCD码
     * @param value 十进制数值
     * @param byteCount 输出字节数
     * @param decimalPlaces 小数位数
     * @return BCD码字节数组
     */
    static QByteArray decimalToBCD(qreal value, int byteCount, int decimalPlaces = 0);

    /**
     * @brief 将BCD码字符串转换为十进制字符串
     * @param bcdString BCD码字符串（如"123456"）
     * @param decimalPlaces 小数位数
     * @return 十进制字符串
     */
    static QString bcdStringToDecimal(const QString &bcdString, int decimalPlaces = 0);

    /**
     * @brief 将十进制字符串转换为BCD码字符串
     * @param decimalString 十进制字符串
     * @param decimalPlaces 小数位数
     * @return BCD码字符串
     */
    static QString decimalToBCDString(const QString &decimalString, int decimalPlaces = 0);

    /**
     * @brief 将BCD码转换为时间字符串
     * @param bcdData BCD码字节数组（6字节：秒分时日月年）
     * @return 时间字符串（格式：YYYY-MM-DD HH:mm:ss）
     */
    static QString bcdToDateTime(const QByteArray &bcdData);

    /**
     * @brief 将时间字符串转换为BCD码
     * @param dateTimeString 时间字符串（格式：YYYY-MM-DD HH:mm:ss）
     * @return BCD码字节数组（6字节）
     */
    static QByteArray dateTimeToBCD(const QString &dateTimeString);

    /**
     * @brief 验证BCD码是否有效
     * @param bcdData BCD码字节数组
     * @return 是否有效
     */
    static bool isValidBCD(const QByteArray &bcdData);

    /**
     * @brief 将字节数组转换为十六进制字符串
     * @param data 字节数组
     * @param separator 分隔符（默认空格）
     * @return 十六进制字符串
     */
    static QString bytesToHexString(const QByteArray &data, const QString &separator = " ");

    /**
     * @brief 将十六进制字符串转换为字节数组
     * @param hexString 十六进制字符串
     * @return 字节数组
     */
    static QByteArray hexStringToBytes(const QString &hexString);

    /**
     * @brief 将数字字符串转换为BCD字节
     * @param digitStr 数字字符串（1-2位）
     * @return BCD字节
     */
    static char convertToBCD(const QString &digitStr);

    /**
     * @brief 根据格式将数值转换为字符串
     * @param value 数值
     * @param format 格式字符串（如"AA,xx,AA"）
     * @return 格式化后的字符串
     */
    static QString formatValueToString(const QVariant &value, const QString &format);
    // 将字符串转为字节数组
    static QByteArray stringToByteArray(const QString &str);

private:
    BCDHelper() = default;
};

#endif // BCDHELPER_H
