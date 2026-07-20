#ifndef DLT645FRAMEFORMATTERVALUEDISPLAY_H
#define DLT645FRAMEFORMATTERVALUEDISPLAY_H

#include "Utils/BCDHelper.h"

#include <algorithm>
#include <QByteArray>
#include <QString>
#include <QStringList>

namespace DLT645FrameFormatterValueDisplay
{
inline int bcdByteToInt(quint8 value, bool *ok = nullptr)
{
    const int high = (value >> 4) & 0x0F;
    const int low = value & 0x0F;
    const bool valid = high <= 9 && low <= 9;
    if (ok)
    {
        *ok = valid;
    }
    return high * 10 + low;
}

inline QString displayBcdDigits(const QByteArray &bytes, bool reverseBytes)
{
    QByteArray normalized = bytes;
    if (reverseBytes)
    {
        std::reverse(normalized.begin(), normalized.end());
    }

    QString digits;
    digits.reserve(normalized.size() * 2);
    for (int i = 0; i < normalized.size(); ++i)
    {
        const quint8 byte = static_cast<quint8>(normalized[i]);
        digits += QStringLiteral("%1").arg(byte, 2, 16, QChar('0')).toUpper();
    }
    return digits;
}

inline QString displayDateTimeSeconds(const QByteArray &bytes)
{
    if (bytes.size() != 6)
    {
        return QString();
    }

    bool ok = true;
    const int second = bcdByteToInt(static_cast<quint8>(bytes[0]), &ok);
    const int minute = ok ? bcdByteToInt(static_cast<quint8>(bytes[1]), &ok) : 0;
    const int hour = ok ? bcdByteToInt(static_cast<quint8>(bytes[2]), &ok) : 0;
    const int day = ok ? bcdByteToInt(static_cast<quint8>(bytes[3]), &ok) : 0;
    const int month = ok ? bcdByteToInt(static_cast<quint8>(bytes[4]), &ok) : 0;
    const int year = ok ? bcdByteToInt(static_cast<quint8>(bytes[5]), &ok) : 0;
    if (!ok)
    {
        return QString();
    }

    return QStringLiteral("%1年%2月%3日 %4时%5分%6秒")
        .arg(year, 2, 10, QChar('0'))
        .arg(month, 2, 10, QChar('0'))
        .arg(day, 2, 10, QChar('0'))
        .arg(hour, 2, 10, QChar('0'))
        .arg(minute, 2, 10, QChar('0'))
        .arg(second, 2, 10, QChar('0'));
}

inline QString displayDateTimeMinutes(const QByteArray &bytes)
{
    if (bytes.size() != 5)
    {
        return QString();
    }

    bool ok = true;
    const int minute = bcdByteToInt(static_cast<quint8>(bytes[0]), &ok);
    const int hour = ok ? bcdByteToInt(static_cast<quint8>(bytes[1]), &ok) : 0;
    const int day = ok ? bcdByteToInt(static_cast<quint8>(bytes[2]), &ok) : 0;
    const int month = ok ? bcdByteToInt(static_cast<quint8>(bytes[3]), &ok) : 0;
    const int year = ok ? bcdByteToInt(static_cast<quint8>(bytes[4]), &ok) : 0;
    if (!ok)
    {
        return QString();
    }

    return QStringLiteral("%1年%2月%3日 %4时%5分")
        .arg(year, 2, 10, QChar('0'))
        .arg(month, 2, 10, QChar('0'))
        .arg(day, 2, 10, QChar('0'))
        .arg(hour, 2, 10, QChar('0'))
        .arg(minute, 2, 10, QChar('0'));
}

inline QString displayTimeSeconds(const QByteArray &bytes)
{
    if (bytes.size() != 3)
    {
        return QString();
    }

    bool ok = true;
    const int second = bcdByteToInt(static_cast<quint8>(bytes[0]), &ok);
    const int minute = ok ? bcdByteToInt(static_cast<quint8>(bytes[1]), &ok) : 0;
    const int hour = ok ? bcdByteToInt(static_cast<quint8>(bytes[2]), &ok) : 0;
    if (!ok || hour > 23 || minute > 59 || second > 59)
    {
        return QString();
    }

    return QStringLiteral("%1时%2分%3秒")
        .arg(hour, 2, 10, QChar('0'))
        .arg(minute, 2, 10, QChar('0'))
        .arg(second, 2, 10, QChar('0'));
}

inline QString displayTimeMinutes(const QByteArray &bytes)
{
    if (bytes.size() != 2)
    {
        return QString();
    }

    bool ok = true;
    const int minute = bcdByteToInt(static_cast<quint8>(bytes[0]), &ok);
    const int hour = ok ? bcdByteToInt(static_cast<quint8>(bytes[1]), &ok) : 0;
    if (!ok || hour > 23 || minute > 59)
    {
        return QString();
    }

    return QStringLiteral("%1时%2分")
        .arg(hour, 2, 10, QChar('0'))
        .arg(minute, 2, 10, QChar('0'));
}

inline QString displayDateWithWeek(const QByteArray &bytes)
{
    if (bytes.size() != 4)
    {
        return QString();
    }

    bool ok = true;
    const int week = bcdByteToInt(static_cast<quint8>(bytes[0]), &ok);
    const int day = ok ? bcdByteToInt(static_cast<quint8>(bytes[1]), &ok) : 0;
    const int month = ok ? bcdByteToInt(static_cast<quint8>(bytes[2]), &ok) : 0;
    const int year = ok ? bcdByteToInt(static_cast<quint8>(bytes[3]), &ok) : 0;
    if (!ok || month < 1 || month > 12 || day < 1 || day > 31 || week > 6)
    {
        return QString();
    }

    const QStringList weekNames = {
        QStringLiteral("日"),
        QStringLiteral("一"),
        QStringLiteral("二"),
        QStringLiteral("三"),
        QStringLiteral("四"),
        QStringLiteral("五"),
        QStringLiteral("六")};

    return QStringLiteral("%1年%2月%3日 星期%4")
        .arg(year, 2, 10, QChar('0'))
        .arg(month, 2, 10, QChar('0'))
        .arg(day, 2, 10, QChar('0'))
        .arg(weekNames.at(week));
}

inline QString displayMonthDayTime(const QByteArray &bytes)
{
    if (bytes.size() != 4)
    {
        return QString();
    }

    bool ok = true;
    const int minute = bcdByteToInt(static_cast<quint8>(bytes[0]), &ok);
    const int hour = ok ? bcdByteToInt(static_cast<quint8>(bytes[1]), &ok) : 0;
    const int day = ok ? bcdByteToInt(static_cast<quint8>(bytes[2]), &ok) : 0;
    const int month = ok ? bcdByteToInt(static_cast<quint8>(bytes[3]), &ok) : 0;
    if (!ok || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59)
    {
        return QString();
    }

    return QStringLiteral("%1月%2日 %3时%4分")
        .arg(month, 2, 10, QChar('0'))
        .arg(day, 2, 10, QChar('0'))
        .arg(hour, 2, 10, QChar('0'))
        .arg(minute, 2, 10, QChar('0'));
}

inline QString displaySettlementDay(const QByteArray &bytes)
{
    if (bytes.size() != 2)
    {
        return QString();
    }

    bool ok = true;
    const int hour = bcdByteToInt(static_cast<quint8>(bytes[0]), &ok);
    const int day = ok ? bcdByteToInt(static_cast<quint8>(bytes[1]), &ok) : 0;
    if (!ok)
    {
        return QString();
    }
    if (day == 99 && hour == 99)
    {
        return QStringLiteral("不启用");
    }
    if (day < 1 || day > 31 || hour > 23)
    {
        return QString();
    }

    return QStringLiteral("%1日%2时")
        .arg(day, 2, 10, QChar('0'))
        .arg(hour, 2, 10, QChar('0'));
}

inline QString displayDecimalFromBcd(const QByteArray &bytes, int decimalDigits)
{
    if (bytes.isEmpty() || !BCDHelper::isValidBCD(bytes))
    {
        return QString();
    }

    QString digits = displayBcdDigits(bytes, true);
    if (decimalDigits > 0)
    {
        if (digits.length() <= decimalDigits)
        {
            digits = digits.rightJustified(decimalDigits + 1, QChar('0'));
        }
        digits.insert(digits.length() - decimalDigits, QChar('.'));
    }
    return digits;
}

inline int decimalDigitsForFormat(const QString &format)
{
    const QStringList tokens = format.split(QStringLiteral(","), Qt::SkipEmptyParts);
    int decimalIndex = -1;
    for (int i = 0; i < tokens.size(); ++i)
    {
        if (tokens.at(i).trimmed() == QStringLiteral("."))
        {
            decimalIndex = i;
            break;
        }
    }
    if (decimalIndex < 0)
    {
        return 0;
    }

    int digits = 0;
    for (int i = decimalIndex + 1; i < tokens.size(); ++i)
    {
        if (tokens.at(i).trimmed() != QStringLiteral("."))
        {
            digits += 2;
        }
    }
    return digits;
}

inline int decimalDigitsForXxFormat(const QString &format)
{
    const QStringList tokens = format.split(QStringLiteral(","), Qt::SkipEmptyParts);
    int xxIndex = -1;
    for (int i = 0; i < tokens.size(); ++i)
    {
        if (tokens.at(i).trimmed() == QStringLiteral("xx"))
        {
            xxIndex = i;
            break;
        }
    }
    if (xxIndex < 0)
    {
        return -1;
    }

    int digitsAfterXx = 0;
    for (int i = xxIndex + 1; i < tokens.size(); ++i)
    {
        if (tokens.at(i).trimmed() != QStringLiteral("."))
        {
            digitsAfterXx += 2;
        }
    }
    return digitsAfterXx + 1;
}

inline QString displaySignedF9DecimalFromLittleEndianBcd(const QByteArray &bytes, const QString &format)
{
    if (bytes.isEmpty())
    {
        return QString();
    }

    QByteArray normalized = bytes;
    std::reverse(normalized.begin(), normalized.end());

    bool negative = false;
    quint8 signByte = static_cast<quint8>(normalized[0]);
    if (signByte >= 0x80)
    {
        negative = true;
        signByte = static_cast<quint8>(signByte - 0x80);
        normalized[0] = static_cast<char>(signByte);
    }

    if (!BCDHelper::isValidBCD(normalized))
    {
        return QString();
    }

    QString digits = displayBcdDigits(normalized, false);
    int decimalDigits = decimalDigitsForFormat(format);
    if (decimalDigits <= 0 && format.contains(QStringLiteral("xx")))
    {
        decimalDigits = decimalDigitsForXxFormat(format);
    }
    if (decimalDigits > 0)
    {
        if (digits.length() <= decimalDigits)
        {
            digits = digits.rightJustified(decimalDigits + 1, QChar('0'));
        }
        digits.insert(digits.length() - decimalDigits, QChar('.'));
    }

    if (format.contains(QStringLiteral("xx")))
    {
        bool ok = false;
        const double value = digits.toDouble(&ok);
        if (ok)
        {
            const double signedValue = negative ? -value : value;
            return QString::number(signedValue, 'f', qMax(0, decimalDigits));
        }
    }

    return negative ? QStringLiteral("-%1").arg(digits) : digits;
}

inline QString displayValueForFormatPart(const QByteArray &bytes, const QString &formatPart)
{
    const QString trimmedFormat = formatPart.trimmed();
    if (trimmedFormat.startsWith(QStringLiteral("F9")))
    {
        return displaySignedF9DecimalFromLittleEndianBcd(bytes, trimmedFormat);
    }
    if (trimmedFormat.startsWith(QStringLiteral("PF")))
    {
        return displaySignedF9DecimalFromLittleEndianBcd(bytes, trimmedFormat);
    }
    if (trimmedFormat == QStringLiteral("AA,AA,AA,AA,AA,AA"))
    {
        const QString dateTime = displayDateTimeSeconds(bytes);
        if (!dateTime.isEmpty())
        {
            return dateTime;
        }
    }
    if (trimmedFormat == QStringLiteral("AA,AA,AA,AA,AA"))
    {
        const QString dateTime = displayDateTimeMinutes(bytes);
        if (!dateTime.isEmpty())
        {
            return dateTime;
        }
    }
    if (trimmedFormat.contains(QStringLiteral("xx")))
    {
        const int decimalDigits = decimalDigitsForXxFormat(trimmedFormat);
        return decimalDigits >= 0 ? displayDecimalFromBcd(bytes, decimalDigits) : QString();
    }
    if (trimmedFormat.contains(QChar('.')))
    {
        return displayDecimalFromBcd(bytes, qMax(0, decimalDigitsForFormat(trimmedFormat)));
    }
    if (trimmedFormat == QStringLiteral("AA,AA,AA,AA"))
    {
        return displayBcdDigits(bytes, true);
    }

    return displayDecimalFromBcd(bytes, 0);
}

inline QString displayParameterValueForItem(const QString &diCode, const QByteArray &bytes)
{
    if (diCode == QStringLiteral("0x04000101"))
    {
        return displayDateWithWeek(bytes);
    }
    if (diCode == QStringLiteral("0x04000102"))
    {
        return displayTimeSeconds(bytes);
    }
    if (diCode == QStringLiteral("0x04000106") ||
        diCode == QStringLiteral("0x04000107") ||
        diCode == QStringLiteral("0x04000108") ||
        diCode == QStringLiteral("0x04000109") ||
        diCode == QStringLiteral("0x04001201"))
    {
        return displayDateTimeMinutes(bytes);
    }
    if (diCode == QStringLiteral("0x04000A01"))
    {
        return displayMonthDayTime(bytes);
    }
    if (diCode == QStringLiteral("0x04001203"))
    {
        return displayTimeMinutes(bytes);
    }
    if (diCode == QStringLiteral("0x04000B01") ||
        diCode == QStringLiteral("0x04000B02") ||
        diCode == QStringLiteral("0x04000B03"))
    {
        return displaySettlementDay(bytes);
    }

    return QString();
}
}

#endif
