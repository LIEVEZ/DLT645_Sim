#include "BCDHelper.h"
#include <QDateTime>
#include <QDebug>
#include <cmath>
#include <QtMath>
qreal BCDHelper::bcdToDecimal(const QByteArray &bcdData, int decimalPlaces)
{
    if (bcdData.isEmpty())
    {
        return 0.0;
    }

    QString decimalString;
    for (int i = 0; i < bcdData.size(); ++i)
    {
        quint8 byte = static_cast<quint8>(bcdData[i]);
        quint8 high = (byte >> 4) & 0x0F;
        quint8 low = byte & 0x0F;

        if (high > 9 || low > 9)
        {
            qWarning() << "Invalid BCD byte:" << QString::number(byte, 16);
            return 0.0;
        }

        decimalString += QString::number(high) + QString::number(low);
    }

    bool ok;
    qreal value = decimalString.toLongLong(&ok);

    if (!ok)
    {
        qWarning() << "Failed to convert BCD to decimal:" << decimalString;
        return 0.0;
    }

    // 处理小数位
    for (int i = 0; i < decimalPlaces; ++i)
    {
        value /= 10.0;
    }

    return value;
}

QByteArray BCDHelper::decimalToBCD(qreal value, int byteCount, int decimalPlaces)
{
    // 处理小数位
    for (int i = 0; i < decimalPlaces; ++i)
    {
        value *= 10.0;
    }

    qint64 intValue = static_cast<qint64>(value);
    QString decimalString = QString::number(intValue).rightJustified(byteCount * 2, '0');

    QByteArray bcdData;
    for (int i = 0; i < byteCount; ++i)
    {
        QString byteStr = decimalString.mid(i * 2, 2);
        quint8 byte = static_cast<quint8>(byteStr.toInt(nullptr, 16));
        bcdData.append(byte);
    }

    return bcdData;
}

QString BCDHelper::bcdStringToDecimal(const QString &bcdString, int decimalPlaces)
{
    QString decimalString = bcdString;

    // 插入小数点
    if (decimalPlaces > 0 && decimalString.length() > decimalPlaces)
    {
        int insertPos = decimalString.length() - decimalPlaces;
        decimalString.insert(insertPos, '.');
    }

    return decimalString;
}

QString BCDHelper::decimalToBCDString(const QString &decimalString, int decimalPlaces)
{
    QString result = decimalString;
    result.remove('.'); // 移除小数点

    // 补齐到偶数长度
    if (result.length() % 2 != 0)
    {
        result.prepend('0');
    }

    return result;
}

QString BCDHelper::bcdToDateTime(const QByteArray &bcdData)
{
    if (bcdData.size() < 6)
    {
        return QString();
    }

    // DLT645-2007时间格式：秒分时日月年（BCD码）
    quint8 second = (bcdData[0] >> 4) * 10 + (bcdData[0] & 0x0F);
    quint8 minute = (bcdData[1] >> 4) * 10 + (bcdData[1] & 0x0F);
    quint8 hour = (bcdData[2] >> 4) * 10 + (bcdData[2] & 0x0F);
    quint8 day = (bcdData[3] >> 4) * 10 + (bcdData[3] & 0x0F);
    quint8 month = (bcdData[4] >> 4) * 10 + (bcdData[4] & 0x0F);
    quint8 year = (bcdData[5] >> 4) * 10 + (bcdData[5] & 0x0F);

    // 处理年份（DLT645-2007使用2位年份，需要转换为4位）
    int fullYear = 2000 + year;

    QDateTime dateTime(QDate(fullYear, month, day), QTime(hour, minute, second));

    if (!dateTime.isValid())
    {
        qWarning() << "Invalid BCD datetime";
        return QString();
    }

    return dateTime.toString("yyyy-MM-dd HH:mm:ss");
}

QByteArray BCDHelper::dateTimeToBCD(const QString &dateTimeString)
{
    QDateTime dateTime = QDateTime::fromString(dateTimeString, "yyyy-MM-dd HH:mm:ss");

    if (!dateTime.isValid())
    {
        qWarning() << "Invalid datetime string:" << dateTimeString;
        return QByteArray();
    }

    QDate date = dateTime.date();
    QTime time = dateTime.time();

    QByteArray bcdData;

    // 秒
    quint8 second = (time.second() / 10) << 4 | (time.second() % 10);
    bcdData.append(second);

    // 分
    quint8 minute = (time.minute() / 10) << 4 | (time.minute() % 10);
    bcdData.append(minute);

    // 时
    quint8 hour = (time.hour() / 10) << 4 | (time.hour() % 10);
    bcdData.append(hour);

    // 日
    quint8 day = (date.day() / 10) << 4 | (date.day() % 10);
    bcdData.append(day);

    // 月
    quint8 month = (date.month() / 10) << 4 | (date.month() % 10);
    bcdData.append(month);

    // 年（取后两位）
    quint8 year = ((date.year() % 100) / 10) << 4 | (date.year() % 10);
    bcdData.append(year);

    return bcdData;
}

