#ifndef METERMODELPRIVATEFREEZE_H
#define METERMODELPRIVATEFREEZE_H

#include "MeterModelPrivateCommon.h"

namespace MeterModelPrivate
{
    QString repeatedFormat(const QString &formatPart, int repeatCount);
    bool parseHexByteValue(const QString &hex, int *value);

    struct FreezeEnergyFamilyDefinition
    {
        QString familyCode;
        QString description;
        QString unit;
        double baseValue;
    };

    struct FreezeCategoryDefinition
    {
        QString prefix;
        QString description;
        int freezeOffsetDays;
        int freezeOffsetHours;
        double valueBias;
    };

    struct FreezeDemandFamilyDefinition
    {
        QString familyCode;
        QString description;
        QString unit;
        double baseValue;
    };

    inline const QList<FreezeEnergyFamilyDefinition> &multiRateFreezeEnergyFamilies()
    {
        static const QList<FreezeEnergyFamilyDefinition> families = {
            {QStringLiteral("01"), QStringLiteral("正向有功"), QStringLiteral("kWh"), 123.45},
            {QStringLiteral("02"), QStringLiteral("反向有功"), QStringLiteral("kWh"), 80.15},
            {QStringLiteral("03"), QStringLiteral("组合无功1"), QStringLiteral("kvarh"), 61.28},
            {QStringLiteral("04"), QStringLiteral("组合无功2"), QStringLiteral("kvarh"), 55.67},
            {QStringLiteral("05"), QStringLiteral("第一象限无功"), QStringLiteral("kvarh"), 42.18},
            {QStringLiteral("06"), QStringLiteral("第二象限无功"), QStringLiteral("kvarh"), 39.76},
            {QStringLiteral("07"), QStringLiteral("第三象限无功"), QStringLiteral("kvarh"), 37.54},
            {QStringLiteral("08"), QStringLiteral("第四象限无功"), QStringLiteral("kvarh"), 35.32}};
        return families;
    }

    inline const QList<FreezeCategoryDefinition> &latestFreezeCategoryDefinitions()
    {
        static const QList<FreezeCategoryDefinition> definitions = {
            {QStringLiteral("0x0500"), QStringLiteral("定时冻结"), 7, 0, 12.0},
            {QStringLiteral("0x0501"), QStringLiteral("瞬时冻结"), 0, 6, 8.0},
            {QStringLiteral("0x0502"), QStringLiteral("两套时区表切换"), 30, 0, 6.0},
            {QStringLiteral("0x0503"), QStringLiteral("两套日时段表切换"), 15, 0, 4.0},
            {QStringLiteral("0x0505"), QStringLiteral("分时费率切换冻结"), 10, 0, 3.0},
            {QStringLiteral("0x0506"), QStringLiteral("日冻结"), 1, 0, 0.0},
            {QStringLiteral("0x0507"), QStringLiteral("两套阶梯切换"), 45, 0, 5.0}};
        return definitions;
    }

    inline bool usesCurrentDayMidnightFreezeTime(const FreezeCategoryDefinition &category)
    {
        return category.prefix == QStringLiteral("0x0506");
    }

    inline QDateTime defaultLatestFreezeSnapshotTime(const FreezeCategoryDefinition &category, const QDateTime &referenceTime)
    {
        if (usesCurrentDayMidnightFreezeTime(category))
        {
            return QDateTime(referenceTime.date(), QTime(0, 0));
        }

        return referenceTime.addDays(-category.freezeOffsetDays).addSecs(-category.freezeOffsetHours * 60 * 60);
    }

    inline const QList<FreezeDemandFamilyDefinition> &freezeDemandFamilies()
    {
        static const QList<FreezeDemandFamilyDefinition> families = {
            {QStringLiteral("09"), QStringLiteral("正向有功"), QStringLiteral("kW"), 9.8765},
            {QStringLiteral("0A"), QStringLiteral("反向有功"), QStringLiteral("kW"), 6.5432}};
        return families;
    }

    inline QStringList freezeRateSuffixes(int rateCount)
    {
        QStringList suffixes;
        suffixes.reserve(rateCount);
        for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
        {
            suffixes.append(upperHexByte(rateIndex + 1));
        }
        return suffixes;
    }

    inline bool isFreezeEnergyFamilyCode(const QString &diCode)
    {
        if (diCode.size() != 10 || !diCode.startsWith(QStringLiteral("0x05")))
        {
            return false;
        }

        int familyValue = 0;
        if (!parseHexByteValue(diCode.mid(6, 2), &familyValue))
        {
            return false;
        }

        return familyValue >= 1 && familyValue <= 8;
    }

    inline bool isFreezeEnergyBlockCode(const QString &diCode)
    {
        return isFreezeEnergyFamilyCode(diCode) && diCode.endsWith(QStringLiteral("01"));
    }

