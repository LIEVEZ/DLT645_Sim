#include "DLT645FrameFormatter.h"

#include "Models/DataItem.h"
#include "Models/MeterModel.h"
#include "Services/ProtocolParser.h"
#include "Utils/BCDHelper.h"
#include "Utils/ChecksumHelper.h"
#include "Utils/DLT645DataHelper.h"

#include <algorithm>
#include <QMap>
#include <QStringList>

namespace
{
constexpr int kDefaultRateCount = 5;
constexpr int kMaxRateCount = kDefaultRateCount;
constexpr int kHarmonicOrderCount = 21;

QString formatByteHex(quint8 value)
{
    return QStringLiteral("0x%1").arg(value, 2, 16, QChar('0')).toUpper();
}

QString upperHexByte(int value)
{
    return QStringLiteral("%1").arg(value, 2, 16, QChar('0')).toUpper();
}

bool isHarmonicBlockCode(const QString &diCode, const QString &familyPrefix)
{
    if (diCode.size() != 10 || !diCode.startsWith(familyPrefix) || !diCode.endsWith(QStringLiteral("FF")))
    {
        return false;
    }

    bool ok = false;
    const int phaseIndex = diCode.mid(6, 2).toInt(&ok, 16);
    return ok && phaseIndex >= 1 && phaseIndex <= 3;
}

QStringList harmonicPayloadOrderCodes(const QString &blockCode)
{
    const QString prefix = blockCode.left(8);
    QStringList codes;
    codes.reserve(kHarmonicOrderCount);
    for (int harmonicOrder = kHarmonicOrderCount; harmonicOrder >= 1; --harmonicOrder)
    {
        codes.append(prefix + upperHexByte(harmonicOrder));
    }
    return codes;
}

QStringList harmonicDisplayOrderCodes(const QString &blockCode)
{
    const QString prefix = blockCode.left(8);
    QStringList codes;
    codes.reserve(kHarmonicOrderCount);
    for (int harmonicOrder = 1; harmonicOrder <= kHarmonicOrderCount; ++harmonicOrder)
    {
        codes.append(prefix + upperHexByte(harmonicOrder));
    }
    return codes;
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

bool calculateChecksumValidity(const QByteArray &frame)
{
    const QByteArray protocolFrame = DLT645DataHelper::stripLeadingFE(frame);
    if (protocolFrame.size() < 12)
    {
        return false;
    }

    const int declaredLength = static_cast<quint8>(protocolFrame[9]);
    const int expectedSize = 12 + declaredLength;
    if (protocolFrame.size() < expectedSize)
    {
        return false;
    }

    const quint8 checksum = static_cast<quint8>(protocolFrame[10 + declaredLength]);
    const QByteArray checksumRange = protocolFrame.left(10 + declaredLength);
    return ChecksumHelper::calculateChecksum(checksumRange) == checksum;
}

FrameParseCard makeCard(const QString &title,
                        const QString &value,
                        const QString &detail = QString(),
                        bool valueMonospace = false,
                        bool detailMonospace = false)
{
    FrameParseCard card;
    card.title = title;
    card.value = value;
    card.detail = detail;
    card.valueMonospace = valueMonospace;
    card.detailMonospace = detailMonospace;
    return card;
}

void appendCard(FrameParseSection &section,
                const QString &title,
                const QString &value,
                const QString &detail = QString(),
                bool valueMonospace = false,
                bool detailMonospace = false)
{
    section.cards.append(makeCard(title, value, detail, valueMonospace, detailMonospace));
}

bool hasModelItem(const MeterModel *meterModel, const QString &diCode)
{
    return meterModel && !meterModel->getDataItem(diCode).diCode.isEmpty();
}

int currentRateCount(const MeterModel *meterModel)
{
    if (!meterModel)
    {
        return kDefaultRateCount;
    }

    bool ok = false;
    int rateCount = meterModel->getDataValue(QStringLiteral("0x04000204")).toInt(&ok);
    if (!ok)
    {
        rateCount = meterModel->getDataItem(QStringLiteral("0x04000204")).value.toInt(&ok);
    }

    return qBound(1, ok ? rateCount : kDefaultRateCount, kMaxRateCount);
}

bool isDynamicDemandBlock(const MeterModel *meterModel, const QString &diCode)
{
    if (!meterModel || !diCode.endsWith(QStringLiteral("FF00")))
    {
        return false;
    }

    const QString prefix = diCode.left(6);
    const DataItem totalItem = meterModel->getDataItem(prefix + QStringLiteral("0000"));
    return !totalItem.diCode.isEmpty() &&
           hasModelItem(meterModel, prefix + QStringLiteral("0001")) &&
           hasModelItem(meterModel, prefix + QStringLiteral("0100")) &&
           hasModelItem(meterModel, prefix + QStringLiteral("0101"));
}

bool isDynamicEnergyBlock(const MeterModel *meterModel, const QString &diCode)
{
    if (!meterModel || !diCode.endsWith(QStringLiteral("FF00")))
    {
        return false;
    }

    const QString prefix = diCode.left(6);
    const DataItem totalItem = meterModel->getDataItem(prefix + QStringLiteral("0000"));
    return !totalItem.diCode.isEmpty() &&
           totalItem.unit.contains(QChar('h'), Qt::CaseInsensitive) &&
           hasModelItem(meterModel, prefix + QStringLiteral("0100")) &&
           !hasModelItem(meterModel, prefix + QStringLiteral("0001"));
}

bool isDynamicFreezeEnergyBlock(const MeterModel *meterModel, const QString &diCode)
{
    if (!meterModel || diCode.size() != 10 || !diCode.startsWith(QStringLiteral("0x05")) || !diCode.endsWith(QStringLiteral("01")))
    {
        return false;
    }

    bool ok = false;
    const int familyValue = diCode.mid(6, 2).toInt(&ok, 16);
    if (!ok || familyValue < 0x01 || familyValue > 0x08)
    {
        return false;
    }

    const QString totalCode = diCode.left(8) + QStringLiteral("00");
    const DataItem totalItem = meterModel->getDataItem(totalCode);
    return !totalItem.diCode.isEmpty() && totalItem.unit.contains(QChar('h'), Qt::CaseInsensitive);
}

bool isLatestHourlyFreezeCompositeBlock(const QString &diCode)
{
    return diCode == QStringLiteral("0x0504FF01");
}

QStringList dynamicEnergyPayloadOrderCodes(const MeterModel *meterModel, const QString &diCode)
{
    const QString prefix = diCode.left(6);
    const int rateCount = currentRateCount(meterModel);

    QStringList codes;
    codes.reserve(rateCount + 1);
    for (int rateIndex = rateCount; rateIndex >= 1; --rateIndex)
    {
        codes.append(prefix + upperHexByte(rateIndex) + QStringLiteral("00"));
    }
    codes.append(prefix + QStringLiteral("0000"));
    return codes;
}

QStringList dynamicEnergyDisplayOrderCodes(const MeterModel *meterModel, const QString &diCode)
{
    const QString prefix = diCode.left(6);
    const int rateCount = currentRateCount(meterModel);

    QStringList codes;
    codes.reserve(rateCount + 1);
    codes.append(prefix + QStringLiteral("0000"));
    for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
    {
        codes.append(prefix + upperHexByte(rateIndex) + QStringLiteral("00"));
    }
    return codes;
}

QStringList dynamicDemandPayloadOrderCodes(const MeterModel *meterModel, const QString &diCode)
{
    const QString prefix = diCode.left(6);
    const int rateCount = currentRateCount(meterModel);

    QStringList codes;
    codes.reserve((rateCount + 1) * 2);
    for (int rateIndex = rateCount; rateIndex >= 1; --rateIndex)
    {
        const QString rateHex = upperHexByte(rateIndex);
        codes.append(prefix + rateHex + QStringLiteral("01"));
        codes.append(prefix + rateHex + QStringLiteral("00"));
    }
    codes.append(prefix + QStringLiteral("0001"));
    codes.append(prefix + QStringLiteral("0000"));
    return codes;
}

QStringList dynamicDemandDisplayOrderCodes(const MeterModel *meterModel, const QString &diCode)
{
    const QString prefix = diCode.left(6);
    const int rateCount = currentRateCount(meterModel);

    QStringList codes;
    codes.reserve((rateCount + 1) * 2);
    codes.append(prefix + QStringLiteral("0000"));
    codes.append(prefix + QStringLiteral("0001"));
    for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
    {
        const QString rateHex = upperHexByte(rateIndex);
        codes.append(prefix + rateHex + QStringLiteral("00"));
        codes.append(prefix + rateHex + QStringLiteral("01"));
    }
    return codes;
}

QStringList dynamicFreezeEnergyPayloadOrderCodes(const MeterModel *meterModel, const QString &diCode)
{
    const QString prefix = diCode.left(8);
    const int rateCount = currentRateCount(meterModel);

    QStringList codes;
    codes.reserve(rateCount + 1);
    for (int rateIndex = rateCount; rateIndex >= 1; --rateIndex)
    {
        codes.append(prefix + upperHexByte(rateIndex + 1));
    }
    codes.append(prefix + QStringLiteral("00"));
    return codes;
}

QStringList dynamicFreezeEnergyDisplayOrderCodes(const MeterModel *meterModel, const QString &diCode)
{
    const QString prefix = diCode.left(8);
    const int rateCount = currentRateCount(meterModel);

    QStringList codes;
    codes.reserve(rateCount + 1);
    codes.append(prefix + QStringLiteral("00"));
    for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
    {
        codes.append(prefix + upperHexByte(rateIndex + 1));
    }
    return codes;
}

QStringList latestHourlyFreezeCompositeCodes()
{
    return {
        QStringLiteral("0x05040001"),
        QStringLiteral("0x05040100"),
        QStringLiteral("0x05040200")};
}

QStringList payloadOrderCodesForBlock(const MeterModel *meterModel, const QString &diCode)
{
    if (isDynamicEnergyBlock(meterModel, diCode))
    {
        return dynamicEnergyPayloadOrderCodes(meterModel, diCode);
    }

    if (isDynamicFreezeEnergyBlock(meterModel, diCode))
    {
        return dynamicFreezeEnergyPayloadOrderCodes(meterModel, diCode);
    }

    if (isDynamicDemandBlock(meterModel, diCode))
    {
        return dynamicDemandPayloadOrderCodes(meterModel, diCode);
    }

    if (isHarmonicBlockCode(diCode, QStringLiteral("0x020A")) ||
        isHarmonicBlockCode(diCode, QStringLiteral("0x020B")))
    {
        return harmonicPayloadOrderCodes(diCode);
    }

    if (diCode == QStringLiteral("0x0201FF00"))
    {
        return {QStringLiteral("0x02010300"), QStringLiteral("0x02010200"), QStringLiteral("0x02010100")};
    }

    if (diCode == QStringLiteral("0x0202FF00"))
    {
        return {QStringLiteral("0x02020300"), QStringLiteral("0x02020200"), QStringLiteral("0x02020100")};
    }

    if (diCode == QStringLiteral("0x0203FF00"))
    {
        return {QStringLiteral("0x02030300"), QStringLiteral("0x02030200"), QStringLiteral("0x02030100"), QStringLiteral("0x02030000")};
    }

    if (diCode == QStringLiteral("0x0204FF00"))
    {
        return {QStringLiteral("0x02040300"), QStringLiteral("0x02040200"), QStringLiteral("0x02040100"), QStringLiteral("0x02040000")};
    }

    if (diCode == QStringLiteral("0x0205FF00"))
    {
        return {QStringLiteral("0x02050300"), QStringLiteral("0x02050200"), QStringLiteral("0x02050100"), QStringLiteral("0x02050000")};
    }

    if (diCode == QStringLiteral("0x0206FF00"))
    {
        return {QStringLiteral("0x02060300"), QStringLiteral("0x02060200"), QStringLiteral("0x02060100"), QStringLiteral("0x02060000")};
    }

    if (diCode == QStringLiteral("0x0207FF00"))
    {
        return {QStringLiteral("0x02070300"), QStringLiteral("0x02070200"), QStringLiteral("0x02070100")};
    }

    if (diCode == QStringLiteral("0x0208FF00"))
    {
        return {QStringLiteral("0x02080300"), QStringLiteral("0x02080200"), QStringLiteral("0x02080100")};
    }

    if (diCode == QStringLiteral("0x0209FF00"))
    {
        return {QStringLiteral("0x02090300"), QStringLiteral("0x02090200"), QStringLiteral("0x02090100")};
    }

    if (diCode == QStringLiteral("0x230100FF"))
    {
        return {
            QStringLiteral("0x23010008"),
            QStringLiteral("0x23010007"),
            QStringLiteral("0x23010006"),
            QStringLiteral("0x23010005"),
            QStringLiteral("0x23010004"),
            QStringLiteral("0x23010003"),
            QStringLiteral("0x23010002"),
            QStringLiteral("0x23010001")};
    }

    if (isLatestHourlyFreezeCompositeBlock(diCode))
    {
        return latestHourlyFreezeCompositeCodes();
    }

    return {};
}

QStringList displayOrderCodesForBlock(const MeterModel *meterModel, const QString &diCode)
{
    if (isDynamicEnergyBlock(meterModel, diCode))
    {
        return dynamicEnergyDisplayOrderCodes(meterModel, diCode);
    }

    if (isDynamicFreezeEnergyBlock(meterModel, diCode))
    {
        return dynamicFreezeEnergyDisplayOrderCodes(meterModel, diCode);
    }

    if (isDynamicDemandBlock(meterModel, diCode))
    {
        return dynamicDemandDisplayOrderCodes(meterModel, diCode);
    }

    if (isHarmonicBlockCode(diCode, QStringLiteral("0x020A")) ||
        isHarmonicBlockCode(diCode, QStringLiteral("0x020B")))
    {
        return harmonicDisplayOrderCodes(diCode);
    }

    if (diCode == QStringLiteral("0x0201FF00"))
    {
        return {QStringLiteral("0x02010100"), QStringLiteral("0x02010200"), QStringLiteral("0x02010300")};
    }

    if (diCode == QStringLiteral("0x0202FF00"))
    {
        return {QStringLiteral("0x02020100"), QStringLiteral("0x02020200"), QStringLiteral("0x02020300")};
    }

    if (diCode == QStringLiteral("0x0203FF00"))
    {
        return {QStringLiteral("0x02030000"), QStringLiteral("0x02030100"), QStringLiteral("0x02030200"), QStringLiteral("0x02030300")};
    }

    if (diCode == QStringLiteral("0x0204FF00"))
    {
        return {QStringLiteral("0x02040000"), QStringLiteral("0x02040100"), QStringLiteral("0x02040200"), QStringLiteral("0x02040300")};
    }

    if (diCode == QStringLiteral("0x0205FF00"))
    {
        return {QStringLiteral("0x02050000"), QStringLiteral("0x02050100"), QStringLiteral("0x02050200"), QStringLiteral("0x02050300")};
    }

    if (diCode == QStringLiteral("0x0206FF00"))
    {
        return {QStringLiteral("0x02060000"), QStringLiteral("0x02060100"), QStringLiteral("0x02060200"), QStringLiteral("0x02060300")};
    }

    if (diCode == QStringLiteral("0x0207FF00"))
    {
        return {QStringLiteral("0x02070100"), QStringLiteral("0x02070200"), QStringLiteral("0x02070300")};
    }

    if (diCode == QStringLiteral("0x0208FF00"))
    {
        return {QStringLiteral("0x02080100"), QStringLiteral("0x02080200"), QStringLiteral("0x02080300")};
    }

    if (diCode == QStringLiteral("0x0209FF00"))
    {
        return {QStringLiteral("0x02090100"), QStringLiteral("0x02090200"), QStringLiteral("0x02090300")};
    }

    if (diCode == QStringLiteral("0x230100FF"))
    {
        return {
            QStringLiteral("0x23010001"),
            QStringLiteral("0x23010002"),
            QStringLiteral("0x23010003"),
            QStringLiteral("0x23010004"),
            QStringLiteral("0x23010005"),
            QStringLiteral("0x23010006"),
            QStringLiteral("0x23010007"),
            QStringLiteral("0x23010008")};
    }

    if (isLatestHourlyFreezeCompositeBlock(diCode))
    {
        return latestHourlyFreezeCompositeCodes();
    }

    return {};
}

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

        if (!item.format.contains(':') && BCDHelper::isValidBCD(payload))
        {
            const QString rawDigits = BCDHelper::bytesToHexString(payload, QString());
            return BCDHelper::bcdStringToDecimal(rawDigits, item.decimalPlaces);
        }
    }

