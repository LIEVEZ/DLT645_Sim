#ifndef METERMODELPRIVATERATE_H
#define METERMODELPRIVATERATE_H

#include "MeterModelPrivateFreeze.h"

namespace MeterModelPrivate
{

    inline QStringList energyRateSuffixes(int rateCount)
    {
        QStringList suffixes;
        suffixes.reserve(rateCount);
        for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
        {
            suffixes.append(upperHexByte(rateIndex) + QStringLiteral("00"));
        }
        return suffixes;
    }

    inline QStringList prefixedCodes(const QString &prefix, const QStringList &suffixes)
    {
        QStringList codes;
        codes.reserve(suffixes.size());
        for (const QString &suffix : suffixes)
        {
            codes.append(prefix + suffix);
        }
        return codes;
    }

    inline QString repeatedFormat(const QString &formatPart, int repeatCount)
    {
        if (formatPart.isEmpty() || repeatCount <= 0)
        {
            return QString();
        }

        QStringList parts;
        parts.reserve(repeatCount);
        for (int i = 0; i < repeatCount; ++i)
        {
            parts.append(formatPart);
        }
        return parts.join(QStringLiteral(":"));
    }

    inline QString rateDescription(const QString &templateDescription, int rateIndex)
    {
        QString description = templateDescription;
        description.replace(QStringLiteral("费率1"), QStringLiteral("费率%1").arg(rateIndex));
        return description;
    }

    inline bool parseHexByteValue(const QString &hex, int *value)
    {
        bool ok = false;
        const int parsed = hex.toInt(&ok, 16);
        if (!ok)
        {
            return false;
        }

        if (value)
        {
            *value = parsed;
        }
        return true;
    }

    inline bool isHarmonicFamilyCode(const QString &diCode,
                              const QString &familyPrefix,
                              int *phaseIndex = nullptr,
                              int *orderIndex = nullptr,
                              bool *isBlock = nullptr)
    {
        if (diCode.size() != 10 || !diCode.startsWith(familyPrefix))
        {
            return false;
        }

        int parsedPhaseIndex = 0;
        if (!parseHexByteValue(diCode.mid(6, 2), &parsedPhaseIndex) || parsedPhaseIndex < 1 || parsedPhaseIndex > 3)
        {
            return false;
        }

        const QString suffixHex = diCode.mid(8, 2);
        const bool block = (suffixHex == QStringLiteral("FF"));
        int parsedOrderIndex = 0;
        if (!block &&
            (!parseHexByteValue(suffixHex, &parsedOrderIndex) || parsedOrderIndex < 1 || parsedOrderIndex > kHarmonicOrderCount))
        {
            return false;
        }

        if (phaseIndex)
        {
            *phaseIndex = parsedPhaseIndex;
        }
        if (orderIndex)
        {
            *orderIndex = parsedOrderIndex;
        }
        if (isBlock)
        {
            *isBlock = block;
        }

        return true;
    }

    inline QStringList harmonicPayloadCodes(const QString &blockCode)
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

    inline void initializePhaseHarmonicFamily(QMap<QString, DataItem> &items,
                                       const QString &familyPrefix,
                                       const QString &quantityName,
                                       int phaseIndex,
                                       double baseValue,
                                       double orderStep)
    {
        const QString phaseName = phaseNameForIndex(phaseIndex);
        const QString diPrefix = familyPrefix + upperHexByte(phaseIndex);

        for (int harmonicOrder = 1; harmonicOrder <= kHarmonicOrderCount; ++harmonicOrder)
        {
            const QString code = diPrefix + upperHexByte(harmonicOrder);
            const double value = roundToDecimals(baseValue + (phaseIndex - 1) * 0.12 + (harmonicOrder - 1) * orderStep, 2);
            items[code] = DataItem(code,
                                   QStringLiteral("%1%2%3次谐波含量").arg(phaseName, quantityName).arg(harmonicOrder),
                                   value,
                                   "%",
                                   "AA,.,AA",
                                   false,
                                   2);
        }

        const QString blockCode = diPrefix + QStringLiteral("FF");
        items[blockCode] = DataItem(blockCode,
                                    QStringLiteral("%1%2谐波含量数据块").arg(phaseName, quantityName),
                                    QVariantList(),
                                    "-",
                                    repeatedFormat(QStringLiteral("AA,.,AA"), kHarmonicOrderCount),
                                    false,
                                    2);
    }