    [[maybe_unused]] inline bool isFreezeDemandBlockCode(const QString &diCode)
    {
        if (diCode.size() != 10 || !diCode.startsWith(QStringLiteral("0x05")) || !diCode.endsWith(QStringLiteral("01")))
        {
            return false;
        }

        const QString familyHex = diCode.mid(6, 2);
        return familyHex == QStringLiteral("09") || familyHex == QStringLiteral("0A");
    }

    [[maybe_unused]] inline bool isFreezeVariableBlockCode(const QString &diCode)
    {
        return diCode.size() == 10 &&
               diCode.startsWith(QStringLiteral("0x05")) &&
               diCode.mid(6, 2) == QStringLiteral("10") &&
               diCode.endsWith(QStringLiteral("01"));
    }

    inline QStringList currentFreezeEnergyBlockCodes(const QMap<QString, DataItem> &items, const QString &diPrefix)
    {
        const int rateCount = currentRateCount(items);
        QStringList codes;
        codes.reserve(rateCount + 1);
        codes.append(diPrefix + QStringLiteral("00"));
        for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
        {
            codes.append(diPrefix + upperHexByte(rateIndex + 1));
        }
        return codes;
    }

    inline void initializeLatestFreezeEnergyCategory(QMap<QString, DataItem> &items,
                                             const FreezeCategoryDefinition &category,
                                             const QDateTime &freezeTime)
    {
        const QString timeCode = category.prefix + QStringLiteral("0001");
        items[timeCode] = DataItem(timeCode,
                                   QStringLiteral("上1次%1时间").arg(category.description),
                                   compactDateTimeToMinutes(freezeTime),
                                   QStringLiteral("-"),
                                   QStringLiteral("AA,AA,AA,AA,AA"),
                                   false,
                                   0);

        for (const FreezeEnergyFamilyDefinition &family : multiRateFreezeEnergyFamilies())
        {
            const QString prefix = category.prefix + family.familyCode;
            const double totalValue = roundToDecimals(family.baseValue + category.valueBias, 2);
            items[prefix + QStringLiteral("00")] = DataItem(prefix + QStringLiteral("00"),
                                                            QStringLiteral("上1次%1%2总电能").arg(category.description, family.description),
                                                            totalValue,
                                                            family.unit,
                                                            QStringLiteral("AA,AA,AA,.,AA"),
                                                            false,
                                                            2);
            items[prefix + QStringLiteral("01")] = DataItem(prefix + QStringLiteral("01"),
                                                            QStringLiteral("上1次%1%2电能数据块").arg(category.description, family.description),
                                                            QVariantList(),
                                                            QStringLiteral("-"),
                                                            repeatedFormat(QStringLiteral("AA,AA,AA,.,AA"), kDefaultRateCount + 1),
                                                            false,
                                                            2);
            const QList<double> weights = {0.28, 0.24, 0.20, 0.16, 0.12};
            for (int rateIndex = 1; rateIndex <= kDefaultRateCount; ++rateIndex)
            {
                const QString rateCode = prefix + upperHexByte(rateIndex + 1);
                items[rateCode] = DataItem(rateCode,
                                           QStringLiteral("上1次%1%2费率%3电能").arg(category.description, family.description).arg(rateIndex),
                                           roundToDecimals(totalValue * weights.at(rateIndex - 1), 2),
                                           family.unit,
                                           QStringLiteral("AA,AA,AA,.,AA"),
                                           false,
                                           2);
            }
        }
    }

    inline void initializeLatestHourlyFreezeCategory(QMap<QString, DataItem> &items, const QDateTime &freezeTime)
    {
        items[QStringLiteral("0x05040001")] = DataItem(QStringLiteral("0x05040001"),
                                                       QStringLiteral("上1次整点冻结时间"),
                                                       compactDateTimeToMinutes(freezeTime),
                                                       QStringLiteral("-"),
                                                       QStringLiteral("AA,AA,AA,AA,AA"),
                                                       false,
                                                       0);
        items[QStringLiteral("0x05040100")] = DataItem(QStringLiteral("0x05040100"),
                                                       QStringLiteral("上1次整点冻结正向有功总电能"),
                                                       96.42,
                                                       QStringLiteral("kWh"),
                                                       QStringLiteral("AA,AA,AA,.,AA"),
                                                       false,
                                                       2);
        items[QStringLiteral("0x05040200")] = DataItem(QStringLiteral("0x05040200"),
                                                       QStringLiteral("上1次整点冻结反向有功总电能"),
                                                       18.37,
                                                       QStringLiteral("kWh"),
                                                       QStringLiteral("AA,AA,AA,.,AA"),
                                                       false,
                                                       2);
        items[QStringLiteral("0x0504FF01")] = DataItem(QStringLiteral("0x0504FF01"),
                                                       QStringLiteral("上1次整点冻结数据块"),
                                                       QVariantList(),
                                                       QStringLiteral("-"),
                                                       QStringLiteral("AA,AA,AA,AA,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA"),
                                                       false,
                                                       2);
    }