    return decodedValueForItem(meterModel, diCode, payload, descriptionOut, unitOut);
}

QList<FrameParseCard> buildBlockDecodeCards(const MeterModel *meterModel, const QString &blockDiCode, const QByteArray &payload)
{
    if (!meterModel || blockDiCode.isEmpty() || payload.isEmpty())
    {
        return {};
    }

    const QStringList payloadCodes = payloadOrderCodesForBlock(meterModel, blockDiCode);
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

    const QStringList displayCodes = displayOrderCodesForBlock(meterModel, blockDiCode);
    QList<FrameParseCard> cards;
    for (const QString &code : displayCodes)
    {
        if (!valuesByCode.contains(code))
        {
            continue;
        }

        const QString payloadHex = payloadHexByCode.value(code);
        const QString description = descriptionsByCode.value(code, code);
        const QString value = appendUnit(valuesByCode.value(code), unitsByCode.value(code), false);
        const QString detail = QString::fromUtf8("DI: %1\n原始HEX: %2").arg(code, payloadHex);
        cards.append(makeCard(description, value, detail));
    }

    return cards;
}

FrameParseSection buildStructureSection(const QByteArray &frame, bool checksumValid, QString *messageOut)
{
    FrameParseSection section;
    section.title = QString::fromUtf8("结构解析");

    int leadingCount = 0;
    const QByteArray protocolFrame = DLT645DataHelper::stripLeadingFE(frame, &leadingCount);

    QString parseMessage;
    if (frame.isEmpty())
    {
        parseMessage = QString::fromUtf8("空帧，无法解析。");
    }

    const bool hasStartFlag = protocolFrame.size() >= 1;
    const bool hasAddress = protocolFrame.size() >= 7;
    const bool hasSecondStartFlag = protocolFrame.size() >= 8;
    const bool hasControlCode = protocolFrame.size() >= 9;
    const bool hasLength = protocolFrame.size() >= 10;

    const QByteArray address = hasAddress ? protocolFrame.mid(1, 6) : QByteArray();
    const quint8 startFlag = hasStartFlag ? static_cast<quint8>(protocolFrame[0]) : 0;
    const quint8 secondStartFlag = hasSecondStartFlag ? static_cast<quint8>(protocolFrame[7]) : 0;
    const quint8 controlCode = hasControlCode ? static_cast<quint8>(protocolFrame[8]) : 0;
    const quint8 dataLength = hasLength ? static_cast<quint8>(protocolFrame[9]) : 0;
    const int expectedSize = hasLength ? (12 + dataLength) : 12;
    const bool hasCompleteFrame = hasLength && protocolFrame.size() >= expectedSize;

    QByteArray displayedDataField;
    if (protocolFrame.size() > 10)
    {
        displayedDataField = hasCompleteFrame ? protocolFrame.mid(10, dataLength) : protocolFrame.mid(10);
    }

    QString structureText;
    structureText += lineWithValue(QString::fromUtf8("前导码："),
                                   leadingCount > 0 ? DLT645FrameFormatter::formatHex(frame.left(leadingCount)) : QString::fromUtf8("无"));
    structureText += lineWithValue(QString::fromUtf8("帧起始符："),
                                   hasStartFlag ? formatByteHex(startFlag) : QString::fromUtf8("未获取"));
    structureText += lineWithValue(QString::fromUtf8("地址域："),
                                   hasAddress ? DLT645FrameFormatter::formatHex(address) : QString::fromUtf8("未获取"));
    structureText += lineWithValue(QString::fromUtf8("电表号："),
                                   hasAddress ? ProtocolParser::bytesToAddress(address) : QString::fromUtf8("未获取"));
    structureText += lineWithValue(QString::fromUtf8("第二起始符："),
                                   hasSecondStartFlag ? formatByteHex(secondStartFlag) : QString::fromUtf8("未获取"));
    structureText += lineWithValue(QString::fromUtf8("控制码："),
                                   hasControlCode
                                       ? QString::fromUtf8("%1 (%2)").arg(formatByteHex(controlCode), DLT645FrameFormatter::controlCodeName(controlCode))
                                       : QString::fromUtf8("未获取"));
    structureText += lineWithValue(QString::fromUtf8("数据长度："),
                                   hasLength ? QString::fromUtf8("%1 (%2 字节)").arg(formatByteHex(dataLength)).arg(dataLength) : QString::fromUtf8("未获取"));
    structureText += lineWithValue(QString::fromUtf8("数据域原始值："),
                                   !displayedDataField.isEmpty() ? DLT645FrameFormatter::formatHex(displayedDataField)
                                                                 : (hasLength ? QString::fromUtf8("无") : QString::fromUtf8("未获取")));
    structureText += lineWithValue(QString::fromUtf8("数据域减0x33："),
                                   !displayedDataField.isEmpty() ? DLT645FrameFormatter::formatHex(DLT645DataHelper::subtract33(displayedDataField))
                                                                 : (hasLength ? QString::fromUtf8("无") : QString::fromUtf8("未获取")));

    if (hasCompleteFrame)
    {
        const quint8 checksum = static_cast<quint8>(protocolFrame[10 + dataLength]);
        const quint8 endFlag = static_cast<quint8>(protocolFrame[11 + dataLength]);
        structureText += lineWithValue(QString::fromUtf8("校验和："), formatByteHex(checksum));
        structureText += lineWithValue(QString::fromUtf8("校验状态："),
                                       checksumValid ? QString::fromUtf8("OK 有效") : QString::fromUtf8("X 无效"));
        structureText += lineWithValue(QString::fromUtf8("结束符："), formatByteHex(endFlag));

        if (protocolFrame.size() > expectedSize)
        {
            structureText += lineWithValue(QString::fromUtf8("尾随数据："),
                                           DLT645FrameFormatter::formatHex(protocolFrame.mid(expectedSize)));
        }
    }
    else
    {
        structureText += lineWithValue(QString::fromUtf8("校验和："), QString::fromUtf8("未获取"));
        structureText += lineWithValue(QString::fromUtf8("校验状态："), QString::fromUtf8("X 无效"));
        structureText += lineWithValue(QString::fromUtf8("结束符："), QString::fromUtf8("未获取"));

        if (parseMessage.isEmpty())
        {
            parseMessage = hasLength ? QString::fromUtf8("报文长度与声明的数据长度不一致，当前仅基于已收到字节解析。")
                                     : QString::fromUtf8("报文长度不足，尚未取得完整帧头。");
        }

        structureText += lineWithValue(QString::fromUtf8("长度异常/报文不完整提示："), parseMessage);
    }

    appendCard(section, section.title, structureText.trimmed());

    if (messageOut)
    {
        *messageOut = parseMessage;
    }

    return section;
}