    inline bool isCurrentEnergyRateCode(const QString &diCode, int *rateIndex = nullptr)
    {
        if (diCode.size() != 10 || !diCode.endsWith(QStringLiteral("00")))
        {
            return false;
        }

        if (!isMultiRateEnergyCode(diCode))
        {
            return false;
        }

        const QString rateHex = diCode.mid(6, 2);
        if (rateHex == QStringLiteral("00") || rateHex == QStringLiteral("FF"))
        {
            return false;
        }

        int parsedRateIndex = 0;
        if (!parseHexByteValue(rateHex, &parsedRateIndex) || parsedRateIndex < 1 || parsedRateIndex > kMaxRateCount)
        {
            return false;
        }

        if (rateIndex)
        {
            *rateIndex = parsedRateIndex;
        }
        return true;
    }

    inline bool isDemandRateCode(const QString &diCode, int *rateIndex = nullptr)
    {
        if (diCode.size() != 10)
        {
            return false;
        }

        if (!isDemandDataCode(diCode))
        {
            return false;
        }

        const QString rateHex = diCode.mid(6, 2);
        const QString itemSuffix = diCode.mid(8, 2);
        if ((itemSuffix != QStringLiteral("00") && itemSuffix != QStringLiteral("01")) ||
            rateHex == QStringLiteral("00") || rateHex == QStringLiteral("FF"))
        {
            return false;
        }

        int parsedRateIndex = 0;
        if (!parseHexByteValue(rateHex, &parsedRateIndex) || parsedRateIndex < 1 || parsedRateIndex > kMaxRateCount)
        {
            return false;
        }

        if (rateIndex)
        {
            *rateIndex = parsedRateIndex;
        }
        return true;
    }

    inline bool isFreezeRateCode(const QString &diCode, int *rateIndex = nullptr)
    {
        if (diCode.size() != 10)
        {
            return false;
        }

        if (!isFreezeEnergyFamilyCode(diCode))
        {
            return false;
        }

        const QString suffixHex = diCode.mid(8, 2);
        if (suffixHex == QStringLiteral("00") || suffixHex == QStringLiteral("01"))
        {
            return false;
        }

        int parsedSuffix = 0;
        if (!parseHexByteValue(suffixHex, &parsedSuffix))
        {
            return false;
        }

        const int parsedRateIndex = parsedSuffix - 1;
        if (parsedRateIndex < 1 || parsedRateIndex > kMaxRateCount)
        {
            return false;
        }

        if (rateIndex)
        {
            *rateIndex = parsedRateIndex;
        }
        return true;
    }

    inline bool isRateControlledCodeSupported(const QMap<QString, DataItem> &items, const QString &diCode)
    {
        const int supportedRateCount = currentRateCount(items);
        int rateIndex = 0;
        if (isCurrentEnergyRateCode(diCode, &rateIndex) ||
            isDemandRateCode(diCode, &rateIndex) ||
            isFreezeRateCode(diCode, &rateIndex))
        {
            return rateIndex <= supportedRateCount;
        }

        return true;
    }

