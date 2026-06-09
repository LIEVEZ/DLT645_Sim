#ifndef METERMODELPRIVATECOMMON_H
#define METERMODELPRIVATECOMMON_H

#include "MeterModel.h"
#include <QDateTime>
#include <QRandomGenerator>
#include <QSet>
#include <cmath>

namespace MeterModelPrivate
{
    constexpr int kDefaultRateCount = 5;
    // 当前产品固定按 5 个费率处理，避免大费率数导致读取链路膨胀卡顿
    constexpr int kMaxRateCount = kDefaultRateCount;
    constexpr int kMaxYearZoneCount = 14;
    constexpr int kMaxDayScheduleTableCount = 8;
    constexpr int kMaxDayScheduleCount = 14;
    constexpr int kMaxHolidayCount = 254;
    constexpr int kMaxSettlementDayCount = 12;
    constexpr int kHarmonicOrderCount = 21;

    struct NumericFormatSpec
    {
        bool valid = false;
        bool allowNegative = false;
        int integerDigits = 0;
        int decimalDigits = 0;
    };

    struct DemandFamilyDefinition
    {
        int prefixValue = 0;
        QString description;
        QString unit;
    };

    QString compactDateTimeToMinutes(const QDateTime &dateTime);
    QString compactDateTimeToSeconds(const QDateTime &dateTime);

    inline bool isSupportedNumericToken(const QString &token)
    {
        return token == QStringLiteral("AA") || token == QStringLiteral("xx");
    }

    inline bool isDataBlockItem(const DataItem &item)
    {
        return item.format.contains(':') || item.diCode.endsWith(QStringLiteral("FF00"));
    }

    inline NumericFormatSpec parseNumericFormatSpec(const QString &format)
    {
        NumericFormatSpec spec;
        if (format.isEmpty() || format.contains(':'))
        {
            return spec;
        }

        QStringList tokens = format.split(',', Qt::SkipEmptyParts);
        for (QString &token : tokens)
        {
            token = token.trimmed();
        }

        if (tokens.isEmpty())
        {
            return spec;
        }

        spec.allowNegative = (tokens.first() == QStringLiteral("F9"));

        QStringList nonF9Tokens;
        for (const QString &token : tokens)
        {
            if (token == QStringLiteral("F9"))
            {
                continue;
            }

            if (token == QStringLiteral(".") || isSupportedNumericToken(token))
            {
                nonF9Tokens.append(token);
                continue;
            }

            return NumericFormatSpec();
        }

        if (nonF9Tokens.isEmpty())
        {
            return spec;
        }

        const int decimalIndex = nonF9Tokens.indexOf(QStringLiteral("."));
        const int xxCount = nonF9Tokens.count(QStringLiteral("xx"));

        if (xxCount > 1)
        {
            return NumericFormatSpec();
        }

        if (decimalIndex != -1)
        {
            int digitsBeforeDecimal = 0;
            int digitsAfterDecimal = 0;

            for (int i = 0; i < nonF9Tokens.size(); ++i)
            {
                if (nonF9Tokens[i] == QStringLiteral("."))
                {
                    continue;
                }

                if (i < decimalIndex)
                {
                    digitsBeforeDecimal += 2;
                }
                else
                {
                    digitsAfterDecimal += 2;
                }
            }

            spec.valid = true;
            spec.integerDigits = digitsBeforeDecimal;
            if (spec.allowNegative && decimalIndex == 0)
            {
                spec.integerDigits += 2;
            }
            spec.decimalDigits = digitsAfterDecimal;
            return spec;
        }

        if (xxCount == 1)
        {
            const int xxIndex = nonF9Tokens.indexOf(QStringLiteral("xx"));
            int totalDigits = 0;
            int digitsAfterXx = 0;

            for (int i = 0; i < nonF9Tokens.size(); ++i)
            {
                if (nonF9Tokens[i] == QStringLiteral("."))
                {
                    continue;
                }

                totalDigits += 2;
                if (i > xxIndex)
                {
                    digitsAfterXx += 2;
                }
            }

            if (spec.allowNegative)
            {
                totalDigits += 2;
            }

            spec.valid = true;
            spec.decimalDigits = digitsAfterXx + 1;
            spec.integerDigits = qMax(0, totalDigits - spec.decimalDigits);
            return spec;
        }

        int totalDigits = 0;
        for (const QString &token : nonF9Tokens)
        {
            if (token != QStringLiteral("."))
            {
                totalDigits += 2;
            }
        }

        spec.valid = (totalDigits > 0);
        spec.integerDigits = totalDigits;
        spec.decimalDigits = 0;
        return spec;
    }