    inline QDateTime freezeSnapshotTime(const QMap<QString, DataItem> &items, const FreezeCategoryDefinition &category)
    {
        if (usesCurrentDayMidnightFreezeTime(category))
        {
            return defaultLatestFreezeSnapshotTime(category, QDateTime::currentDateTime());
        }

        const QString timeCode = category.prefix + QStringLiteral("0001");
        if (items.contains(timeCode))
        {
            const QDateTime parsed = QDateTime::fromString(items.value(timeCode).value.toString(), QStringLiteral("yyMMddHHmm"));
            if (parsed.isValid())
            {
                return parsed;
            }
        }

        return defaultLatestFreezeSnapshotTime(category, QDateTime::currentDateTime());
    }

    inline QVariantList buildFreezeDemandBlockValue(const FreezeDemandFamilyDefinition &family,
                                             const QDateTime &freezeTime,
                                             double valueBias,
                                             int rateCount)
    {
        const QList<double> weights = {0.94, 0.88, 0.81, 0.76, 0.70};
        const double totalValue = roundToDecimals(family.baseValue + valueBias, 4);

        QVariantList values;
        values.reserve((rateCount + 1) * 2);
        values.append(totalValue);
        values.append(compactDateTimeToMinutes(freezeTime.addSecs(-7 * 60)));

        for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
        {
            const int weightIndex = qMin(rateIndex - 1, weights.size() - 1);
            const QDateTime eventTime = freezeTime.addSecs(-(rateIndex * 37 + 11) * 60);
            values.append(roundToDecimals(totalValue * weights.at(weightIndex), 4));
            values.append(compactDateTimeToMinutes(eventTime));
        }
        return values;
    }

    inline QVariantList buildFreezeVariableBlockValue(double valueBias)
    {
        const double aActive = roundToDecimals(4.2800 + valueBias * 0.08, 4);
        const double bActive = roundToDecimals(3.9100 + valueBias * 0.07, 4);
        const double cActive = roundToDecimals(3.5800 + valueBias * 0.06, 4);
        const double totalActive = roundToDecimals(aActive + bActive + cActive, 4);

        const double aReactive = roundToDecimals(1.8200 + valueBias * 0.05, 4);
        const double bReactive = roundToDecimals(1.6400 + valueBias * 0.04, 4);
        const double cReactive = roundToDecimals(1.5300 + valueBias * 0.03, 4);
        const double totalReactive = roundToDecimals(aReactive + bReactive + cReactive, 4);

        return QVariantList{
            totalActive,
            aActive,
            bActive,
            cActive,
            totalReactive,
            aReactive,
            bReactive,
            cReactive};
    }

    inline void refreshLatestFreezeCompositeBlocks(QMap<QString, DataItem> &items)
    {
        const int rateCount = currentRateCount(items);
        for (const FreezeCategoryDefinition &category : latestFreezeCategoryDefinitions())
        {
            const QDateTime freezeTime = freezeSnapshotTime(items, category);
            const QString timeCode = category.prefix + QStringLiteral("0001");
            if (usesCurrentDayMidnightFreezeTime(category) && items.contains(timeCode))
            {
                items[timeCode].value = compactDateTimeToMinutes(freezeTime);
            }

            for (const FreezeDemandFamilyDefinition &family : freezeDemandFamilies())
            {
                const QString blockCode = category.prefix + family.familyCode + QStringLiteral("01");
                if (!items.contains(blockCode))
                {
                    items[blockCode] = DataItem(blockCode,
                                                QStringLiteral("上1次%1%2最大需量及发生时间数据块").arg(category.description, family.description),
                                                QVariantList(),
                                                QStringLiteral("-"),
                                                repeatedFormat(QStringLiteral("AA,.,AA,AA:AA,AA,AA,AA,AA"), rateCount + 1),
                                                false,
                                                0);
                }

                items[blockCode].format = repeatedFormat(QStringLiteral("AA,.,AA,AA:AA,AA,AA,AA,AA"), rateCount + 1);
                items[blockCode].value = buildFreezeDemandBlockValue(family, freezeTime, category.valueBias, rateCount);
            }

            const QString variableBlockCode = category.prefix + QStringLiteral("1001");
            if (!items.contains(variableBlockCode))
            {
                items[variableBlockCode] = DataItem(variableBlockCode,
                                                    QStringLiteral("上1次%1变量数据块").arg(category.description),
                                                    QVariantList(),
                                                    QStringLiteral("-"),
                                                    repeatedFormat(QStringLiteral("AA,.,AA,AA"), 8),
                                                    false,
                                                    4);
            }

            items[variableBlockCode].value = buildFreezeVariableBlockValue(category.valueBias);
        }

        if (items.contains(QStringLiteral("0x0504FF01")))
        {
            items[QStringLiteral("0x0504FF01")].value = QVariantList{
                items.value(QStringLiteral("0x05040001")).value,
                items.value(QStringLiteral("0x05040100")).value,
                items.value(QStringLiteral("0x05040200")).value};
        }
    }

}

#endif // METERMODELPRIVATEFREEZE_H