FrameParseSection buildDataSection(const QByteArray &frame, const MeterModel *meterModel)
{
    FrameParseSection section;
    section.title = QString::fromUtf8("数据解析");

    if (frame.isEmpty())
    {
        appendCard(section, section.title, QString::fromUtf8("空帧，暂无可解析内容。"));
        return section;
    }

    const QByteArray protocolFrame = DLT645DataHelper::stripLeadingFE(frame);
    if (protocolFrame.size() < 12)
    {
        appendCard(section,
                   section.title,
                   QString::fromUtf8("无可解析数据"),
                   QString::fromUtf8("报文长度不足，未形成完整协议帧。"));
        return section;
    }

    const quint8 controlCode = static_cast<quint8>(protocolFrame[8]);
    const quint8 dataLength = static_cast<quint8>(protocolFrame[9]);
    const int actualDataLength = qMin(static_cast<int>(dataLength), qMax(0, protocolFrame.size() - 12));
    const QByteArray dataField = protocolFrame.mid(10, actualDataLength);
    const quint8 baseControlCode = controlCode & 0x7F;
    const bool isResponse = (controlCode & 0x80) != 0;

    if (dataField.isEmpty())
    {
        appendCard(section, section.title, QString::fromUtf8("当前帧无数据域。"));
        return section;
    }

    if (baseControlCode == 0x13)
    {
        QString dataText;
        if (isResponse)
        {
            const QByteArray decodedAddressBytes = DLT645DataHelper::decodeReversedPayload(dataField);
            dataText += lineWithValue(QString::fromUtf8("载荷类型："), QString::fromUtf8("地址响应"));
            dataText += lineWithValue(QString::fromUtf8("解码字节："), DLT645FrameFormatter::formatHex(decodedAddressBytes));

            if (decodedAddressBytes.size() == 6)
            {
                dataText += lineWithValue(QString::fromUtf8("解析值："), ProtocolParser::bytesToAddress(decodedAddressBytes));
            }
            else
            {
                dataText += lineWithValue(QString::fromUtf8("解析说明："), QString::fromUtf8("地址响应长度异常，未能解析为 6 字节地址。"));
            }
        }
        else
        {
            dataText += lineWithValue(QString::fromUtf8("载荷类型："), QString::fromUtf8("读地址请求"));
            dataText += lineWithValue(QString::fromUtf8("解析说明："), QString::fromUtf8("该请求帧不携带数据域，用于读取目标电表通信地址。"));
        }

        appendCard(section, section.title, dataText.trimmed());
        return section;
    }

    if (baseControlCode != 0x11 && baseControlCode != 0x14)
    {
        const QString value = QString::fromUtf8("解析说明：当前控制码暂无细分展示。\n控制码：%1 (%2)\n原始数据：%3")
                                  .arg(formatByteHex(controlCode),
                                       DLT645FrameFormatter::controlCodeName(controlCode),
                                       DLT645FrameFormatter::formatHex(dataField));
        appendCard(section, section.title, value);
        return section;
    }

    QString dataText;
    QString diCode;
    QString description;
    QString unit;
    QByteArray decodedPayload;
    QString payloadLabel;
    DataItem diItem;

    if (dataField.size() >= 4)
    {
        diCode = ProtocolParser::extractDIFromDataField(dataField);
        dataText += lineWithValue(QString::fromUtf8("数据标识："), diCode);

        if (meterModel)
        {
            diItem = meterModel->getDataItem(diCode);
            if (!diItem.diCode.isEmpty())
            {
                description = diItem.description;
                unit = diItem.unit;
                dataText += lineWithValue(QString::fromUtf8("数据项名称："), description);
            }
            else
            {
                dataText += lineWithValue(QString::fromUtf8("数据项名称："), QString::fromUtf8("未找到匹配数据项"));
            }
        }
    }
    else
    {
        const QString value = QString::fromUtf8("解析说明：数据域长度不足，无法提取数据标识。\n原始数据：%1")
                                  .arg(DLT645FrameFormatter::formatHex(dataField));
        appendCard(section, section.title, value);
        return section;
    }

    if (baseControlCode == 0x11)
    {
        if (isResponse)
        {
            decodedPayload = DLT645DataHelper::decodeReversedPayload(dataField.mid(4));
            payloadLabel = QString::fromUtf8("应答数据");
        }
        else
        {
            decodedPayload = dataField.mid(4);
            payloadLabel = QString::fromUtf8("请求参数");
        }
    }
    else if (isResponse)
    {
        decodedPayload = dataField.mid(5);
        payloadLabel = QString::fromUtf8("写入状态");
    }
    else if (dataField.size() > 5)
    {
        const int payloadLength = static_cast<quint8>(dataField[4]);
        decodedPayload = DLT645DataHelper::decodeReversedPayload(dataField.mid(5, payloadLength > 0 ? payloadLength : -1));
        payloadLabel = QString::fromUtf8("写入数据");
    }

    if (!payloadLabel.isEmpty())
    {
        dataText += lineWithValue(QString::fromUtf8("载荷类型："), payloadLabel);
    }

    if (!decodedPayload.isEmpty())
    {
        dataText += lineWithValue(QString::fromUtf8("解码字节："), DLT645FrameFormatter::formatHex(decodedPayload));

        const QList<FrameParseCard> blockCards = buildBlockDecodeCards(meterModel, diCode, decodedPayload);
        if (!blockCards.isEmpty())
        {
            QStringList blockLines;
            for (const FrameParseCard &card : blockCards)
            {
                QString line = QString::fromUtf8("%1：%2").arg(card.title, card.value);
                if (!card.detail.isEmpty())
                {
                    QString compactDetail = card.detail;
                    compactDetail.replace(QStringLiteral("\n"), QStringLiteral("，"));
                    line += QString::fromUtf8("（%1）").arg(compactDetail);
                }
                blockLines.append(line);
            }
            dataText += lineWithValue(QString::fromUtf8("解析值："), blockLines.join(QStringLiteral("\n")));
        }
        else
        {
            QString decodeDescription = description;
            QString decodeUnit = unit;
            const QString decodedValue = diCode.isEmpty()
                                             ? DLT645FrameFormatter::formatHex(decodedPayload)
                                             : decodedValueForItem(meterModel, diCode, decodedPayload, &decodeDescription, &decodeUnit);

            if (!decodedValue.isEmpty())
            {
                dataText += lineWithValue(QString::fromUtf8("解析值："), appendUnit(decodedValue, decodeUnit));
                if (!decodeDescription.isEmpty() && decodeDescription != description)
                {
                    dataText += lineWithValue(QString::fromUtf8("解析说明："), decodeDescription);
                }
            }
            else
            {
                dataText += lineWithValue(QString::fromUtf8("解析说明："), QString::fromUtf8("当前数据暂未能进一步解码。"));
            }
        }
    }
    else
    {
        dataText += lineWithValue(QString::fromUtf8("解析说明："), QString::fromUtf8("该帧未携带附加数据。"));
    }

    if (meterModel && !diCode.isEmpty() && isResponse && !diItem.diCode.isEmpty() && !diItem.format.contains(':'))
    {
        const QVariant modelValue = meterModel->getDataValue(diCode);
        if (modelValue.isValid())
        {
            DataItem modelItem = diItem;
            modelItem.value = modelValue;
            const QString modelText = modelItem.getFormattedValue();
            if (!modelText.isEmpty())
            {
                dataText += lineWithValue(QString::fromUtf8("MeterModel 当前值："), appendUnit(modelText, unit));
            }
        }
    }

    appendCard(section, section.title, dataText.trimmed());
    return section;
}