    inline bool isParameterStructureCodeSupported(const QMap<QString, DataItem> &items, const QString &diCode)
    {
        if (diCode.size() == 10 &&
            (diCode.startsWith(QStringLiteral("0x040501")) || diCode.startsWith(QStringLiteral("0x040502"))))
        {
            int rateIndex = 0;
            if (!parseHexByteValue(diCode.mid(8, 2), &rateIndex))
            {
                return false;
            }

            return rateIndex >= 1 && rateIndex <= currentRateCount(items);
        }

        if (diCode == QStringLiteral("0x04010000") || diCode == QStringLiteral("0x04020000"))
        {
            return true;
        }

        if (diCode.size() == 10 &&
            (diCode.startsWith(QStringLiteral("0x040100")) || diCode.startsWith(QStringLiteral("0x040200"))))
        {
            int tableIndex = 0;
            if (!parseHexByteValue(diCode.mid(8, 2), &tableIndex))
            {
                return false;
            }

            return tableIndex >= 1 && tableIndex <= currentDayScheduleTableCount(items);
        }

        if (diCode.size() == 10 && diCode.startsWith(QStringLiteral("0x0403")))
        {
            bool ok = false;
            const int holidayIndex = diCode.mid(6, 4).toInt(&ok, 16);
            return ok && holidayIndex >= 1 && holidayIndex <= currentHolidayCount(items);
        }

        return true;
    }

    inline void appendValuesForCodes(const QMap<QString, DataItem> &items, const QStringList &codes, QVariantList &values)
    {
        for (const QString &code : codes)
        {
            if (items.contains(code))
            {
                values.append(items.value(code).value);
            }
        }
    }

    inline QStringList currentEnergyBlockCodes(const QMap<QString, DataItem> &items, const QString &diCode)
    {
        const QString prefix = diCode.left(6);
        const int rateCount = currentRateCount(items);
        QStringList codes;
        codes.reserve(rateCount + 1);
        codes.append(prefix + QStringLiteral("0000"));
        for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
        {
            codes.append(prefix + upperHexByte(rateIndex) + QStringLiteral("00"));
        }
        return codes;
    }

    inline bool applyEnergyBlockValues(QMap<QString, DataItem> &items, const QString &blockCode, const QVariantList &values)
    {
        if (!blockCode.endsWith(QStringLiteral("FF00")) || !isMultiRateEnergyCode(blockCode))
        {
            return false;
        }

        const QStringList blockCodes = currentEnergyBlockCodes(items, blockCode);
        const int valueCount = qMin(values.size(), blockCodes.size());
        for (int valueIndex = 0; valueIndex < valueCount; ++valueIndex)
        {
            const QString &targetCode = blockCodes.at(valueIndex);
            if (!items.contains(targetCode))
            {
                continue;
            }

            items[targetCode].value = values.at(valueIndex);
        }

        return true;
    }

    inline QStringList currentDemandBlockCodes(const QMap<QString, DataItem> &items, const QString &diPrefix)
    {
        const int rateCount = currentRateCount(items);
        QStringList codes;
        codes.reserve((rateCount + 1) * 2);
        codes.append(diPrefix + QStringLiteral("0000"));
        codes.append(diPrefix + QStringLiteral("0001"));
        for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
        {
            const QString rateHex = upperHexByte(rateIndex);
            codes.append(diPrefix + rateHex + QStringLiteral("00"));
            codes.append(diPrefix + rateHex + QStringLiteral("01"));
        }
        return codes;
    }

    inline double defaultDemandRateValue(const DemandFamilyDefinition &family, int familyIndex, int rateIndex)
    {
        double baseValue = 9.5 - familyIndex * 0.08 - (rateIndex - 1) * 0.56;
        if (family.unit == QStringLiteral("kvar"))
        {
            baseValue *= 0.82;
        }
        else if (family.unit == QStringLiteral("kVA"))
        {
            baseValue *= 1.08;
        }

        if (family.description.startsWith(QStringLiteral("A相")))
        {
            baseValue *= 0.78;
        }
        else if (family.description.startsWith(QStringLiteral("B相")))
        {
            baseValue *= 0.64;
        }
        else if (family.description.startsWith(QStringLiteral("C相")))
        {
            baseValue *= 0.58;
        }

        return roundToDecimals(qMax(0.0, baseValue), 4);
    }