    inline double maxRepresentableValue(const NumericFormatSpec &spec)
    {
        const double scale = std::pow(10.0, spec.decimalDigits);
        const double step = (scale > 1.0) ? (1.0 / scale) : 1.0;
        return std::pow(10.0, spec.integerDigits) - step;
    }

    inline QVariant normalizeValueForFormat(const QVariant &value, const NumericFormatSpec &spec)
    {
        bool ok = false;
        double numericValue = value.toDouble(&ok);
        if (!ok || !spec.valid)
        {
            return value;
        }

        const double scale = std::pow(10.0, spec.decimalDigits);
        const double maxValue = maxRepresentableValue(spec);
        const double minValue = spec.allowNegative ? -maxValue : 0.0;

        double normalizedValue = numericValue;
        if (scale > 1.0)
        {
            normalizedValue = std::round(normalizedValue * scale) / scale;
        }
        else
        {
            normalizedValue = std::round(normalizedValue);
        }

        normalizedValue = qBound(minValue, normalizedValue, maxValue);

        if (spec.decimalDigits == 0)
        {
            return static_cast<qint64>(std::llround(normalizedValue));
        }

        return normalizedValue;
    }

    inline double roundToDecimals(double value, int decimalPlaces)
    {
        if (decimalPlaces <= 0)
        {
            return std::round(value);
        }

        const double scale = std::pow(10.0, decimalPlaces);
        return std::round(value * scale) / scale;
    }

    inline QString upperHexByte(int value)
    {
        return QStringLiteral("%1").arg(value, 2, 16, QChar('0')).toUpper();
    }

    inline QString repeatedByteFormat(int byteCount)
    {
        QStringList tokens;
        tokens.reserve(byteCount);
        for (int i = 0; i < byteCount; ++i)
        {
            tokens.append(QStringLiteral("AA"));
        }
        return tokens.join(QStringLiteral(","));
    }

    inline QString repeatedFormatParts(const QString &part, int count)
    {
        QStringList parts;
        parts.reserve(count);
        for (int i = 0; i < count; ++i)
        {
            parts.append(part);
        }
        return parts.join(QStringLiteral(":"));
    }

    inline QString buildDayScheduleTableValue(int seed, int setIndex, int tableIndex)
    {
        Q_UNUSED(seed);
        Q_UNUSED(setIndex);

        static const QVector<QStringList> tableTemplates = {
            {QStringLiteral("000003"), QStringLiteral("040004"), QStringLiteral("080002"), QStringLiteral("110003"), QStringLiteral("130004"), QStringLiteral("170003"), QStringLiteral("190002")},
            {QStringLiteral("000003"), QStringLiteral("040004"), QStringLiteral("080002"), QStringLiteral("110003"), QStringLiteral("130004"), QStringLiteral("140005"), QStringLiteral("160004"), QStringLiteral("170003"), QStringLiteral("190002")},
            {QStringLiteral("000003"), QStringLiteral("040004"), QStringLiteral("080002"), QStringLiteral("110003"), QStringLiteral("130004"), QStringLiteral("140005"), QStringLiteral("160004"), QStringLiteral("170003"), QStringLiteral("190002"), QStringLiteral("210001"), QStringLiteral("230002")},
            {QStringLiteral("000003"), QStringLiteral("040004"), QStringLiteral("080002"), QStringLiteral("110003"), QStringLiteral("130004"), QStringLiteral("170003"), QStringLiteral("190001"), QStringLiteral("210002")}};

        const QStringList &templateValues = tableTemplates.at(tableIndex % tableTemplates.size());
        QString value;
        value.reserve(kMaxDayScheduleCount * 6);
        for (int slotIndex = 0; slotIndex < kMaxDayScheduleCount; ++slotIndex)
        {
            value += templateValues.at(qMin(slotIndex, templateValues.size() - 1));
        }
        return value;
    }