QString renderDisplayResultAsText(const FrameParseDisplayResult &result)
{
    QString detail;

    if (!result.errorText.isEmpty())
    {
        detail += result.errorText + QStringLiteral("\n\n");
    }
    else if (!result.noticeText.isEmpty())
    {
        detail += result.noticeText + QStringLiteral("\n\n");
    }

    for (int sectionIndex = 0; sectionIndex < result.sections.size(); ++sectionIndex)
    {
        const FrameParseSection &section = result.sections.at(sectionIndex);
        detail += section.title + QString::fromUtf8("：\n");

        if (section.cards.isEmpty())
        {
            detail += section.placeholderText.isEmpty() ? QString::fromUtf8("无。\n") : (section.placeholderText + QStringLiteral("\n"));
        }
        else
        {
            for (const FrameParseCard &card : section.cards)
            {
                detail += lineWithValue(card.title + QString::fromUtf8("："), card.value);
                if (!card.detail.isEmpty())
                {
                    detail += card.detail + QStringLiteral("\n");
                }
            }
        }

        if (sectionIndex + 1 < result.sections.size())
        {
            detail += QStringLiteral("\n");
        }
    }

    return detail;
}
}

QString DLT645FrameFormatter::formatHex(const QByteArray &data, bool withSpaces)
{
    return QString::fromLatin1(withSpaces ? data.toHex(' ') : data.toHex()).toUpper();
}