    inline void initializeCurrentDemandFamily(QMap<QString, DataItem> &items,
                                       const DemandFamilyDefinition &family,
                                       const QDateTime &now,
                                       int familyIndex)
    {
        const QString prefix = demandFamilyPrefix(family.prefixValue);
        items[prefix + QStringLiteral("0000")] = DataItem(prefix + QStringLiteral("0000"),
                                                          QStringLiteral("(当前)%1总最大需量").arg(family.description),
                                                          0.0,
                                                          family.unit,
                                                          QStringLiteral("AA,.,AA,AA"),
                                                          false,
                                                          4);
        items[prefix + QStringLiteral("0001")] = DataItem(prefix + QStringLiteral("0001"),
                                                          QStringLiteral("(当前)%1总最大需量发生时间").arg(family.description),
                                                          compactDateTimeToMinutes(now.addSecs(-(familyIndex + 1) * 6 * 60 * 60)),
                                                          QStringLiteral("-"),
                                                          QStringLiteral("AA,AA,AA,AA,AA"),
                                                          false,
                                                          0);

        for (int rateIndex = 1; rateIndex <= kDefaultRateCount; ++rateIndex)
        {
            const QString rateHex = upperHexByte(rateIndex);
            const QString valueCode = prefix + rateHex + QStringLiteral("00");
            const QString timeCode = prefix + rateHex + QStringLiteral("01");
            items[valueCode] = DataItem(valueCode,
                                        QStringLiteral("(当前)%1费率%2最大需量").arg(family.description).arg(rateIndex),
                                        defaultDemandRateValue(family, familyIndex, rateIndex),
                                        family.unit,
                                        QStringLiteral("AA,.,AA,AA"),
                                        false,
                                        4);
            items[timeCode] = DataItem(timeCode,
                                       QStringLiteral("(当前)%1费率%2最大需量发生时间").arg(family.description).arg(rateIndex),
                                       compactDateTimeToMinutes(now.addSecs(-(familyIndex * 10 + rateIndex) * 35 * 60)),
                                       QStringLiteral("-"),
                                       QStringLiteral("AA,AA,AA,AA,AA"),
                                       false,
                                       0);
        }

        items[prefix + QStringLiteral("FF00")] = DataItem(prefix + QStringLiteral("FF00"),
                                                          QStringLiteral("(当前)%1最大需量及发生时间数据块").arg(family.description),
                                                          QVariantList(),
                                                          QStringLiteral("-"),
                                                          repeatedFormat(QStringLiteral("AA,.,AA,AA:AA,AA,AA,AA,AA"), kDefaultRateCount + 1),
                                                          false,
                                                          0);
    }

    inline void ensureCurrentEnergyRateItems(QMap<QString, DataItem> &items)
    {
        const QList<QString> templateCodes = items.keys();
        for (const QString &templateCode : templateCodes)
        {
            if (!templateCode.endsWith(QStringLiteral("0100")) || !isMultiRateEnergyCode(templateCode))
            {
                continue;
            }

            const QString prefix = templateCode.left(6);
            const DataItem templateItem = items.value(templateCode);
            for (int rateIndex = 1; rateIndex <= kMaxRateCount; ++rateIndex)
            {
                const QString code = prefix + upperHexByte(rateIndex) + QStringLiteral("00");
                if (items.contains(code))
                {
                    continue;
                }

                items.insert(code, DataItem(code,
                                            rateDescription(templateItem.description, rateIndex),
                                            0.0,
                                            templateItem.unit,
                                            templateItem.format,
                                            templateItem.isWritable,
                                            templateItem.decimalPlaces));
            }
        }
    }

