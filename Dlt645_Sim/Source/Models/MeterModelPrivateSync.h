#ifndef METERMODELPRIVATESYNC_H
#define METERMODELPRIVATESYNC_H

#include "MeterModelPrivateSupport.h"

namespace MeterModelPrivate
{
    inline void synchronizeGroupTotal(QMap<QString, DataItem> &items,
                               const QString &totalCode,
                               const QStringList &rateCodes)
    {
        if (!items.contains(totalCode))
        {
            return;
        }

        double sum = 0.0;
        for (const QString &rateCode : rateCodes)
        {
            if (!items.contains(rateCode))
            {
                return;
            }

            sum += items[rateCode].value.toDouble();
        }

        DataItem &totalItem = items[totalCode];
        const double roundedSum = roundToDecimals(sum, totalItem.decimalPlaces);
        const NumericFormatSpec spec = parseNumericFormatSpec(totalItem.format);
        totalItem.value = spec.valid ? normalizeValueForFormat(roundedSum, spec) : QVariant(roundedSum);
    }

    inline void synchronizeCurrentEnergyTotals(QMap<QString, DataItem> &items)
    {
        const int rateCount = currentRateCount(items);

        for (int settlementDay = 0; settlementDay <= kMaxSettlementDayCount; ++settlementDay)
        {
            for (const int baseFamilyValue : multiRateEnergyBaseFamilies())
            {
                const QString prefix = settlementDayEnergyPrefix(baseFamilyValue, settlementDay);
                synchronizeGroupTotal(items,
                                      prefix + "0000",
                                      prefixedCodes(prefix, energyRateSuffixes(rateCount)));
            }
        }
    }

    inline void synchronizeFreezeTotals(QMap<QString, DataItem> &items)
    {
        const int rateCount = currentRateCount(items);
        const QList<QString> codes = items.keys();
        for (const QString &code : codes)
        {
            if (!code.endsWith(QStringLiteral("00")) || !isFreezeEnergyFamilyCode(code))
            {
                continue;
            }

            if (code.startsWith(QStringLiteral("0x0504")))
            {
                continue;
            }

            synchronizeGroupTotal(items, code, prefixedCodes(code.left(8), freezeRateSuffixes(rateCount)));
        }
    }

    inline void synchronizeInstantaneousPowerTotals(QMap<QString, DataItem> &items)
    {
        synchronizeGroupTotal(items, "0x02030000", {"0x02030100", "0x02030200", "0x02030300"});
        synchronizeGroupTotal(items, "0x02040000", {"0x02040100", "0x02040200", "0x02040300"});
        synchronizeGroupTotal(items, "0x02050000", {"0x02050100", "0x02050200", "0x02050300"});
    }

    inline void synchronizeDemandTotals(QMap<QString, DataItem> &items)
    {
        const int rateCount = currentRateCount(items);
        for (int settlementDay = 0; settlementDay <= kMaxSettlementDayCount; ++settlementDay)
        {
            for (const DemandFamilyDefinition &family : currentDemandFamilies())
            {
                const QString prefix = settlementDayDemandPrefix(family.prefixValue, settlementDay);
                QStringList rateCodes;
                rateCodes.reserve(rateCount);
                for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
                {
                    rateCodes.append(prefix + upperHexByte(rateIndex) + QStringLiteral("00"));
                }
                synchronizeGroupTotal(items, prefix + QStringLiteral("0000"), rateCodes);
            }
        }
    }

    inline QString compactDateTimeToMinutes(const QDateTime &dateTime)
    {
        return dateTime.toString(QStringLiteral("yyMMddHHmm"));
    }

    inline QString compactDateTimeToSeconds(const QDateTime &dateTime)
    {
        return dateTime.toString(QStringLiteral("yyMMddHHmmss"));
    }

    inline QString compactDateWithWeek(const QDate &date)
    {
        const int weekCode = date.dayOfWeek() % 7;
        return date.toString(QStringLiteral("yyMMdd")) + QStringLiteral("%1").arg(weekCode, 2, 10, QChar('0'));
    }

    inline QVariant currentDateTimeParameterValue(const QString &diCode)
    {
        const QDateTime now = QDateTime::currentDateTime();
        if (diCode == QStringLiteral("0x04000101"))
        {
            return compactDateWithWeek(now.date());
        }
        if (diCode == QStringLiteral("0x04000102"))
        {
            return now.time().toString(QStringLiteral("HHmmss"));
        }
        return QVariant();
    }

    [[maybe_unused]] inline QString eventHistoryCode(const QString &baseCode, int historyIndex)
    {
        return baseCode + upperHexByte(historyIndex);
    }
}

#endif // METERMODELPRIVATESYNC_H