bool BCDHelper::isValidBCD(const QByteArray &bcdData)
{
    for (int i = 0; i < bcdData.size(); ++i)
    {
        quint8 byte = static_cast<quint8>(bcdData[i]);
        quint8 high = (byte >> 4) & 0x0F;
        quint8 low = byte & 0x0F;

        if (high > 9 || low > 9)
        {
            return false;
        }
    }
    return true;
}

QString BCDHelper::bytesToHexString(const QByteArray &data, const QString &separator)
{
    QStringList hexList;
    for (int i = 0; i < data.size(); ++i)
    {
        hexList << QString("%1").arg(static_cast<quint8>(data[i]), 2, 16, QChar('0')).toUpper();
    }
    return hexList.join(separator);
}

QByteArray BCDHelper::hexStringToBytes(const QString &hexString)
{
    QByteArray result;
    QString cleaned = hexString;
    cleaned.remove(' ');
    cleaned.remove('-');
    cleaned.remove(':');

    if (cleaned.length() % 2 != 0)
    {
        cleaned.prepend('0');
    }

    for (int i = 0; i < cleaned.length(); i += 2)
    {
        QString byteStr = cleaned.mid(i, 2);
        bool ok;
        quint8 byte = static_cast<quint8>(byteStr.toInt(&ok, 16));
        if (ok)
        {
            result.append(byte);
        }
    }
    return result;
}

char BCDHelper::convertToBCD(const QString &digitStr)
{
    QString padded = digitStr.left(2).leftJustified(2, '0');
    if (padded.length() >= 2)
    {
        int high = qMax(0, qMin(9, padded[0].digitValue()));
        int low = qMax(0, qMin(9, padded[1].digitValue()));
        return static_cast<char>((high << 4) | low);
    }

    int single = qMax(0, qMin(9, padded[0].digitValue()));
    return static_cast<char>((single << 4) & 0xF0);
}

