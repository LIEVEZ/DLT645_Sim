#include "DLT645DataHelper.h"
#include "Models/DataItem.h"
#include "Models/MeterModel.h"
#include "Services/ProtocolParser.h"
#include "Utils/BCDHelper.h"
#include <algorithm>

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

    if (diCode == "0x04000000" && payload.size() == 6)
    {
        return ProtocolParser::bytesToAddress(payload);
    }

    const QString decoded = decodePayloadByFormat(payload, dataItem.format, dataItem.decimalPlaces);
    if (!decoded.isEmpty())
    {
        return decoded;
    }

    return BCDHelper::bytesToHexString(payload);
}
