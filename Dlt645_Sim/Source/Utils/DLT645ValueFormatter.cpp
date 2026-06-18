#include "DLT645ValueFormatter.h"
#include "DLT645BlockHandler.h"
#include "DLT645FrameFormatter.h"
#include "DLT645FrameFormatterFields.h"
#include "DLT645FrameFormatterValueDisplay.h"

#include "Models/DataItem.h"
#include "Models/MeterModel.h"
#include "Utils/BCDHelper.h"
#include "Utils/DLT645DataHelper.h"

#include <algorithm>
#include <QMap>

namespace
{
using namespace DLT645FrameFormatterFields;
using namespace DLT645FrameFormatterValueDisplay;

QString formatByteHex(quint8 value)
{
    return QStringLiteral("0x%1").arg(value, 2, 16, QChar('0')).toUpper();
}

QString lineWithValue(const QString &label, const QString &value)
{
    return label + value + QStringLiteral("\n");
}

QString appendUnit(const QString &value, const QString &unit, bool withSpace = true)
{
    if (value.isEmpty())
    {
        return value;
    }

    if (unit.isEmpty() || unit == QStringLiteral("-") || unit == QStringLiteral("none"))
    {
        return value;
    }

    return withSpace ? (value + QStringLiteral(" ") + unit) : (value + unit);
}

QString annotatedLine(const QString &hex, const QString &label, const QString &value = QString())
{
    return value.isEmpty()
               ? QStringLiteral("%1 --- %2").arg(hex, label)
               : QStringLiteral("%1 --- %2:%3").arg(hex, label, value);
}

bool isCoverEventDi(const QString &diCode)
{
    return diCode.startsWith(QStringLiteral("0x03300D")) ||
           diCode.startsWith(QStringLiteral("0x03300E"));
}

QString inferredUnitForDi(const QString &diCode, int fieldIndex)
{
    if (isCoverEventDi(diCode) && isHistoryItem(diCode) && fieldIndex >= 2)
    {
        const int energyIndex = (fieldIndex - 2) % 6;
        return energyIndex < 2 ? QStringLiteral("kWh") : QStringLiteral("kvarh");
    }

    const bool isPhasePowerEvent = (diCode.startsWith(QStringLiteral("0x0301")) ||
                                    diCode.startsWith(QStringLiteral("0x0302")) ||
                                    diCode.startsWith(QStringLiteral("0x0303")) ||
                                    diCode.startsWith(QStringLiteral("0x0304")) ||
                                    diCode.startsWith(QStringLiteral("0x030B")) ||
                                    diCode.startsWith(QStringLiteral("0x030C")) ||
                                    diCode.startsWith(QStringLiteral("0x030D"))) &&
                                   diCode.mid(6, 2) != QStringLiteral("00") &&
                                   isHistoryItem(diCode);
    if (isPhasePowerEvent)
    {
        if (fieldIndex >= 2 && fieldIndex < 6)
        {
            const int energyIndex = fieldIndex - 2;
            return energyIndex < 2 ? QStringLiteral("kWh") : QStringLiteral("kvarh");
        }
        if (fieldIndex >= 6)
        {
            const int blockIndex = (fieldIndex - 6) % 9;
            if (blockIndex < 4)
            {
                return blockIndex < 2 ? QStringLiteral("kWh") : QStringLiteral("kvarh");
            }
            if (blockIndex == 4)
            {
                return QStringLiteral("V");
            }
            if (blockIndex == 5)
            {
                return QStringLiteral("A");
            }
            if (blockIndex == 6)
            {
                return QStringLiteral("kW");
            }
            if (blockIndex == 7)
            {
                return QStringLiteral("kvar");
            }
        }
    }

    const bool isUnbalanceEvent = (diCode.startsWith(QStringLiteral("0x030900")) ||
                                   diCode.startsWith(QStringLiteral("0x030A00"))) &&
                                  isHistoryItem(diCode);
    const bool isSequenceEvent = (diCode.startsWith(QStringLiteral("0x030700")) ||
                                  diCode.startsWith(QStringLiteral("0x030800")) ||
                                  ((diCode.startsWith(QStringLiteral("0x030E")) ||
                                    diCode.startsWith(QStringLiteral("0x030F"))) &&
                                   diCode.mid(6, 2) != QStringLiteral("00"))) &&
                                 isHistoryItem(diCode);
    if (isUnbalanceEvent || isSequenceEvent)
    {
        const int firstEnergyField = isUnbalanceEvent ? 3 : 2;
        if (fieldIndex >= firstEnergyField)
        {
            const int energyIndex = (fieldIndex - firstEnergyField) % 4;
            return energyIndex < 2 ? QStringLiteral("kWh") : QStringLiteral("kvarh");
        }
    }

    return QString();
}

} // anonymous namespace