    inline QString buildYearZoneEntryValue(int seed, int setIndex, int zoneIndex)
    {
        Q_UNUSED(seed);
        Q_UNUSED(setIndex);

        static const QStringList yearZoneTemplates = {
            QStringLiteral("010104"),
            QStringLiteral("020101"),
            QStringLiteral("050102"),
            QStringLiteral("070103"),
            QStringLiteral("080102"),
            QStringLiteral("090101"),
            QStringLiteral("110104"),
            QStringLiteral("120104")};

        const QString templateValue = yearZoneTemplates.at(qMin(zoneIndex, yearZoneTemplates.size() - 1));
        const int month = templateValue.mid(0, 2).toInt();
        const int day = templateValue.mid(2, 2).toInt();
        const int zoneNumber = templateValue.mid(4, 2).toInt();
        return QStringLiteral("%1%2%3")
            .arg(month, 2, 10, QChar('0'))
            .arg(day, 2, 10, QChar('0'))
            .arg(zoneNumber, 2, 10, QChar('0'));
    }

    inline QString buildHolidayEntryValue(const QDate &date, int scheduleTableIndex)
    {
        return QStringLiteral("%1%2")
            .arg(date.toString(QStringLiteral("yyMMdd")))
            .arg(scheduleTableIndex, 2, 10, QChar('0'));
    }

    inline QString buildDisplayScreenEntryValue(const QString &targetDiCode, int splitScreenFlag)
    {
        QString diCode = targetDiCode;
        diCode.remove(QStringLiteral("0x"));
        diCode = diCode.toUpper().rightJustified(8, QChar('0'));
        return diCode + QStringLiteral("%1").arg(splitScreenFlag, 2, 10, QChar('0'));
    }

    inline QString phaseNameForIndex(int phaseIndex)
    {
        switch (phaseIndex)
        {
        case 1:
            return QStringLiteral("A相");
        case 2:
            return QStringLiteral("B相");
        case 3:
            return QStringLiteral("C相");
        default:
            return QStringLiteral("未知相");
        }
    }

    inline int normalizedRateCount(int rawRateCount)
    {
        return qBound(1, rawRateCount, kMaxRateCount);
    }

    inline QVariant normalizedParameterSettingValue(const QString &diCode, const QVariant &value)
    {
        bool ok = false;
        const int rawValue = value.toInt(&ok);
        if (!ok)
        {
            return value;
        }

        if (diCode == QStringLiteral("0x04000201"))
        {
            return qBound(1, rawValue, kMaxYearZoneCount);
        }

        if (diCode == QStringLiteral("0x04000202"))
        {
            return qBound(1, rawValue, kMaxDayScheduleTableCount);
        }

        if (diCode == QStringLiteral("0x04000203"))
        {
            return qBound(1, rawValue, kMaxDayScheduleCount);
        }

        if (diCode == QStringLiteral("0x04000204"))
        {
            return normalizedRateCount(rawValue);
        }

        if (diCode == QStringLiteral("0x04000205"))
        {
            return qBound(0, rawValue, kMaxHolidayCount);
        }

        return value;
    }

    inline int currentRateCount(const QMap<QString, DataItem> &items)
    {
        if (!items.contains(QStringLiteral("0x04000204")))
        {
            return kDefaultRateCount;
        }

        bool ok = false;
        const int rawRateCount = items.value(QStringLiteral("0x04000204")).value.toInt(&ok);
        return normalizedRateCount(ok ? rawRateCount : kDefaultRateCount);
    }

    inline int boundedParameterCount(const QMap<QString, DataItem> &items,
                              const QString &diCode,
                              int defaultValue,
                              int minimumValue,
                              int maximumValue)
    {
        if (!items.contains(diCode))
        {
            return defaultValue;
        }

        bool ok = false;
        const int rawValue = items.value(diCode).value.toInt(&ok);
        return qBound(minimumValue, ok ? rawValue : defaultValue, maximumValue);
    }

    inline int currentYearZoneCount(const QMap<QString, DataItem> &items)
    {
        return boundedParameterCount(items, QStringLiteral("0x04000201"), 2, 1, kMaxYearZoneCount);
    }

    inline int currentDayScheduleTableCount(const QMap<QString, DataItem> &items)
    {
        return boundedParameterCount(items, QStringLiteral("0x04000202"), 2, 1, kMaxDayScheduleTableCount);
    }

    inline int currentDayScheduleCount(const QMap<QString, DataItem> &items)
    {
        return boundedParameterCount(items, QStringLiteral("0x04000203"), 8, 1, kMaxDayScheduleCount);
    }

    inline int currentHolidayCount(const QMap<QString, DataItem> &items)
    {
        return boundedParameterCount(items, QStringLiteral("0x04000205"), 12, 0, kMaxHolidayCount);
    }