QString BCDHelper::formatValueToString(const QVariant &value, const QString &format)
{
    qDebug() << "formatValueToString: " << value.toString();

    double numValue = value.toDouble();
    QStringList originalFormatList = format.split(",");

    QString result;

    // 处理F9格式
    int f9Index = originalFormatList.indexOf("F9");
    if (f9Index != -1)
    {
        if (numValue >= 80)
        {
            result += "80";
        }
        else
        {
            result += "00";
        }
    }

    // 获取非F9格式列表
    QStringList nonF9List;
    for (const QString &elem : originalFormatList)
    {
        if (elem != "F9")
        {
            nonF9List.append(elem);
        }
    }

    // 检查是否有小数点
    int decimalIndex = nonF9List.indexOf(".");

    if (decimalIndex != -1)
    {
        // 有小数点，按之前正确的逻辑处理
        int elementsBeforeDecimal = 0;
        for (int i = 0; i < decimalIndex; ++i)
        {
            if (nonF9List[i] != ".")
            {
                elementsBeforeDecimal++;
            }
        }

        int elementsAfterDecimal = 0;
        for (int i = decimalIndex + 1; i < nonF9List.size(); ++i)
        {
            if (nonF9List[i] != ".")
            {
                elementsAfterDecimal++;
            }
        }

        // 分离数值的整数和小数部分
        int intPart = static_cast<int>(numValue);
        double decimalValue = numValue - intPart;

        // 计算需要的位数
        int intDigits = elementsBeforeDecimal * 2;
        int decimalDigits = elementsAfterDecimal * 2;

        // 格式化整数部分
        QString intStr = QString::number(intPart);
        QString formattedInt = intStr.rightJustified(intDigits, '0');
        if (formattedInt.length() > intDigits)
        {
            formattedInt = formattedInt.right(intDigits);
        }

        // 格式化小数部分
        int decimalInt = static_cast<int>(decimalValue * std::pow(10.0, decimalDigits));
        QString decimalStr = QString::number(decimalInt);
        QString formattedDecimal = decimalStr.rightJustified(decimalDigits, '0');
        if (formattedDecimal.length() > decimalDigits)
        {
            formattedDecimal = formattedDecimal.left(decimalDigits);
        }

        // 按格式构建结果
        QString combinedStr = formattedInt + formattedDecimal;
        int strIndex = 0;

        for (const QString &elem : originalFormatList)
        {
            if (elem == "F9")
            {
                continue;
            }

            if (elem == ".")
            {
                result += ".";
            }
            else
            {
                if (strIndex < combinedStr.length())
                {
                    QString digits = combinedStr.mid(strIndex, 2);
                    if (digits.length() < 2)
                    {
                        digits = digits.leftJustified(2, '0');
                    }
                    result += digits;
                    strIndex += 2;
                }
                else
                {
                    result += "00";
                }
            }
        }
    }
    else
    {
        // 没有小数点的情况，但可能有xx格式
        int xxIndex = nonF9List.indexOf("xx");

        if (xxIndex != -1)
        {
            // 计算xx后面有多少位数字
            int digitsAfterXx = 0;
            for (int i = xxIndex + 1; i < nonF9List.size(); ++i)
            {
                if (nonF9List[i] != ".")
                {
                    digitsAfterXx += 2; // 每个元素占2位
                }
            }

            // 乘数 = 10^(后面位数 + 1)
            double multiplier = std::pow(10.0, digitsAfterXx + 1);
            double multipliedValue = numValue * multiplier;
            qint64 intValue = static_cast<qint64>(multipliedValue);

            // 计算总位数
            int totalElements = 0;
            for (const QString &elem : nonF9List)
            {
                if (elem != ".")
                {
                    totalElements++;
                }
            }
            int totalDigits = totalElements * 2;

            // 转换为字符串并右对齐
            QString numStr = QString::number(intValue);
            QString paddedStr = numStr.rightJustified(totalDigits, '0');
            if (paddedStr.length() > totalDigits)
            {
                paddedStr = paddedStr.right(totalDigits);
            }

            // 构建结果
            int digitIndex = 0;
            for (const QString &elem : originalFormatList)
            {
                if (elem == "F9")
                {
                    continue;
                }

                if (elem == "xx")
                {
                    // xx格式：取2位数字，在中间插入小数点
                    if (digitIndex + 2 <= paddedStr.length())
                    {
                        QString digits = paddedStr.mid(digitIndex, 2);
                        result += digits[0]; // 第一位数字
                        result += ".";       // 小数点
                        result += digits[1]; // 第二位数字
                        digitIndex += 2;
                    }
                    else
                    {
                        result += "0.0"; // 不够时的默认值
                    }
                }
                else if (elem == ".")
                {
                    result += ".";
                }
                else
                { // AA或其他格式
                    if (digitIndex < paddedStr.length())
                    {
                        QString digitPair = paddedStr.mid(digitIndex, 2);
                        if (digitPair.length() < 2)
                        {
                            digitPair = digitPair.leftJustified(2, '0');
                        }
                        result += digitPair;
                        digitIndex += 2;
                    }
                    else
                    {
                        result += "00";
                    }
                }
            }
        }
        else
        {
            // 没有xx格式，按普通逻辑处理
            int totalElements = 0;
            for (const QString &elem : nonF9List)
            {
                if (elem != ".")
                {
                    totalElements++;
                }
            }

            // 计算总位数
            int totalDigits = totalElements * 2;
            qint64 intValue = static_cast<qint64>(numValue);

            // 转换为字符串并右对齐
            QString numStr = QString::number(intValue);
            QString paddedStr = numStr.rightJustified(totalDigits, '0');
            if (paddedStr.length() > totalDigits)
            {
                paddedStr = paddedStr.right(totalDigits);
            }

            // 构建结果
            int digitIndex = 0;
            for (const QString &elem : originalFormatList)
            {
                if (elem == "F9")
                {
                    continue;
                }

                if (elem == ".")
                {
                    result += ".";
                }
                else
                { // AA或其他格式
                    if (digitIndex < paddedStr.length())
                    {
                        QString digitPair = paddedStr.mid(digitIndex, 2);
                        if (digitPair.length() < 2)
                        {
                            digitPair = digitPair.leftJustified(2, '0');
                        }
                        result += digitPair;
                        digitIndex += 2;
                    }
                    else
                    {
                        result += "00";
                    }
                }
            }
        }
    }

    return result;
}

QByteArray BCDHelper::stringToByteArray(const QString &str)
{
    QByteArray result;

    // 确保字符串长度为偶数
    QString cleanStr = str;
    if (cleanStr.length() % 2 != 0)
    {
        cleanStr = "0" + cleanStr; // 在前面补0
    }

    // 每两个字符转换为一个字节
    for (int i = 0; i < cleanStr.length(); i += 2)
    {
        QString byteStr = cleanStr.mid(i, 2);
        bool ok;
        quint8 byteValue = byteStr.toUInt(&ok, 16); // 以十六进制解析
        if (ok)
        {
            result.append(static_cast<char>(byteValue));
        }
        else
        {
            // 如果转换失败，默认添加0x00
            result.append(static_cast<char>(0x00));
        }
    }

    return result;
}