namespace DLT645ValueFormatter
{

QString decodedValueForItem(const MeterModel *meterModel,
                            const QString &diCode,
                            const QByteArray &payload,
                            QString *descriptionOut,
                            QString *unitOut)
{
    QString decoded = DLT645DataHelper::decodePayloadForItem(meterModel, diCode, payload, descriptionOut, unitOut);

    if (!decoded.isEmpty() && decoded != BCDHelper::bytesToHexString(payload))
    {
        return decoded;
    }

    const DataItem item = meterModel ? meterModel->getDataItem(diCode) : DataItem();
    if (!item.diCode.isEmpty() && BCDHelper::isValidBCD(payload))
    {
        const QString rawDigits = BCDHelper::bytesToHexString(payload, QString());
        return BCDHelper::bcdStringToDecimal(rawDigits, item.decimalPlaces);
    }

    return decoded;
}

QString blockDisplayValueForItem(const MeterModel *meterModel,
                                 const QString &diCode,
                                 const QByteArray &payload,
                                 QString *descriptionOut,
                                 QString *unitOut)
{
    const DataItem item = meterModel ? meterModel->getDataItem(diCode) : DataItem();
    if (!item.diCode.isEmpty())
    {
        if (descriptionOut)
        {
            *descriptionOut = item.description;
        }
        if (unitOut)
        {
            *unitOut = item.unit;
        }

        const QString formattedValue = displayValueForFormatPart(payload, item.format);
        if (!formattedValue.isEmpty())
        {
            return formattedValue;
        }
    }

    return decodedValueForItem(meterModel, diCode, payload, descriptionOut, unitOut);
}

QString byteBreakdownLines(const QString &diCode,
                           const QByteArray &payload,
                           const QString &format,
                           const QString &fallbackDecodedValue,
                           const QString &unit,
                           const QString &fallbackFieldName)
{
    const QStringList fieldNames = fieldNamesForDecodedValue(diCode);
    const QStringList formatParts = format.split(QStringLiteral(":"), Qt::SkipEmptyParts);
    if (formatParts.isEmpty())
    {
        return decodedValueWithFieldDescriptions(diCode, appendUnit(fallbackDecodedValue, unit));
    }

    QStringList lines;
    int offset = 0;
    for (int i = 0; i < formatParts.size(); ++i)
    {
        const int size = DLT645DataHelper::bytesRequiredForFormat(formatParts.at(i));
        if (size <= 0 || offset + size > payload.size())
        {
            return decodedValueWithFieldDescriptions(diCode, appendUnit(fallbackDecodedValue, unit));
        }

        const QByteArray fieldBytes = payload.mid(offset, size);
        const QString fieldName = i < fieldNames.size()
                                      ? fieldNames.at(i)
                                      : (formatParts.size() == 1
                                             ? (fallbackFieldName.isEmpty() ? QStringLiteral("数据值") : fallbackFieldName)
                                             : QStringLiteral("字段%1").arg(i + 1));
        QString fieldValue = displayParameterValueForItem(diCode, fieldBytes);
        if (fieldValue.isEmpty())
        {
            fieldValue = displayValueForFormatPart(fieldBytes, formatParts.at(i));
        }
        if (fieldValue.isEmpty())
        {
            fieldValue = DLT645FrameFormatter::formatHex(fieldBytes);
        }

        const QString fieldUnit = !inferredUnitForDi(diCode, i).isEmpty()
                                      ? inferredUnitForDi(diCode, i)
                                      : unit;
        lines.append(QStringLiteral("%1 --- %2:%3")
                         .arg(DLT645FrameFormatter::formatHex(fieldBytes),
                              fieldName,
                              appendUnit(fieldValue, fieldUnit)));
        offset += size;
    }

    if (offset < payload.size())
    {
        lines.append(annotatedLine(DLT645FrameFormatter::formatHex(payload.mid(offset)),
                                   QStringLiteral("未匹配剩余数据")));
    }

    return lines.join(QStringLiteral("\n"));
}

QString blockByteBreakdownLines(const MeterModel *meterModel, const QString &blockDiCode, const QByteArray &payload)
{
    if (!meterModel || blockDiCode.isEmpty() || payload.isEmpty())
    {
        return QString();
    }

    int rateCountOverride = -1;
    const QStringList payloadCodes = DLT645BlockHandler::selectPayloadOrderCodesForBlock(meterModel, blockDiCode, payload, &rateCountOverride);
    Q_UNUSED(rateCountOverride);
    if (payloadCodes.isEmpty())
    {
        return QString();
    }

    QStringList lines;
    int offset = 0;
    for (const QString &code : payloadCodes)
    {
        DataItem item = meterModel->getDataItem(code);
        if (item.diCode.isEmpty())
        {
            return QString();
        }

        const int size = DLT645DataHelper::bytesRequiredForFormat(item.format);
        if (size <= 0 || offset + size > payload.size())
        {
            return QString();
        }

        QString description;
        QString unit;
        const QByteArray fieldBytes = payload.mid(offset, size);
        const QString value = blockDisplayValueForItem(meterModel, code, fieldBytes, &description, &unit);
        if (description.isEmpty())
        {
            description = item.description;
        }
        if (unit.isEmpty())
        {
            unit = item.unit;
        }

        lines.append(QStringLiteral("%1 --- %2:%3")
                         .arg(DLT645FrameFormatter::formatHex(fieldBytes),
                              description.isEmpty() ? code : description,
                              appendUnit(value.isEmpty() ? DLT645FrameFormatter::formatHex(fieldBytes) : value, unit)));
        offset += size;
    }

    if (offset != payload.size())
    {
        return QString();
    }

    return lines.join(QStringLiteral("\n"));
}

QList<FrameParseCard> buildBlockDecodeCards(const MeterModel *meterModel, const QString &blockDiCode, const QByteArray &payload)
{
    if (!meterModel || blockDiCode.isEmpty() || payload.isEmpty())
    {
        return {};
    }

    int rateCountOverride = -1;
    const QStringList payloadCodes = DLT645BlockHandler::selectPayloadOrderCodesForBlock(meterModel, blockDiCode, payload, &rateCountOverride);
    if (payloadCodes.isEmpty())
    {
        return {};
    }

    QMap<QString, QString> payloadHexByCode;
    QMap<QString, QString> valuesByCode;
    QMap<QString, QString> descriptionsByCode;
    QMap<QString, QString> unitsByCode;

    int offset = 0;
    for (const QString &code : payloadCodes)
    {
        const DataItem item = meterModel->getDataItem(code);
        if (item.diCode.isEmpty())
        {
            return {};
        }

        const int size = DLT645DataHelper::bytesRequiredForFormat(item.format);
        if (size <= 0 || offset + size > payload.size())
        {
            return {};
        }

        const QByteArray partPayload = payload.mid(offset, size);
        QByteArray displayBytes = partPayload;
        std::reverse(displayBytes.begin(), displayBytes.end());

        QString description;
        QString unit;
        const QString value = blockDisplayValueForItem(meterModel, code, partPayload, &description, &unit);

        payloadHexByCode.insert(code, DLT645FrameFormatter::formatHex(displayBytes));
        valuesByCode.insert(code, value);
        descriptionsByCode.insert(code, description.isEmpty() ? item.description : description);
        unitsByCode.insert(code, unit.isEmpty() ? item.unit : unit);
        offset += size;
    }

    if (offset != payload.size())
    {
        return {};
    }

    const QStringList displayCodes = DLT645BlockHandler::displayOrderCodesForBlock(meterModel, blockDiCode, rateCountOverride);
    QList<FrameParseCard> cards;
    for (const QString &code : displayCodes)
    {
        if (!valuesByCode.contains(code))
        {
            continue;
        }

        FrameParseCard card;
        card.title = descriptionsByCode.value(code, code);
        card.value = appendUnit(valuesByCode.value(code), unitsByCode.value(code), false);
        card.detail = QString::fromUtf8("DI: %1\n原始HEX: %2").arg(code, payloadHexByCode.value(code));
        card.valueMonospace = false;
        card.detailMonospace = false;
        cards.append(card);
    }

    return cards;
}

} // namespace DLT645ValueFormatter