QString DLT645FrameFormatter::controlCodeName(quint8 code)
{
    switch (code)
    {
    case 0x11:
        return QString::fromUtf8("读数据");
    case 0x12:
        return QString::fromUtf8("读后续数据");
    case 0x13:
        return QString::fromUtf8("读通信地址");
    case 0x14:
        return QString::fromUtf8("写数据");
    case 0x15:
        return QString::fromUtf8("广播校时");
    case 0x16:
        return QString::fromUtf8("写设备地址");
    case 0x17:
        return QString::fromUtf8("更改通信速率");
    case 0x18:
        return QString::fromUtf8("修改密码");
    case 0x19:
        return QString::fromUtf8("最大需量清零");
    case 0x1A:
        return QString::fromUtf8("电表清零");
    case 0x1B:
        return QString::fromUtf8("事件清零");
    case 0x91:
        return QString::fromUtf8("正常应答");
    case 0x92:
        return QString::fromUtf8("后续应答");
    case 0x94:
        return QString::fromUtf8("写数据应答");
    case 0xD1:
        return QString::fromUtf8("异常应答");
    case 0xD2:
        return QString::fromUtf8("不支持");
    default:
        return QString::fromUtf8("未知");
    }
}

QString DLT645FrameFormatter::buildFrameStructureText(const QByteArray &frame)
{
    const FrameParseDisplayResult result = buildParseDisplayResult(frame, nullptr);

    FrameParseDisplayResult structureOnly;
    structureOnly.checksumValid = result.checksumValid;
    structureOnly.errorText = result.errorText;
    structureOnly.noticeText = result.noticeText;
    if (!result.sections.isEmpty())
    {
        structureOnly.sections.append(result.sections.first());
    }

    return renderDisplayResultAsText(structureOnly);
}