    inline void ensureDemandRateItems(QMap<QString, DataItem> &items)
    {
        const QList<QString> templateCodes = items.keys();
        for (const QString &valueTemplateCode : templateCodes)
        {
            if (!valueTemplateCode.endsWith(QStringLiteral("0100")) || !isDemandDataCode(valueTemplateCode))
            {
                continue;
            }

            const QString prefix = valueTemplateCode.left(6);
            const QString timeTemplateCode = prefix + QStringLiteral("0101");
            if (!items.contains(valueTemplateCode) || !items.contains(timeTemplateCode))
            {
                continue;
            }

            const DataItem valueTemplate = items.value(valueTemplateCode);
            const DataItem timeTemplate = items.value(timeTemplateCode);

            for (int rateIndex = 1; rateIndex <= kMaxRateCount; ++rateIndex)
            {
                const QString rateHex = upperHexByte(rateIndex);
                const QString valueCode = prefix + rateHex + QStringLiteral("00");
                const QString timeCode = prefix + rateHex + QStringLiteral("01");

                if (!items.contains(valueCode))
                {
                    items.insert(valueCode, DataItem(valueCode,
                                                     rateDescription(valueTemplate.description, rateIndex),
                                                     0.0,
                                                     valueTemplate.unit,
                                                     valueTemplate.format,
                                                     valueTemplate.isWritable,
                                                     valueTemplate.decimalPlaces));
                }

                if (!items.contains(timeCode))
                {
                    items.insert(timeCode, DataItem(timeCode,
                                                    rateDescription(timeTemplate.description, rateIndex),
                                                    timeTemplate.value,
                                                    timeTemplate.unit,
                                                    timeTemplate.format,
                                                    timeTemplate.isWritable,
                                                    timeTemplate.decimalPlaces));
                }
            }
        }
    }

    inline void ensureFreezeRateItems(QMap<QString, DataItem> &items)
    {
        const QList<QString> templateCodes = items.keys();
        for (const QString &templateCode : templateCodes)
        {
            if (!templateCode.endsWith(QStringLiteral("02")) || !isFreezeEnergyFamilyCode(templateCode))
            {
                continue;
            }

            const DataItem templateItem = items.value(templateCode);
            for (int rateIndex = 1; rateIndex <= kMaxRateCount; ++rateIndex)
            {
                const QString code = templateCode.left(8) + upperHexByte(rateIndex + 1);
                if (items.contains(code))
                {
                    continue;
                }

                items.insert(code, DataItem(code,
                                            rateDescription(templateItem.description, rateIndex),
                                            0.0,
                                            templateItem.unit,
                                            templateItem.format,
                                            templateItem.isWritable,
                                            templateItem.decimalPlaces));
            }
        }
    }

    inline void refreshRateDependentBlockFormats(QMap<QString, DataItem> &items)
    {
        const int rateCount = currentRateCount(items);

        const QList<QString> blockCodes = items.keys();
        for (const QString &blockCode : blockCodes)
        {
            if (!blockCode.endsWith(QStringLiteral("FF00")) || !isMultiRateEnergyCode(blockCode))
            {
                continue;
            }

            const QString prefix = blockCode.left(6);
            const QString templateCode = prefix + QStringLiteral("0100");
            if (items.contains(blockCode) && items.contains(templateCode))
            {
                items[blockCode].format = repeatedFormat(items[templateCode].format, rateCount + 1);
            }
        }

        const QList<QString> freezeBlockCodes = items.keys();
        for (const QString &blockCode : freezeBlockCodes)
        {
            if (!isFreezeEnergyBlockCode(blockCode))
            {
                continue;
            }

            const QString templateCode = blockCode.left(8) + QStringLiteral("02");
            if (items.contains(blockCode) && items.contains(templateCode))
            {
                items[blockCode].format = repeatedFormat(items[templateCode].format, rateCount + 1);
            }
        }

        const QString demandSegmentFormat = QStringLiteral("AA,.,AA,AA:AA,AA,AA,AA,AA");
        for (const QString &blockCode : blockCodes)
        {
            if (!blockCode.endsWith(QStringLiteral("FF00")) || !isDemandDataCode(blockCode))
            {
                continue;
            }

            const QString prefix = blockCode.left(6);
            if (items.contains(prefix + QStringLiteral("0100")))
            {
                items[blockCode].format = repeatedFormat(demandSegmentFormat, rateCount + 1);
            }
        }

    }
}

#endif // METERMODELPRIVATERATE_H
