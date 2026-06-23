#include "DLT645BlockHandler.h"
#include "Models/MeterModel.h"
#include "Models/DataItem.h"
#include "Utils/DLT645DataHelper.h"

namespace
{
    constexpr int kDefaultRateCount = 5;
    constexpr int kMaxRateCount = 8;
    constexpr int kHarmonicOrderCount = 21;

    QString upperHexByte(int value)
    {
        return QStringLiteral("%1").arg(value, 2, 16, QChar('0')).toUpper();
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

    int effectiveRateCount(const MeterModel *meterModel, int rateCountOverride)
    {
        return rateCountOverride > 0
                   ? qBound(1, rateCountOverride, kMaxRateCount)
                   : currentRateCount(meterModel);
    }

    int payloadSizeForCodes(const MeterModel *meterModel, const QStringList &codes)
    {
        if (!meterModel || codes.isEmpty())
        {
            return -1;
        }

        int totalSize = 0;
        for (const QString &code : codes)
        {
            const DataItem item = meterModel->getDataItem(code);
            if (item.diCode.isEmpty())
            {
                return -1;
            }

            const int size = DLT645DataHelper::bytesRequiredForFormat(item.format);
            if (size <= 0)
            {
                return -1;
            }

            totalSize += size;
        }

        return totalSize;
    }
}

namespace DLT645BlockHandler
{

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
    for (int harmonicOrder = 1; harmonicOrder <= kHarmonicOrderCount; ++harmonicOrder)
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

bool isRateCountDependentBlock(const MeterModel *meterModel, const QString &diCode)
{
    return isDynamicEnergyBlock(meterModel, diCode) ||
           isDynamicFreezeEnergyBlock(meterModel, diCode) ||
           isDynamicDemandBlock(meterModel, diCode);
}

QStringList dynamicEnergyPayloadOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride)
{
    const QString prefix = diCode.left(6);
    const int rateCount = effectiveRateCount(meterModel, rateCountOverride);

    QStringList codes;
    codes.reserve(rateCount + 1);
    codes.append(prefix + QStringLiteral("0000"));
    for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
    {
        codes.append(prefix + upperHexByte(rateIndex) + QStringLiteral("00"));
    }
    return codes;
}

QStringList dynamicEnergyDisplayOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride)
{
    return dynamicEnergyPayloadOrderCodes(meterModel, diCode, rateCountOverride);
}

QStringList dynamicDemandPayloadOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride)
{
    const QString prefix = diCode.left(6);
    const int rateCount = effectiveRateCount(meterModel, rateCountOverride);

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

QStringList dynamicDemandDisplayOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride)
{
    return dynamicDemandPayloadOrderCodes(meterModel, diCode, rateCountOverride);
}

QStringList dynamicFreezeEnergyPayloadOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride)
{
    const QString prefix = diCode.left(8);
    const int rateCount = effectiveRateCount(meterModel, rateCountOverride);

    QStringList codes;
    codes.reserve(rateCount + 1);
    codes.append(prefix + QStringLiteral("00"));
    for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
    {
        codes.append(prefix + upperHexByte(rateIndex + 1));
    }
    return codes;
}

QStringList dynamicFreezeEnergyDisplayOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride)
{
    return dynamicFreezeEnergyPayloadOrderCodes(meterModel, diCode, rateCountOverride);
}

QStringList latestHourlyFreezeCompositeCodes()
{
    return {
        QStringLiteral("0x05040001"),
        QStringLiteral("0x05040100"),
        QStringLiteral("0x05040200")};
}

QStringList payloadOrderCodesForBlock(const MeterModel *meterModel, const QString &diCode, int rateCountOverride)
{
    if (isDynamicEnergyBlock(meterModel, diCode))
    {
        return dynamicEnergyPayloadOrderCodes(meterModel, diCode, rateCountOverride);
    }

    if (isDynamicFreezeEnergyBlock(meterModel, diCode))
    {
        return dynamicFreezeEnergyPayloadOrderCodes(meterModel, diCode, rateCountOverride);
    }

    if (isDynamicDemandBlock(meterModel, diCode))
    {
        return dynamicDemandPayloadOrderCodes(meterModel, diCode, rateCountOverride);
    }

    if (isHarmonicBlockCode(diCode, QStringLiteral("0x020A")) ||
        isHarmonicBlockCode(diCode, QStringLiteral("0x020B")))
    {
        return harmonicPayloadOrderCodes(diCode);
    }

    // 硬编码的块代码映射
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

QStringList displayOrderCodesForBlock(const MeterModel *meterModel, const QString &diCode, int rateCountOverride)
{
    return payloadOrderCodesForBlock(meterModel, diCode, rateCountOverride);
}

QStringList selectPayloadOrderCodesForBlock(const MeterModel *meterModel,
                                            const QString &blockDiCode,
                                            const QByteArray &payload,
                                            int *rateCountOverrideOut)
{
    if (rateCountOverrideOut)
    {
        *rateCountOverrideOut = -1;
    }

    QStringList payloadCodes = payloadOrderCodesForBlock(meterModel, blockDiCode);
    if (payloadSizeForCodes(meterModel, payloadCodes) == payload.size())
    {
        return payloadCodes;
    }

    if (!isRateCountDependentBlock(meterModel, blockDiCode))
    {
        return payloadCodes;
    }

    for (int rateCount = kMaxRateCount; rateCount >= 1; --rateCount)
    {
        const QStringList candidateCodes = payloadOrderCodesForBlock(meterModel, blockDiCode, rateCount);
        if (payloadSizeForCodes(meterModel, candidateCodes) == payload.size())
        {
            if (rateCountOverrideOut)
            {
                *rateCountOverrideOut = rateCount;
            }
            return candidateCodes;
        }
    }

    return payloadCodes;
}

} // namespace DLT645BlockHandler