FrameParseDisplayResult DLT645FrameFormatter::buildParseDisplayResult(const QByteArray &frame, const MeterModel *meterModel)
{
    return buildParseDisplayResult(frame, meterModel, calculateChecksumValidity(frame));
}

FrameParseDisplayResult DLT645FrameFormatter::buildParseDisplayResult(const QByteArray &frame, const MeterModel *meterModel, bool checksumValid)
{
    FrameParseDisplayResult result;
    result.checksumValid = checksumValid;

    QString structureMessage;
    result.sections.append(buildStructureSection(frame, checksumValid, &structureMessage));
    result.sections.append(buildDataSection(frame, meterModel));

    if (frame.isEmpty())
    {
        result.errorText = QString::fromUtf8("空帧，无法解析。");
    }
    else
    {
        const QByteArray protocolFrame = DLT645DataHelper::stripLeadingFE(frame);
        if (protocolFrame.size() < 12)
        {
            result.errorText = QString::fromUtf8("报文长度不足，无法完成完整帧解析。");
        }
        else
        {
            const quint8 dataLength = static_cast<quint8>(protocolFrame[9]);
            const int expectedSize = 12 + dataLength;
            if (protocolFrame.size() < expectedSize)
            {
                result.noticeText = structureMessage;
            }
            else if (!checksumValid)
            {
                result.noticeText = QString::fromUtf8("校验和无效，请核对原始帧。");
            }
        }
    }

    return result;
}

QString DLT645FrameFormatter::buildParseText(const QByteArray &frame, const MeterModel *meterModel)
{
    return renderDisplayResultAsText(buildParseDisplayResult(frame, meterModel));
}

QString DLT645FrameFormatter::buildParseText(const QByteArray &frame, const MeterModel *meterModel, bool checksumValid)
{
    return renderDisplayResultAsText(buildParseDisplayResult(frame, meterModel, checksumValid));
}
