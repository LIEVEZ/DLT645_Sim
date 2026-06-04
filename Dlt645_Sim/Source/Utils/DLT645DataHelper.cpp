#include "DLT645DataHelper.h"
#include "Models/DataItem.h"
#include "Models/MeterModel.h"
#include "Services/ProtocolParser.h"
#include "Utils/BCDHelper.h"
#include <algorithm>

namespace
{
int bcdByteToInt(quint8 byte, bool *ok = nullptr)
{
    const int high = (byte >> 4) & 0x0F;
    const int low = byte & 0x0F;
    const bool valid = high <= 9 && low <= 9;
    if (ok)
    {
        *ok = valid;
    }
    return high * 10 + low;
}

QString decodeTimeSeconds(const QByteArray &payload)
{
    if (payload.size() != 3)
    {
        return QString();
    }

    bool ok = true;
    const int second = bcdByteToInt(static_cast<quint8>(payload[0]), &ok);
    const int minute = ok ? bcdByteToInt(static_cast<quint8>(payload[1]), &ok) : 0;
    const int hour = ok ? bcdByteToInt(static_cast<quint8>(payload[2]), &ok) : 0;
    if (!ok || hour > 23 || minute > 59 || second > 59)
    {
        return QString();
    }

    return QStringLiteral("%1时%2分%3秒")
        .arg(hour, 2, 10, QChar('0'))
        .arg(minute, 2, 10, QChar('0'))
        .arg(second, 2, 10, QChar('0'));
}

QString decodeTimeMinutes(const QByteArray &payload)
{
    if (payload.size() != 2)
    {
        return QString();
    }

    bool ok = true;
    const int minute = bcdByteToInt(static_cast<quint8>(payload[0]), &ok);
    const int hour = ok ? bcdByteToInt(static_cast<quint8>(payload[1]), &ok) : 0;
    if (!ok || hour > 23 || minute > 59)
    {
        return QString();
    }

    return QStringLiteral("%1时%2分")
        .arg(hour, 2, 10, QChar('0'))
        .arg(minute, 2, 10, QChar('0'));
}

QString decodeDateTimeMinutes(const QByteArray &payload)
{
    if (payload.size() != 5)
    {
        return QString();
    }

    bool ok = true;
    const int minute = bcdByteToInt(static_cast<quint8>(payload[0]), &ok);
    const int hour = ok ? bcdByteToInt(static_cast<quint8>(payload[1]), &ok) : 0;
    const int day = ok ? bcdByteToInt(static_cast<quint8>(payload[2]), &ok) : 0;
    const int month = ok ? bcdByteToInt(static_cast<quint8>(payload[3]), &ok) : 0;
    const int year = ok ? bcdByteToInt(static_cast<quint8>(payload[4]), &ok) : 0;
    if (!ok || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59)
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

QString decodeDateWithWeek(const QByteArray &payload)
{
    if (payload.size() != 4)
    {
        return QString();
    }

    bool ok = true;
    const int week = bcdByteToInt(static_cast<quint8>(payload[0]), &ok);
    const int day = ok ? bcdByteToInt(static_cast<quint8>(payload[1]), &ok) : 0;
    const int month = ok ? bcdByteToInt(static_cast<quint8>(payload[2]), &ok) : 0;
    const int year = ok ? bcdByteToInt(static_cast<quint8>(payload[3]), &ok) : 0;
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

QString decodeMonthDayTime(const QByteArray &payload)
{
    if (payload.size() != 4)
    {
        return QString();
    }

    bool ok = true;
    const int minute = bcdByteToInt(static_cast<quint8>(payload[0]), &ok);
    const int hour = ok ? bcdByteToInt(static_cast<quint8>(payload[1]), &ok) : 0;
    const int day = ok ? bcdByteToInt(static_cast<quint8>(payload[2]), &ok) : 0;
    const int month = ok ? bcdByteToInt(static_cast<quint8>(payload[3]), &ok) : 0;
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

QString decodeSettlementDay(const QByteArray &payload)
{
    if (payload.size() != 2)
    {
        return QString();
    }

    bool ok = true;
    const int hour = bcdByteToInt(static_cast<quint8>(payload[0]), &ok);
    const int day = ok ? bcdByteToInt(static_cast<quint8>(payload[1]), &ok) : 0;
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

QString decodeParameterPayload(const QString &diCode, const QByteArray &payload)
{
    if (diCode == QStringLiteral("0x04000101"))
    {
        return decodeDateWithWeek(payload);
    }
    if (diCode == QStringLiteral("0x04000102"))
    {
        return decodeTimeSeconds(payload);
    }
    if (diCode == QStringLiteral("0x04000106") ||
        diCode == QStringLiteral("0x04000107") ||
        diCode == QStringLiteral("0x04000108") ||
        diCode == QStringLiteral("0x04000109") ||
        diCode == QStringLiteral("0x04001201"))
    {
        return decodeDateTimeMinutes(payload);
    }
    if (diCode == QStringLiteral("0x04000A01"))
    {
        return decodeMonthDayTime(payload);
    }
    if (diCode == QStringLiteral("0x04001203"))
    {
        return decodeTimeMinutes(payload);
    }
    if (diCode == QStringLiteral("0x04000B01") ||
        diCode == QStringLiteral("0x04000B02") ||
        diCode == QStringLiteral("0x04000B03"))
    {
        return decodeSettlementDay(payload);
    }

    return QString();
}
}

QByteArray DLT645DataHelper::stripLeadingFE(const QByteArray &frame, int *leadingCount)
{
    int count = 0;
    while (count < frame.size() && static_cast<quint8>(frame[count]) == 0xFE)
    {
        ++count;
    }

    if (leadingCount)
    {
        *leadingCount = count;
    }

    return frame.mid(count);
}

QByteArray DLT645DataHelper::subtract33(const QByteArray &data)
{
    QByteArray result = data;
    for (int i = 0; i < result.size(); ++i)
    {
        result[i] = static_cast<char>(static_cast<quint8>(result[i]) - 0x33);
    }
    return result;
}

QByteArray DLT645DataHelper::decodeReversedPayload(const QByteArray &data)
{
    QByteArray result = data;
    std::reverse(result.begin(), result.end());
    return subtract33(result);
}

int DLT645DataHelper::bytesRequiredForFormat(const QString &format)
{
    const QStringList sections = format.split(":", Qt::SkipEmptyParts);
    if (sections.size() > 1)
    {
        int total = 0;
        for (const QString &section : sections)
        {
            total += bytesRequiredForFormat(section);
        }
        return total;
    }

    const QStringList tokens = format.split(",", Qt::SkipEmptyParts);
    int count = 0;
    for (const QString &token : tokens)
    {
        if (token.trimmed() != ".")
        {
            ++count;
        }
    }
    return count;
}

QString DLT645DataHelper::formatDecimalValue(const QByteArray &payload, int decimalPlaces)
{
    if (payload.isEmpty() || !BCDHelper::isValidBCD(payload))
    {
        return QString();
    }

    const double value = BCDHelper::bcdToDecimal(payload, decimalPlaces);
    if (decimalPlaces > 0)
    {
        return QString::number(value, 'f', decimalPlaces);
    }

    return QString::number(static_cast<qint64>(value));
}

QString DLT645DataHelper::decodePayloadByFormat(const QByteArray &payload, const QString &format, int decimalPlaces)
{
    if (payload.isEmpty())
    {
        return QString();
    }

    const QStringList parts = format.split(":", Qt::SkipEmptyParts);
    if (parts.size() > 1)
    {
        QStringList decodedParts;
        int offset = 0;
        for (const QString &part : parts)
        {
            const int size = bytesRequiredForFormat(part);
            if (size <= 0 || offset + size > payload.size())
            {
                return BCDHelper::bytesToHexString(payload);
            }

            decodedParts.append(decodePayloadByFormat(payload.mid(offset, size), part, decimalPlaces));
            offset += size;
        }

        if (offset != payload.size())
        {
            return BCDHelper::bytesToHexString(payload);
        }

        return decodedParts.join(" : ");
    }

    const QString signedDecoded = BCDHelper::decodeValueFromFormat(payload, format);
    if (!signedDecoded.isEmpty())
    {
        return signedDecoded;
    }

    return formatDecimalValue(payload, decimalPlaces);
}

QString DLT645DataHelper::decodePayloadForItem(const MeterModel *meterModel,
                                               const QString &diCode,
                                               const QByteArray &payload,
                                               QString *descriptionOut,
                                               QString *unitOut)
{
    if (descriptionOut)
    {
        descriptionOut->clear();
    }
    if (unitOut)
    {
        unitOut->clear();
    }

    if (payload.isEmpty())
    {
        return QString();
    }

    if (!meterModel || diCode.isEmpty())
    {
        return BCDHelper::bytesToHexString(payload);
    }

    const DataItem dataItem = meterModel->getDataItem(diCode);
    if (!dataItem.diCode.isEmpty())
    {
        if (descriptionOut)
        {
            *descriptionOut = dataItem.description;
        }
        if (unitOut)
        {
            *unitOut = dataItem.unit;
        }
    }

    if (diCode == "0x04000401" && payload.size() == 6)
    {
        return ProtocolParser::bytesToAddress(payload);
    }

    const QString parameterDecoded = decodeParameterPayload(diCode, payload);
    if (!parameterDecoded.isEmpty())
    {
        return parameterDecoded;
    }

    const QString decoded = decodePayloadByFormat(payload, dataItem.format, dataItem.decimalPlaces);
    if (!decoded.isEmpty())
    {
        return decoded;
    }

    return BCDHelper::bytesToHexString(payload);
}