    inline QString normalizedYearZoneEntryValue(const QString &value, int supportedTableCount, int fallbackSeed, int setIndex, int zoneIndex)
    {
        QString normalized = value;
        if (normalized.size() < 6)
        {
            normalized = buildYearZoneEntryValue(fallbackSeed, setIndex, zoneIndex);
        }

        bool ok = false;
        const int tableIndex = normalized.mid(4, 2).toInt(&ok);
        const int clampedTableIndex = qBound(1, ok ? tableIndex : 1, supportedTableCount);
        normalized.replace(4, 2, QStringLiteral("%1").arg(clampedTableIndex, 2, 10, QChar('0')));
        return normalized.left(6);
    }

    inline QString normalizedDayScheduleEntryValue(const QString &value, int supportedRateCount, int fallbackSeed, int setIndex, int tableIndex, int slotIndex)
    {
        QString normalized = value;
        if (normalized.size() < 6)
        {
            const QString generated = buildDayScheduleTableValue(fallbackSeed, setIndex, tableIndex);
            normalized = generated.mid(slotIndex * 6, 6);
        }

        bool ok = false;
        const int rateIndex = normalized.mid(4, 2).toInt(&ok);
        const int clampedRateIndex = qBound(1, ok ? rateIndex : 1, supportedRateCount);
        normalized.replace(4, 2, QStringLiteral("%1").arg(clampedRateIndex, 2, 10, QChar('0')));
        return normalized.left(6);
    }

    inline QString normalizedHolidayEntryValue(const QString &value, int supportedTableCount, const QDate &fallbackDate)
    {
        QString normalized = value;
        if (normalized.size() < 8)
        {
            normalized = buildHolidayEntryValue(fallbackDate, 1);
        }

        bool ok = false;
        const int tableIndex = normalized.mid(6, 2).toInt(&ok);
        const int clampedTableIndex = qBound(1, ok ? tableIndex : 1, supportedTableCount);
        normalized.replace(6, 2, QStringLiteral("%1").arg(clampedTableIndex, 2, 10, QChar('0')));
        return normalized.left(8);
    }

    inline void refreshParameterScheduleData(QMap<QString, DataItem> &items)
    {
        const int supportedYearZoneCount = currentYearZoneCount(items);
        const int supportedDayScheduleTableCount = currentDayScheduleTableCount(items);
        const int supportedDayScheduleCount = currentDayScheduleCount(items);
        const int supportedRateCount = currentRateCount(items);
        const struct
        {
            QString blockCode;
            int setIndex;
            int seed;
        } yearZoneBlocks[] = {
            {QStringLiteral("0x04010000"), 0, 11},
            {QStringLiteral("0x04020000"), 1, 37}};

        for (const auto &block : yearZoneBlocks)
        {
            if (!items.contains(block.blockCode))
            {
                continue;
            }

            const QVariantList existingValues = items.value(block.blockCode).value.toList();
            QVariantList normalizedValues;
            normalizedValues.reserve(supportedYearZoneCount);
            for (int zoneIndex = 0; zoneIndex < supportedYearZoneCount; ++zoneIndex)
            {
                const QString existingValue = zoneIndex < existingValues.size() ? existingValues.at(zoneIndex).toString() : QString();
                normalizedValues.append(normalizedYearZoneEntryValue(existingValue,
                                                                     supportedDayScheduleTableCount,
                                                                     block.seed,
                                                                     block.setIndex,
                                                                     zoneIndex));
            }

            items[block.blockCode].value = normalizedValues;
            items[block.blockCode].format = repeatedFormatParts(QStringLiteral("AA,AA,AA"), supportedYearZoneCount);
        }

        for (int setIndex = 0; setIndex < 2; ++setIndex)
        {
            const QString prefix = setIndex == 0 ? QStringLiteral("0x040100") : QStringLiteral("0x040200");
            const int seed = setIndex == 0 ? 19 : 43;

            for (int tableIndex = 0; tableIndex < kMaxDayScheduleTableCount; ++tableIndex)
            {
                const QString diCode = prefix + upperHexByte(tableIndex + 1);
                if (!items.contains(diCode))
                {
                    continue;
                }

                const QVariantList existingValues = items.value(diCode).value.toList();
                QVariantList normalizedValues;
                normalizedValues.reserve(supportedDayScheduleCount);
                for (int slotIndex = 0; slotIndex < supportedDayScheduleCount; ++slotIndex)
                {
                    const QString existingValue = slotIndex < existingValues.size() ? existingValues.at(slotIndex).toString() : QString();
                    normalizedValues.append(normalizedDayScheduleEntryValue(existingValue,
                                                                            supportedRateCount,
                                                                            seed,
                                                                            setIndex,
                                                                            tableIndex,
                                                                            slotIndex));
                }

                items[diCode].value = normalizedValues;
                items[diCode].format = repeatedFormatParts(QStringLiteral("AA,AA,AA"), supportedDayScheduleCount);
            }
        }

        for (int holidayIndex = 0; holidayIndex < kMaxHolidayCount; ++holidayIndex)
        {
            const QString diCode = QStringLiteral("0x0403%1").arg(holidayIndex + 1, 4, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
            if (!items.contains(diCode))
            {
                continue;
            }

            const QDate fallbackDate(QDate::currentDate().year(), (holidayIndex % 12) + 1, qMin(holidayIndex + 1, 28));
            items[diCode].value = normalizedHolidayEntryValue(items.value(diCode).value.toString(),
                                                              supportedDayScheduleTableCount,
                                                              fallbackDate);
        }
    }

    [[maybe_unused]] inline const QStringList &currentEnergyPrefixes()
    {
        static const QStringList prefixes = {
            QStringLiteral("0x0000"),
            QStringLiteral("0x0001"),
            QStringLiteral("0x0002"),
            QStringLiteral("0x0003"),
            QStringLiteral("0x0004"),
            QStringLiteral("0x0005"),
            QStringLiteral("0x0006"),
            QStringLiteral("0x0007"),
            QStringLiteral("0x0008"),
            QStringLiteral("0x0009"),
            QStringLiteral("0x000A")};
        return prefixes;
    }

    inline const QList<DemandFamilyDefinition> &currentDemandFamilies()
    {
        static const QList<DemandFamilyDefinition> families = {
            {0x0101, QStringLiteral("正向有功"), QStringLiteral("kW")},
            {0x0102, QStringLiteral("反向有功"), QStringLiteral("kW")},
            {0x0103, QStringLiteral("组合无功1"), QStringLiteral("kvar")},
            {0x0104, QStringLiteral("组合无功2"), QStringLiteral("kvar")},
            {0x0105, QStringLiteral("第一象限无功"), QStringLiteral("kvar")},
            {0x0106, QStringLiteral("第二象限无功"), QStringLiteral("kvar")},
            {0x0107, QStringLiteral("第三象限无功"), QStringLiteral("kvar")},
            {0x0108, QStringLiteral("第四象限无功"), QStringLiteral("kvar")},
            {0x0109, QStringLiteral("正向视在"), QStringLiteral("kVA")},
            {0x010A, QStringLiteral("反向视在"), QStringLiteral("kVA")},
            {0x0115, QStringLiteral("A相正向有功"), QStringLiteral("kW")},
            {0x0116, QStringLiteral("A相反向有功"), QStringLiteral("kW")},
            {0x0117, QStringLiteral("A相组合无功1"), QStringLiteral("kvar")},
            {0x0118, QStringLiteral("A相组合无功2"), QStringLiteral("kvar")},
            {0x0119, QStringLiteral("A相第一象限无功"), QStringLiteral("kvar")},
            {0x011A, QStringLiteral("A相第二象限无功"), QStringLiteral("kvar")},
            {0x011B, QStringLiteral("A相第三象限无功"), QStringLiteral("kvar")},
            {0x011C, QStringLiteral("A相第四象限无功"), QStringLiteral("kvar")},
            {0x011D, QStringLiteral("A相正向视在"), QStringLiteral("kVA")},
            {0x011E, QStringLiteral("A相反向视在"), QStringLiteral("kVA")},
            {0x0194, QStringLiteral("A相关联"), QStringLiteral("kW")},
            {0x0195, QStringLiteral("A相正向有功基波"), QStringLiteral("kW")},
            {0x0196, QStringLiteral("A相反向有功基波"), QStringLiteral("kW")},
            {0x0197, QStringLiteral("A相正向有功谐波"), QStringLiteral("kW")},
            {0x0198, QStringLiteral("A相反向有功谐波"), QStringLiteral("kW")},
            {0x0199, QStringLiteral("A相铜损有功补偿量"), QStringLiteral("kW")},
            {0x019A, QStringLiteral("A相铁损有功补偿量"), QStringLiteral("kW")},
            {0x0129, QStringLiteral("B相正向有功"), QStringLiteral("kW")},
            {0x012A, QStringLiteral("B相反向有功"), QStringLiteral("kW")},
            {0x012B, QStringLiteral("B相组合无功1"), QStringLiteral("kvar")},
            {0x012C, QStringLiteral("B相组合无功2"), QStringLiteral("kvar")},
            {0x012D, QStringLiteral("B相第一象限无功"), QStringLiteral("kvar")},
            {0x012E, QStringLiteral("B相第二象限无功"), QStringLiteral("kvar")},
            {0x012F, QStringLiteral("B相第三象限无功"), QStringLiteral("kvar")},
            {0x0130, QStringLiteral("B相第四象限无功"), QStringLiteral("kvar")},
            {0x0131, QStringLiteral("B相正向视在"), QStringLiteral("kVA")},
            {0x0132, QStringLiteral("B相反向视在"), QStringLiteral("kVA")},
            {0x01A8, QStringLiteral("B相关联"), QStringLiteral("kW")},
            {0x01A9, QStringLiteral("B相正向有功基波"), QStringLiteral("kW")},
            {0x01AA, QStringLiteral("B相反向有功基波"), QStringLiteral("kW")},
            {0x01AB, QStringLiteral("B相正向有功谐波"), QStringLiteral("kW")},
            {0x01AC, QStringLiteral("B相反向有功谐波"), QStringLiteral("kW")},
            {0x01AD, QStringLiteral("B相铜损有功补偿量"), QStringLiteral("kW")},
            {0x01AE, QStringLiteral("B相铁损有功补偿量"), QStringLiteral("kW")},
            {0x013D, QStringLiteral("C相正向有功"), QStringLiteral("kW")},
            {0x013E, QStringLiteral("C相反向有功"), QStringLiteral("kW")},
            {0x013F, QStringLiteral("C相组合无功1"), QStringLiteral("kvar")},
            {0x0140, QStringLiteral("C相组合无功2"), QStringLiteral("kvar")},
            {0x0141, QStringLiteral("C相第一象限无功"), QStringLiteral("kvar")},
            {0x0142, QStringLiteral("C相第二象限无功"), QStringLiteral("kvar")},
            {0x0143, QStringLiteral("C相第三象限无功"), QStringLiteral("kvar")},
            {0x0144, QStringLiteral("C相第四象限无功"), QStringLiteral("kvar")},
            {0x0145, QStringLiteral("C相正向视在"), QStringLiteral("kVA")},
            {0x0146, QStringLiteral("C相反向视在"), QStringLiteral("kVA")},
            {0x01BC, QStringLiteral("C相关联"), QStringLiteral("kW")},
            {0x01BD, QStringLiteral("C相正向有功基波"), QStringLiteral("kW")},
            {0x01BE, QStringLiteral("C相反向有功基波"), QStringLiteral("kW")},
            {0x01BF, QStringLiteral("C相正向有功谐波"), QStringLiteral("kW")},
            {0x01C0, QStringLiteral("C相反向有功谐波"), QStringLiteral("kW")},
            {0x01C1, QStringLiteral("C相铜损有功补偿量"), QStringLiteral("kW")},
            {0x01C2, QStringLiteral("C相铁损有功补偿量"), QStringLiteral("kW")}};
        return families;
    }

    inline const QSet<int> &demandBaseFamilyValues()
    {
        static const QSet<int> values = []() {
            QSet<int> result;
            for (const DemandFamilyDefinition &family : currentDemandFamilies())
            {
                result.insert(family.prefixValue);
            }
            return result;
        }();
        return values;
    }

    inline const QList<int> &multiRateEnergyBaseFamilies()
    {
        static const QList<int> families = {
            0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005,
            0x0006, 0x0007, 0x0008, 0x0009, 0x000A};
        return families;
    }

    inline const QList<int> &historicalTotalOnlyEnergyBaseFamilies()
    {
        static const QList<int> families = {
            0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086,
            0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E,
            0x0094, 0x0095, 0x0096, 0x0097, 0x0098, 0x0099, 0x009A,
            0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F, 0x0030, 0x0031, 0x0032,
            0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE,
            0x003D, 0x003E, 0x003F, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046,
            0x00BC, 0x00BD, 0x00BE, 0x00BF, 0x00C0, 0x00C1, 0x00C2};
        return families;
    }

    inline QString fourDigitHex(int value)
    {
        return QStringLiteral("%1").arg(value, 4, 16, QChar('0')).toUpper();
    }

    inline QString demandFamilyPrefix(int prefixValue)
    {
        return QStringLiteral("0x") + fourDigitHex(prefixValue);
    }

    inline QString settlementDayDemandPrefix(int baseFamilyValue, int settlementDay)
    {
        return QStringLiteral("0x") + fourDigitHex(baseFamilyValue + settlementDay * 0x1000);
    }

    inline int prefixedCodeValue(const QString &diCode, bool *ok = nullptr)
    {
        bool localOk = false;
        const int value = diCode.mid(2, 4).toInt(&localOk, 16);
        if (ok)
        {
            *ok = localOk;
        }
        return value;
    }

    inline bool isValidSettlementDayIndex(int settlementDay)
    {
        return settlementDay >= 0 && settlementDay <= kMaxSettlementDayCount;
    }

    inline int settlementDayFromPrefixValue(int prefixValue)
    {
        return prefixValue / 0x1000;
    }

    inline int energyBaseFamilyFromPrefixValue(int prefixValue)
    {
        return prefixValue % 0x1000;
    }

    inline bool isMultiRateEnergyBaseFamily(int familyValue)
    {
        return multiRateEnergyBaseFamilies().contains(familyValue);
    }

    inline bool isHistoricalTotalOnlyEnergyBaseFamily(int familyValue)
    {
        return historicalTotalOnlyEnergyBaseFamilies().contains(familyValue);
    }

    inline bool isEnergyFamilyPrefixValue(int prefixValue)
    {
        const int settlementDay = settlementDayFromPrefixValue(prefixValue);
        if (!isValidSettlementDayIndex(settlementDay))
        {
            return false;
        }

        const int familyValue = energyBaseFamilyFromPrefixValue(prefixValue);
        return isMultiRateEnergyBaseFamily(familyValue) || isHistoricalTotalOnlyEnergyBaseFamily(familyValue);
    }

    inline bool isMultiRateEnergyFamilyPrefixValue(int prefixValue)
    {
        const int settlementDay = settlementDayFromPrefixValue(prefixValue);
        return isValidSettlementDayIndex(settlementDay) &&
               isMultiRateEnergyBaseFamily(energyBaseFamilyFromPrefixValue(prefixValue));
    }

    inline bool isEnergyDataCode(const QString &diCode)
    {
        if (diCode.size() != 10 || !diCode.startsWith(QStringLiteral("0x")))
        {
            return false;
        }

        bool ok = false;
        const int prefixValue = prefixedCodeValue(diCode, &ok);
        return ok && isEnergyFamilyPrefixValue(prefixValue);
    }

    inline bool isMultiRateEnergyCode(const QString &diCode)
    {
        if (diCode.size() != 10 || !diCode.startsWith(QStringLiteral("0x")))
        {
            return false;
        }

        bool ok = false;
        const int prefixValue = prefixedCodeValue(diCode, &ok);
        return ok && isMultiRateEnergyFamilyPrefixValue(prefixValue);
    }

    inline bool isHistoricalEnergyCode(const QString &diCode)
    {
        if (!isEnergyDataCode(diCode))
        {
            return false;
        }

        bool ok = false;
        return prefixedCodeValue(diCode, &ok) >= 0x1000 && ok;
    }

    inline bool isDemandFamilyPrefixValue(int prefixValue)
    {
        const int settlementDay = settlementDayFromPrefixValue(prefixValue);
        if (!isValidSettlementDayIndex(settlementDay))
        {
            return false;
        }

        return demandBaseFamilyValues().contains(prefixValue % 0x1000);
    }

    inline bool isDemandDataCode(const QString &diCode)
    {
        if (diCode.size() != 10 || !diCode.startsWith(QStringLiteral("0x")))
        {
            return false;
        }

        bool ok = false;
        const int prefixValue = prefixedCodeValue(diCode, &ok);
        return ok && isDemandFamilyPrefixValue(prefixValue);
    }

    inline QString settlementDayDescriptionPrefix(int settlementDay)
    {
        return settlementDay == 0
                   ? QStringLiteral("(当前)")
                   : QStringLiteral("(上%1次结算日)").arg(settlementDay);
    }

    inline QString settlementDayDescriptionFromCurrent(const QString &currentDescription, int settlementDay)
    {
        QString description = currentDescription;
        description.replace(QStringLiteral("(当前)"), settlementDayDescriptionPrefix(settlementDay));
        return description;
    }

    inline QString settlementDayEnergyPrefix(int baseFamilyValue, int settlementDay)
    {
        return QStringLiteral("0x") + fourDigitHex(baseFamilyValue + settlementDay * 0x1000);
    }

    inline DataItem clonedDataItem(const DataItem &templateItem,
                            const QString &newCode,
                            const QString &newDescription,
                            const QVariant &newValue)
    {
        return DataItem(newCode,
                        newDescription,
                        newValue,
                        templateItem.unit,
                        templateItem.format,
                        templateItem.isWritable,
                        templateItem.decimalPlaces);
    }

    inline void initializeSettlementDayEnergyData(QMap<QString, DataItem> &items)
    {
        for (int settlementDay = 1; settlementDay <= kMaxSettlementDayCount; ++settlementDay)
        {
            for (const int baseFamilyValue : multiRateEnergyBaseFamilies())
            {
                const QString currentPrefix = settlementDayEnergyPrefix(baseFamilyValue, 0);
                const QString historicalPrefix = settlementDayEnergyPrefix(baseFamilyValue, settlementDay);
                const QString currentTotalCode = currentPrefix + QStringLiteral("0000");
                const QString currentRateTemplateCode = currentPrefix + QStringLiteral("0100");
                const QString currentBlockCode = currentPrefix + QStringLiteral("FF00");

                if (items.contains(currentTotalCode) && !items.contains(historicalPrefix + QStringLiteral("0000")))
                {
                    const DataItem templateItem = items.value(currentTotalCode);
                    items.insert(historicalPrefix + QStringLiteral("0000"),
                                 clonedDataItem(templateItem,
                                                historicalPrefix + QStringLiteral("0000"),
                                                settlementDayDescriptionFromCurrent(templateItem.description, settlementDay),
                                                templateItem.value));
                }

                if (items.contains(currentRateTemplateCode) && !items.contains(historicalPrefix + QStringLiteral("0100")))
                {
                    const DataItem templateItem = items.value(currentRateTemplateCode);
                    items.insert(historicalPrefix + QStringLiteral("0100"),
                                 clonedDataItem(templateItem,
                                                historicalPrefix + QStringLiteral("0100"),
                                                settlementDayDescriptionFromCurrent(templateItem.description, settlementDay),
                                                templateItem.value));
                }

                if (items.contains(currentBlockCode) && !items.contains(historicalPrefix + QStringLiteral("FF00")))
                {
                    const DataItem templateItem = items.value(currentBlockCode);
                    items.insert(historicalPrefix + QStringLiteral("FF00"),
                                 clonedDataItem(templateItem,
                                                historicalPrefix + QStringLiteral("FF00"),
                                                settlementDayDescriptionFromCurrent(templateItem.description, settlementDay),
                                                QVariantList()));
                }
            }

            for (const int baseFamilyValue : historicalTotalOnlyEnergyBaseFamilies())
            {
                const QString currentCode = settlementDayEnergyPrefix(baseFamilyValue, 0) + QStringLiteral("0000");
                const QString historicalCode = settlementDayEnergyPrefix(baseFamilyValue, settlementDay) + QStringLiteral("0000");
                if (!items.contains(currentCode) || items.contains(historicalCode))
                {
                    continue;
                }

                const DataItem templateItem = items.value(currentCode);
                items.insert(historicalCode,
                             clonedDataItem(templateItem,
                                            historicalCode,
                                            settlementDayDescriptionFromCurrent(templateItem.description, settlementDay),
                                            templateItem.value));
            }
        }
    }

    inline void initializeSettlementDayDemandData(QMap<QString, DataItem> &items)
    {
        for (int settlementDay = 1; settlementDay <= kMaxSettlementDayCount; ++settlementDay)
        {
            for (const DemandFamilyDefinition &family : currentDemandFamilies())
            {
                const QString currentPrefix = settlementDayDemandPrefix(family.prefixValue, 0);
                const QString historicalPrefix = settlementDayDemandPrefix(family.prefixValue, settlementDay);

                const QStringList suffixes = {
                    QStringLiteral("0000"),
                    QStringLiteral("0001"),
                    QStringLiteral("0100"),
                    QStringLiteral("0101"),
                    QStringLiteral("FF00")};

                for (const QString &suffix : suffixes)
                {
                    const QString currentCode = currentPrefix + suffix;
                    const QString historicalCode = historicalPrefix + suffix;
                    if (!items.contains(currentCode) || items.contains(historicalCode))
                    {
                        continue;
                    }

                    const DataItem templateItem = items.value(currentCode);
                    items.insert(historicalCode,
                                 clonedDataItem(templateItem,
                                                historicalCode,
                                                settlementDayDescriptionFromCurrent(templateItem.description, settlementDay),
                                                suffix == QStringLiteral("FF00") ? QVariantList() : templateItem.value));
                }
            }
        }
    }

}

#endif // METERMODELPRIVATECOMMON_H
