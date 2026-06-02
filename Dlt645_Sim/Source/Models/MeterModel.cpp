#include "MeterModel.h"
#include <QRandomGenerator>
#include <QDateTime>
#include <QSet>
#include <cmath>

namespace
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

    bool isSupportedNumericToken(const QString &token)
    {
        return token == QStringLiteral("AA") || token == QStringLiteral("xx");
    }

    bool isDataBlockItem(const DataItem &item)
    {
        return item.format.contains(':') || item.diCode.endsWith(QStringLiteral("FF00"));
    }

    NumericFormatSpec parseNumericFormatSpec(const QString &format)
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

    double maxRepresentableValue(const NumericFormatSpec &spec)
    {
        const double scale = std::pow(10.0, spec.decimalDigits);
        const double step = (scale > 1.0) ? (1.0 / scale) : 1.0;
        return std::pow(10.0, spec.integerDigits) - step;
    }

    QVariant normalizeValueForFormat(const QVariant &value, const NumericFormatSpec &spec)
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

    double roundToDecimals(double value, int decimalPlaces)
    {
        if (decimalPlaces <= 0)
        {
            return std::round(value);
        }

        const double scale = std::pow(10.0, decimalPlaces);
        return std::round(value * scale) / scale;
    }

    QString upperHexByte(int value)
    {
        return QStringLiteral("%1").arg(value, 2, 16, QChar('0')).toUpper();
    }

    QString repeatedByteFormat(int byteCount)
    {
        QStringList tokens;
        tokens.reserve(byteCount);
        for (int i = 0; i < byteCount; ++i)
        {
            tokens.append(QStringLiteral("AA"));
        }
        return tokens.join(QStringLiteral(","));
    }

    QString repeatedFormatParts(const QString &part, int count)
    {
        QStringList parts;
        parts.reserve(count);
        for (int i = 0; i < count; ++i)
        {
            parts.append(part);
        }
        return parts.join(QStringLiteral(":"));
    }

    QString buildDayScheduleTableValue(int seed, int setIndex, int tableIndex)
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

    QString buildYearZoneEntryValue(int seed, int setIndex, int zoneIndex)
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

    QString buildHolidayEntryValue(const QDate &date, int scheduleTableIndex)
    {
        return QStringLiteral("%1%2")
            .arg(date.toString(QStringLiteral("yyMMdd")))
            .arg(scheduleTableIndex, 2, 10, QChar('0'));
    }

    QString buildDisplayScreenEntryValue(const QString &targetDiCode, int splitScreenFlag)
    {
        QString diCode = targetDiCode;
        diCode.remove(QStringLiteral("0x"));
        diCode = diCode.toUpper().rightJustified(8, QChar('0'));
        return diCode + QStringLiteral("%1").arg(splitScreenFlag, 2, 10, QChar('0'));
    }

    QString phaseNameForIndex(int phaseIndex)
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

    int normalizedRateCount(int rawRateCount)
    {
        return qBound(1, rawRateCount, kMaxRateCount);
    }

    QVariant normalizedParameterSettingValue(const QString &diCode, const QVariant &value)
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

    int currentRateCount(const QMap<QString, DataItem> &items)
    {
        if (!items.contains(QStringLiteral("0x04000204")))
        {
            return kDefaultRateCount;
        }

        bool ok = false;
        const int rawRateCount = items.value(QStringLiteral("0x04000204")).value.toInt(&ok);
        return normalizedRateCount(ok ? rawRateCount : kDefaultRateCount);
    }

    int boundedParameterCount(const QMap<QString, DataItem> &items,
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

    int currentYearZoneCount(const QMap<QString, DataItem> &items)
    {
        return boundedParameterCount(items, QStringLiteral("0x04000201"), 2, 1, kMaxYearZoneCount);
    }

    int currentDayScheduleTableCount(const QMap<QString, DataItem> &items)
    {
        return boundedParameterCount(items, QStringLiteral("0x04000202"), 2, 1, kMaxDayScheduleTableCount);
    }

    int currentDayScheduleCount(const QMap<QString, DataItem> &items)
    {
        return boundedParameterCount(items, QStringLiteral("0x04000203"), 8, 1, kMaxDayScheduleCount);
    }

    int currentHolidayCount(const QMap<QString, DataItem> &items)
    {
        return boundedParameterCount(items, QStringLiteral("0x04000205"), 12, 0, kMaxHolidayCount);
    }

    QString normalizedYearZoneEntryValue(const QString &value, int supportedTableCount, int fallbackSeed, int setIndex, int zoneIndex)
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

    QString normalizedDayScheduleEntryValue(const QString &value, int supportedRateCount, int fallbackSeed, int setIndex, int tableIndex, int slotIndex)
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

    QString normalizedHolidayEntryValue(const QString &value, int supportedTableCount, const QDate &fallbackDate)
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

    void refreshParameterScheduleData(QMap<QString, DataItem> &items)
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

    [[maybe_unused]] const QStringList &currentEnergyPrefixes()
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

    const QList<DemandFamilyDefinition> &currentDemandFamilies()
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

    const QSet<int> &demandBaseFamilyValues()
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

    const QList<int> &multiRateEnergyBaseFamilies()
    {
        static const QList<int> families = {
            0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005,
            0x0006, 0x0007, 0x0008, 0x0009, 0x000A};
        return families;
    }

    const QList<int> &historicalTotalOnlyEnergyBaseFamilies()
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

    QString fourDigitHex(int value)
    {
        return QStringLiteral("%1").arg(value, 4, 16, QChar('0')).toUpper();
    }

    QString demandFamilyPrefix(int prefixValue)
    {
        return QStringLiteral("0x") + fourDigitHex(prefixValue);
    }

    QString settlementDayDemandPrefix(int baseFamilyValue, int settlementDay)
    {
        return QStringLiteral("0x") + fourDigitHex(baseFamilyValue + settlementDay * 0x1000);
    }

    int prefixedCodeValue(const QString &diCode, bool *ok = nullptr)
    {
        bool localOk = false;
        const int value = diCode.mid(2, 4).toInt(&localOk, 16);
        if (ok)
        {
            *ok = localOk;
        }
        return value;
    }

    bool isValidSettlementDayIndex(int settlementDay)
    {
        return settlementDay >= 0 && settlementDay <= kMaxSettlementDayCount;
    }

    int settlementDayFromPrefixValue(int prefixValue)
    {
        return prefixValue / 0x1000;
    }

    int energyBaseFamilyFromPrefixValue(int prefixValue)
    {
        return prefixValue % 0x1000;
    }

    bool isMultiRateEnergyBaseFamily(int familyValue)
    {
        return multiRateEnergyBaseFamilies().contains(familyValue);
    }

    bool isHistoricalTotalOnlyEnergyBaseFamily(int familyValue)
    {
        return historicalTotalOnlyEnergyBaseFamilies().contains(familyValue);
    }

    bool isEnergyFamilyPrefixValue(int prefixValue)
    {
        const int settlementDay = settlementDayFromPrefixValue(prefixValue);
        if (!isValidSettlementDayIndex(settlementDay))
        {
            return false;
        }

        const int familyValue = energyBaseFamilyFromPrefixValue(prefixValue);
        return isMultiRateEnergyBaseFamily(familyValue) || isHistoricalTotalOnlyEnergyBaseFamily(familyValue);
    }

    bool isMultiRateEnergyFamilyPrefixValue(int prefixValue)
    {
        const int settlementDay = settlementDayFromPrefixValue(prefixValue);
        return isValidSettlementDayIndex(settlementDay) &&
               isMultiRateEnergyBaseFamily(energyBaseFamilyFromPrefixValue(prefixValue));
    }

    bool isEnergyDataCode(const QString &diCode)
    {
        if (diCode.size() != 10 || !diCode.startsWith(QStringLiteral("0x")))
        {
            return false;
        }

        bool ok = false;
        const int prefixValue = prefixedCodeValue(diCode, &ok);
        return ok && isEnergyFamilyPrefixValue(prefixValue);
    }

    bool isMultiRateEnergyCode(const QString &diCode)
    {
        if (diCode.size() != 10 || !diCode.startsWith(QStringLiteral("0x")))
        {
            return false;
        }

        bool ok = false;
        const int prefixValue = prefixedCodeValue(diCode, &ok);
        return ok && isMultiRateEnergyFamilyPrefixValue(prefixValue);
    }

    bool isHistoricalEnergyCode(const QString &diCode)
    {
        if (!isEnergyDataCode(diCode))
        {
            return false;
        }

        bool ok = false;
        return prefixedCodeValue(diCode, &ok) >= 0x1000 && ok;
    }

    bool isDemandFamilyPrefixValue(int prefixValue)
    {
        const int settlementDay = settlementDayFromPrefixValue(prefixValue);
        if (!isValidSettlementDayIndex(settlementDay))
        {
            return false;
        }

        return demandBaseFamilyValues().contains(prefixValue % 0x1000);
    }

    bool isDemandDataCode(const QString &diCode)
    {
        if (diCode.size() != 10 || !diCode.startsWith(QStringLiteral("0x")))
        {
            return false;
        }

        bool ok = false;
        const int prefixValue = prefixedCodeValue(diCode, &ok);
        return ok && isDemandFamilyPrefixValue(prefixValue);
    }

    QString settlementDayDescriptionPrefix(int settlementDay)
    {
        return settlementDay == 0
                   ? QStringLiteral("(当前)")
                   : QStringLiteral("(上%1次结算日)").arg(settlementDay);
    }

    QString settlementDayDescriptionFromCurrent(const QString &currentDescription, int settlementDay)
    {
        QString description = currentDescription;
        description.replace(QStringLiteral("(当前)"), settlementDayDescriptionPrefix(settlementDay));
        return description;
    }

    QString settlementDayEnergyPrefix(int baseFamilyValue, int settlementDay)
    {
        return QStringLiteral("0x") + fourDigitHex(baseFamilyValue + settlementDay * 0x1000);
    }

    DataItem clonedDataItem(const DataItem &templateItem,
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

    void initializeSettlementDayEnergyData(QMap<QString, DataItem> &items)
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

    void initializeSettlementDayDemandData(QMap<QString, DataItem> &items)
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

    const QList<FreezeEnergyFamilyDefinition> &multiRateFreezeEnergyFamilies()
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

    const QList<FreezeCategoryDefinition> &latestFreezeCategoryDefinitions()
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

    bool usesCurrentDayMidnightFreezeTime(const FreezeCategoryDefinition &category)
    {
        return category.prefix == QStringLiteral("0x0506");
    }

    QDateTime defaultLatestFreezeSnapshotTime(const FreezeCategoryDefinition &category, const QDateTime &referenceTime)
    {
        if (usesCurrentDayMidnightFreezeTime(category))
        {
            return QDateTime(referenceTime.date(), QTime(0, 0));
        }

        return referenceTime.addDays(-category.freezeOffsetDays).addSecs(-category.freezeOffsetHours * 60 * 60);
    }

    const QList<FreezeDemandFamilyDefinition> &freezeDemandFamilies()
    {
        static const QList<FreezeDemandFamilyDefinition> families = {
            {QStringLiteral("09"), QStringLiteral("正向有功"), QStringLiteral("kW"), 9.8765},
            {QStringLiteral("0A"), QStringLiteral("反向有功"), QStringLiteral("kW"), 6.5432}};
        return families;
    }

    QStringList freezeRateSuffixes(int rateCount)
    {
        QStringList suffixes;
        suffixes.reserve(rateCount);
        for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
        {
            suffixes.append(upperHexByte(rateIndex + 1));
        }
        return suffixes;
    }

    bool isFreezeEnergyFamilyCode(const QString &diCode)
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

    bool isFreezeEnergyBlockCode(const QString &diCode)
    {
        return isFreezeEnergyFamilyCode(diCode) && diCode.endsWith(QStringLiteral("01"));
    }

    [[maybe_unused]] bool isFreezeDemandBlockCode(const QString &diCode)
    {
        if (diCode.size() != 10 || !diCode.startsWith(QStringLiteral("0x05")) || !diCode.endsWith(QStringLiteral("01")))
        {
            return false;
        }

        const QString familyHex = diCode.mid(6, 2);
        return familyHex == QStringLiteral("09") || familyHex == QStringLiteral("0A");
    }

    [[maybe_unused]] bool isFreezeVariableBlockCode(const QString &diCode)
    {
        return diCode.size() == 10 &&
               diCode.startsWith(QStringLiteral("0x05")) &&
               diCode.mid(6, 2) == QStringLiteral("10") &&
               diCode.endsWith(QStringLiteral("01"));
    }

    QStringList currentFreezeEnergyBlockCodes(const QMap<QString, DataItem> &items, const QString &diPrefix)
    {
        const int rateCount = currentRateCount(items);
        QStringList codes;
        codes.reserve(rateCount + 1);
        for (int rateIndex = rateCount; rateIndex >= 1; --rateIndex)
        {
            codes.append(diPrefix + upperHexByte(rateIndex + 1));
        }
        codes.append(diPrefix + QStringLiteral("00"));
        return codes;
    }

    void initializeLatestFreezeEnergyCategory(QMap<QString, DataItem> &items,
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

    void initializeLatestHourlyFreezeCategory(QMap<QString, DataItem> &items, const QDateTime &freezeTime)
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

    QDateTime freezeSnapshotTime(const QMap<QString, DataItem> &items, const FreezeCategoryDefinition &category)
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

    QVariantList buildFreezeDemandBlockValue(const FreezeDemandFamilyDefinition &family,
                                             const QDateTime &freezeTime,
                                             double valueBias,
                                             int rateCount)
    {
        const QList<double> weights = {0.94, 0.88, 0.81, 0.76, 0.70};
        const double totalValue = roundToDecimals(family.baseValue + valueBias, 4);

        QVariantList values;
        values.reserve((rateCount + 1) * 2);
        for (int rateIndex = rateCount; rateIndex >= 1; --rateIndex)
        {
            const int weightIndex = qMin(rateIndex - 1, weights.size() - 1);
            const QDateTime eventTime = freezeTime.addSecs(-(rateIndex * 37 + 11) * 60);
            values.append(compactDateTimeToMinutes(eventTime));
            values.append(roundToDecimals(totalValue * weights.at(weightIndex), 4));
        }

        values.append(compactDateTimeToMinutes(freezeTime.addSecs(-7 * 60)));
        values.append(totalValue);
        return values;
    }

    QVariantList buildFreezeVariableBlockValue(double valueBias)
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

    void refreshLatestFreezeCompositeBlocks(QMap<QString, DataItem> &items)
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
                                                repeatedFormat(QStringLiteral("AA,AA,AA,AA,AA:AA,.,AA,AA"), rateCount + 1),
                                                false,
                                                0);
                }

                items[blockCode].format = repeatedFormat(QStringLiteral("AA,AA,AA,AA,AA:AA,.,AA,AA"), rateCount + 1);
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

    QStringList energyRateSuffixes(int rateCount)
    {
        QStringList suffixes;
        suffixes.reserve(rateCount);
        for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
        {
            suffixes.append(upperHexByte(rateIndex) + QStringLiteral("00"));
        }
        return suffixes;
    }

    QStringList prefixedCodes(const QString &prefix, const QStringList &suffixes)
    {
        QStringList codes;
        codes.reserve(suffixes.size());
        for (const QString &suffix : suffixes)
        {
            codes.append(prefix + suffix);
        }
        return codes;
    }

    QString repeatedFormat(const QString &formatPart, int repeatCount)
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

    QString rateDescription(const QString &templateDescription, int rateIndex)
    {
        QString description = templateDescription;
        description.replace(QStringLiteral("费率1"), QStringLiteral("费率%1").arg(rateIndex));
        return description;
    }

    bool parseHexByteValue(const QString &hex, int *value)
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

    bool isHarmonicFamilyCode(const QString &diCode,
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

    QStringList harmonicPayloadCodes(const QString &blockCode)
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

    void initializePhaseHarmonicFamily(QMap<QString, DataItem> &items,
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

    bool isCurrentEnergyRateCode(const QString &diCode, int *rateIndex = nullptr)
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

    bool isDemandRateCode(const QString &diCode, int *rateIndex = nullptr)
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

    bool isFreezeRateCode(const QString &diCode, int *rateIndex = nullptr)
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

    bool isRateControlledCodeSupported(const QMap<QString, DataItem> &items, const QString &diCode)
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

    bool isParameterStructureCodeSupported(const QMap<QString, DataItem> &items, const QString &diCode)
    {
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

    void appendValuesForCodes(const QMap<QString, DataItem> &items, const QStringList &codes, QVariantList &values)
    {
        for (const QString &code : codes)
        {
            if (items.contains(code))
            {
                values.append(items.value(code).value);
            }
        }
    }

    QStringList currentEnergyBlockCodes(const QMap<QString, DataItem> &items, const QString &diCode)
    {
        const QString prefix = diCode.left(6);
        const int rateCount = currentRateCount(items);
        QStringList codes;
        codes.reserve(rateCount + 1);
        for (int rateIndex = rateCount; rateIndex >= 1; --rateIndex)
        {
            codes.append(prefix + upperHexByte(rateIndex) + QStringLiteral("00"));
        }
        codes.append(prefix + QStringLiteral("0000"));
        return codes;
    }

    bool applyEnergyBlockValues(QMap<QString, DataItem> &items, const QString &blockCode, const QVariantList &values)
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

    QStringList currentDemandBlockCodes(const QMap<QString, DataItem> &items, const QString &diPrefix)
    {
        const int rateCount = currentRateCount(items);
        QStringList codes;
        codes.reserve((rateCount + 1) * 2);
        for (int rateIndex = rateCount; rateIndex >= 1; --rateIndex)
        {
            const QString rateHex = upperHexByte(rateIndex);
            codes.append(diPrefix + rateHex + QStringLiteral("01"));
            codes.append(diPrefix + rateHex + QStringLiteral("00"));
        }
        codes.append(diPrefix + QStringLiteral("0001"));
        codes.append(diPrefix + QStringLiteral("0000"));
        return codes;
    }

    double defaultDemandRateValue(const DemandFamilyDefinition &family, int familyIndex, int rateIndex)
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

    void initializeCurrentDemandFamily(QMap<QString, DataItem> &items,
                                       const DemandFamilyDefinition &family,
                                       const QDateTime &now,
                                       int familyIndex)
    {
        const QString prefix = demandFamilyPrefix(family.prefixValue);
        items[prefix + QStringLiteral("0000")] = DataItem(prefix + QStringLiteral("0000"),
                                                          QStringLiteral("(当前)%1总最大需量").arg(family.description),
                                                          0.0,
                                                          family.unit,
                                                          QStringLiteral("AA,AA,.,AA,AA"),
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
                                                          repeatedFormat(QStringLiteral("AA,AA,AA,AA,AA:AA,.,AA,AA"), kDefaultRateCount + 1),
                                                          false,
                                                          0);
    }

    void ensureCurrentEnergyRateItems(QMap<QString, DataItem> &items)
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

    void ensureDemandRateItems(QMap<QString, DataItem> &items)
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

    void ensureFreezeRateItems(QMap<QString, DataItem> &items)
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

    void refreshRateDependentBlockFormats(QMap<QString, DataItem> &items)
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

        const QString demandSegmentFormat = QStringLiteral("AA,AA,AA,AA,AA:AA,.,AA,AA");
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

    bool isSinglePhaseUnsupportedCode(const QString &diCode)
    {
        static const QSet<QString> unsupportedCodes = {
            QStringLiteral("0x0201FF00"),
            QStringLiteral("0x0202FF00"),
            QStringLiteral("0x0203FF00"),
            QStringLiteral("0x0204FF00"),
            QStringLiteral("0x0205FF00"),
            QStringLiteral("0x0206FF00"),
            QStringLiteral("0x0207FF00"),
            QStringLiteral("0x0208FF00"),
            QStringLiteral("0x0209FF00"),
            QStringLiteral("0x02010200"),
            QStringLiteral("0x02010300"),
            QStringLiteral("0x02020200"),
            QStringLiteral("0x02020300"),
            QStringLiteral("0x02030200"),
            QStringLiteral("0x02030300"),
            QStringLiteral("0x02040200"),
            QStringLiteral("0x02040300"),
            QStringLiteral("0x02050200"),
            QStringLiteral("0x02050300"),
            QStringLiteral("0x02060200"),
            QStringLiteral("0x02060300"),
            QStringLiteral("0x02070200"),
            QStringLiteral("0x02070300"),
            QStringLiteral("0x02080200"),
            QStringLiteral("0x02080300"),
            QStringLiteral("0x02090200"),
            QStringLiteral("0x02090300"),
            QStringLiteral("0x23010002"),
            QStringLiteral("0x23010003"),
            QStringLiteral("0x23010006"),
            QStringLiteral("0x23010007"),
            QStringLiteral("0x230100FF")};
        if (unsupportedCodes.contains(diCode))
        {
            return true;
        }

        int phaseIndex = 0;
        if ((isHarmonicFamilyCode(diCode, QStringLiteral("0x020A"), &phaseIndex) ||
             isHarmonicFamilyCode(diCode, QStringLiteral("0x020B"), &phaseIndex)) &&
            phaseIndex >= 2)
        {
            return true;
        }

        if (isDemandDataCode(diCode))
        {
            bool ok = false;
            const int prefixValue = prefixedCodeValue(diCode, &ok);
            if (!ok)
            {
                return false;
            }

            const int familyValue = prefixValue % 0x1000;
            return (familyValue >= 0x0129 && familyValue <= 0x0132) ||
                   (familyValue >= 0x01A8 && familyValue <= 0x01AE) ||
                   (familyValue >= 0x013D && familyValue <= 0x0146) ||
                   (familyValue >= 0x01BC && familyValue <= 0x01C2);
        }

        if (!isEnergyDataCode(diCode))
        {
            return false;
        }

        bool ok = false;
        const int prefixValue = prefixedCodeValue(diCode, &ok);
        if (!ok)
        {
            return false;
        }

        const int familyValue = energyBaseFamilyFromPrefixValue(prefixValue);
        return (familyValue >= 0x0029 && familyValue <= 0x0032) ||
               (familyValue >= 0x003D && familyValue <= 0x0046) ||
               (familyValue >= 0x00A8 && familyValue <= 0x00AE) ||
               (familyValue >= 0x00BC && familyValue <= 0x00C2);
    }

    void synchronizeGroupTotal(QMap<QString, DataItem> &items,
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

    void synchronizeCurrentEnergyTotals(QMap<QString, DataItem> &items)
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

    void synchronizeFreezeTotals(QMap<QString, DataItem> &items)
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

    void synchronizeInstantaneousPowerTotals(QMap<QString, DataItem> &items)
    {
        synchronizeGroupTotal(items, "0x02030000", {"0x02030100", "0x02030200", "0x02030300"});
        synchronizeGroupTotal(items, "0x02040000", {"0x02040100", "0x02040200", "0x02040300"});
        synchronizeGroupTotal(items, "0x02050000", {"0x02050100", "0x02050200", "0x02050300"});
    }

    void synchronizeDemandTotals(QMap<QString, DataItem> &items)
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

    QString compactDateTimeToMinutes(const QDateTime &dateTime)
    {
        return dateTime.toString(QStringLiteral("yyMMddHHmm"));
    }

    QString compactDateTimeToSeconds(const QDateTime &dateTime)
    {
        return dateTime.toString(QStringLiteral("yyMMddHHmmss"));
    }

    QString compactDateWithWeek(const QDate &date)
    {
        const int weekCode = date.dayOfWeek() % 7;
        return date.toString(QStringLiteral("yyMMdd")) + QStringLiteral("%1").arg(weekCode, 2, 10, QChar('0'));
    }

    [[maybe_unused]] QString eventHistoryCode(const QString &baseCode, int historyIndex)
    {
        return baseCode + upperHexByte(historyIndex);
    }

    void randomizeDataItems(QMap<QString, DataItem> &items,
                            const QString &meterAddress,
                            MeterPhaseType meterPhaseType,
                            const QSet<QString> *targetCodes = nullptr)
    {
        QRandomGenerator *rng = QRandomGenerator::global();

        for (auto it = items.begin(); it != items.end(); ++it)
        {
            DataItem &item = it.value();
            if (targetCodes && !targetCodes->contains(item.diCode))
            {
                continue;
            }

            if (meterPhaseType == MeterPhaseType::SinglePhase && isSinglePhaseUnsupportedCode(item.diCode))
            {
                continue;
            }

            if (!isRateControlledCodeSupported(items, item.diCode))
            {
                continue;
            }

            if (isDataBlockItem(item))
            {
                continue;
            }

            if (item.diCode == "0x04000401")
            {
                item.value = meterAddress;
                continue;
            }

            if (item.diCode == "0x04000101")
            {
                item.value = compactDateWithWeek(QDate::currentDate().addDays(-rng->bounded(0, 7)));
                continue;
            }

            if (item.diCode == "0x04000102")
            {
                item.value = QTime::currentTime().addSecs(rng->bounded(0, 24 * 60 * 60)).toString(QStringLiteral("HHmmss"));
                continue;
            }

            if (item.diCode == "0x05060001")
            {
                item.value = compactDateTimeToMinutes(QDateTime(QDate::currentDate(), QTime(0, 0)));
                continue;
            }

            if (item.diCode == "0x03010000" ||
                item.diCode == "0x03010001" ||
                item.diCode == "0x03010002" ||
                item.diCode == "0x03020000" ||
                item.diCode == "0x03020001" ||
                item.diCode == "0x03020002" ||
                item.diCode == "0x03030000" ||
                item.diCode == "0x03030001" ||
                item.diCode == "0x03030002" ||
                item.diCode == "0x03040000" ||
                item.diCode == "0x03040001" ||
                item.diCode == "0x03040002")
            {
                item.value = QVariantList{
                    rng->bounded(1, 30),
                    rng->bounded(30, 24 * 60)};
                continue;
            }

            if (item.diCode.startsWith("0x030101") ||
                item.diCode.startsWith("0x030102") ||
                item.diCode.startsWith("0x030103") ||
                item.diCode.startsWith("0x030201") ||
                item.diCode.startsWith("0x030202") ||
                item.diCode.startsWith("0x030203") ||
                item.diCode.startsWith("0x030301") ||
                item.diCode.startsWith("0x030302") ||
                item.diCode.startsWith("0x030303") ||
                item.diCode.startsWith("0x030401") ||
                item.diCode.startsWith("0x030402") ||
                item.diCode.startsWith("0x030403"))
            {
                const int phaseIndex = item.diCode.mid(6, 2).toInt(nullptr, 16) - 1;
                const QDateTime endTime = QDateTime::currentDateTime().addSecs(-rng->bounded(2 * 60 * 60, 90 * 24 * 60 * 60));
                const QDateTime startTime = endTime.addSecs(-rng->bounded(10 * 60, 8 * 60 * 60));
                QVariantList values{
                    compactDateTimeToSeconds(startTime),
                    compactDateTimeToSeconds(endTime),
                    roundToDecimals(rng->bounded(1000, 7000) / 100.0, 2),
                    roundToDecimals(rng->bounded(100, 2500) / 100.0, 2),
                    roundToDecimals(rng->bounded(200, 3500) / 100.0, 2),
                    roundToDecimals(rng->bounded(150, 3200) / 100.0, 2)};

                for (int dataPhase = 0; dataPhase < 3; ++dataPhase)
                {
                    const double baseVoltage = (dataPhase == phaseIndex) ? rng->bounded(1200, 1900) / 10.0
                                                                         : rng->bounded(2100, 2350) / 10.0;
                    values.append(roundToDecimals(rng->bounded(200, 2000) / 100.0, 2));
                    values.append(roundToDecimals(rng->bounded(50, 800) / 100.0, 2));
                    values.append(roundToDecimals(rng->bounded(80, 1200) / 100.0, 2));
                    values.append(roundToDecimals(rng->bounded(80, 1200) / 100.0, 2));
                    values.append(roundToDecimals(baseVoltage, 1));
                    values.append(roundToDecimals(rng->bounded(10, 800) / 100.0, 3));
                    values.append(roundToDecimals(rng->bounded(10, 500) / 100.0, 4));
                    values.append(roundToDecimals(rng->bounded(5, 300) / 100.0, 4));
                    values.append(roundToDecimals(rng->bounded(850, 999) / 1000.0, 3));
                }

                item.value = values;
                continue;
            }

            if (item.diCode == "0x03050000" ||
                item.diCode == "0x03060000" ||
                item.diCode == "0x03070000" ||
                item.diCode == "0x03080000" ||
                item.diCode == "0x03090000")
            {
                item.value = QVariantList{
                    rng->bounded(1, 12),
                    rng->bounded(20, 12 * 60)};
                continue;
            }

            if (item.diCode.startsWith("0x030500") && item.diCode != "0x03050000")
            {
                const QDateTime endTime = QDateTime::currentDateTime().addSecs(-rng->bounded(2 * 60 * 60, 120 * 24 * 60 * 60));
                const QDateTime startTime = endTime.addSecs(-rng->bounded(10 * 60, 12 * 60 * 60));
                item.value = QVariantList{
                    compactDateTimeToSeconds(startTime),
                    roundToDecimals(rng->bounded(5, 180) / 1000.0, 3),
                    compactDateTimeToSeconds(endTime)};
                continue;
            }

            if (item.diCode.startsWith("0x030600") && item.diCode != "0x03060000")
            {
                const QDateTime endTime = QDateTime::currentDateTime().addSecs(-rng->bounded(2 * 60 * 60, 120 * 24 * 60 * 60));
                const QDateTime startTime = endTime.addSecs(-rng->bounded(5 * 60, 24 * 60 * 60));
                item.value = QVariantList{
                    compactDateTimeToSeconds(startTime),
                    compactDateTimeToSeconds(endTime)};
                continue;
            }

            if ((item.diCode.startsWith("0x030700") && item.diCode != "0x03070000") ||
                (item.diCode.startsWith("0x030800") && item.diCode != "0x03080000"))
            {
                QVariantList values;
                const QDateTime endTime = QDateTime::currentDateTime().addSecs(-rng->bounded(2 * 60 * 60, 120 * 24 * 60 * 60));
                const QDateTime startTime = endTime.addSecs(-rng->bounded(10 * 60, 18 * 60 * 60));
                values.append(compactDateTimeToSeconds(startTime));
                values.append(compactDateTimeToSeconds(endTime));
                for (int index = 0; index < 16; ++index)
                {
                    values.append(roundToDecimals(rng->bounded(100, 12000) / 100.0, 2));
                }
                item.value = values;
                continue;
            }

            if (item.diCode.startsWith("0x030900") && item.diCode != "0x03090000")
            {
                QVariantList values;
                const QDateTime endTime = QDateTime::currentDateTime().addSecs(-rng->bounded(2 * 60 * 60, 120 * 24 * 60 * 60));
                const QDateTime startTime = endTime.addSecs(-rng->bounded(10 * 60, 18 * 60 * 60));
                values.append(compactDateTimeToSeconds(startTime));
                values.append(compactDateTimeToSeconds(endTime));
                values.append(roundToDecimals(rng->bounded(50, 1200) / 100.0, 2));
                for (int index = 0; index < 16; ++index)
                {
                    values.append(roundToDecimals(rng->bounded(100, 12000) / 100.0, 2));
                }
                item.value = values;
                continue;
            }

            if (item.diCode.startsWith("0x0310"))
            {
                const bool totalItem = item.diCode.startsWith("0x031000");
                const int phaseOffset = totalItem ? 0 : qMax(0, item.diCode.mid(6, 2).toInt(nullptr, 16) - 1);
                const int monthOffset = item.diCode.mid(8, 2).toInt(nullptr, 16);
                item.value = QVariantList{
                    43000 - monthOffset * 2200 - phaseOffset * 500,
                    roundToDecimals((totalItem ? 97.5 : 98.1) - monthOffset * 0.2 - phaseOffset * 0.1, 2),
                    roundToDecimals((totalItem ? 1.2 : 0.9) + monthOffset * 0.05 + phaseOffset * 0.03, 2),
                    100 + monthOffset * 7 + phaseOffset * 5,
                    80 + monthOffset * 6 + phaseOffset * 4,
                    roundToDecimals(241.0 - monthOffset * 0.6 - phaseOffset * 0.4, 1),
                    QDateTime(QDate::currentDate().addMonths(-monthOffset), QTime(9 + (monthOffset % 8), 15)).toString(QStringLiteral("MMddHHmm")),
                    roundToDecimals(201.0 + monthOffset * 0.4 + phaseOffset * 0.3, 1),
                    QDateTime(QDate::currentDate().addMonths(-monthOffset), QTime(18 - (monthOffset % 6), 25)).toString(QStringLiteral("MMddHHmm"))};
                continue;
            }

            if (item.diCode == "0x03110000")
            {
                item.value = rng->bounded(1, 20);
                continue;
            }

            if (item.diCode.startsWith("0x031100") && item.diCode != "0x03110000")
            {
                const QDateTime endTime = QDateTime::currentDateTime().addSecs(-rng->bounded(2 * 60 * 60, 120 * 24 * 60 * 60));
                const QDateTime startTime = endTime.addSecs(-rng->bounded(5 * 60, 72 * 60 * 60));
                item.value = QVariantList{
                    compactDateTimeToSeconds(startTime),
                    compactDateTimeToSeconds(endTime)};
                continue;
            }

            if (item.diCode == "0x03120000")
            {
                QVariantList totals;
                for (int index = 0; index < 6; ++index)
                {
                    totals.append(rng->bounded(0, 12 - qMin(index, 5)));
                }
                item.value = totals;
                continue;
            }

            if (item.diCode.startsWith("0x0312") && item.diCode != "0x03120000")
            {
                bool demandTypeOk = false;
                const int demandType = item.diCode.mid(6, 2).toInt(&demandTypeOk, 16);
                if (demandTypeOk && demandType >= 1 && demandType <= 6)
                {
                    const QDateTime endTime = QDateTime::currentDateTime().addSecs(-rng->bounded(2 * 60 * 60, 120 * 24 * 60 * 60));
                    const QDateTime startTime = endTime.addSecs(-rng->bounded(10 * 60, 12 * 60 * 60));
                    const int demandDurationSecs = qMax(6 * 60, static_cast<int>(startTime.secsTo(endTime)));
                    const QDateTime maxDemandTime = startTime.addSecs(rng->bounded(5 * 60, demandDurationSecs));
                    item.value = QVariantList{
                        compactDateTimeToSeconds(startTime),
                        compactDateTimeToSeconds(endTime),
                        roundToDecimals(2.5 + demandType * 0.4 + rng->bounded(0, 1200) / 10000.0, 4),
                        compactDateTimeToMinutes(maxDemandTime)};
                    continue;
                }
            }

            if (item.diCode == "0x03300000" ||
                item.diCode == "0x03300100" ||
                item.diCode == "0x03300200" ||
                item.diCode == "0x03300300" ||
                item.diCode == "0x03300400" ||
                item.diCode == "0x03300500" ||
                item.diCode == "0x03300600" ||
                item.diCode == "0x03300D00" ||
                item.diCode == "0x03300E00")
            {
                item.value = rng->bounded(1, 12);
                continue;
            }

            if (item.diCode.startsWith("0x033000") && item.diCode != "0x03300000")
            {
                QVariantList values;
                values.append(compactDateTimeToSeconds(QDateTime::currentDateTime().addSecs(-rng->bounded(60 * 60, 180 * 24 * 60 * 60))));
                values.append(QStringLiteral("%1").arg(rng->bounded(0, 100000000), 8, 10, QChar('0')));

                const QStringList programmedDiCodes = {
                    QStringLiteral("03010000"), QStringLiteral("03010001"), QStringLiteral("03010002"),
                    QStringLiteral("03110000"), QStringLiteral("03120000"), QStringLiteral("03300000"),
                    QStringLiteral("03300100"), QStringLiteral("03300200"), QStringLiteral("03300300"),
                    QStringLiteral("03300400"), QStringLiteral("03300500"), QStringLiteral("03300600")};
                for (const QString &diCode : programmedDiCodes)
                {
                    values.append(diCode);
                }

                item.value = values;
                continue;
            }

            if (item.diCode.startsWith("0x033001") && item.diCode != "0x03300100")
            {
                QVariantList values;
                values.append(compactDateTimeToSeconds(QDateTime::currentDateTime().addSecs(-rng->bounded(60 * 60, 180 * 24 * 60 * 60))));
                values.append(QStringLiteral("%1").arg(rng->bounded(0, 100000000), 8, 10, QChar('0')));
                for (int energyIndex = 0; energyIndex < 24; ++energyIndex)
                {
                    values.append(roundToDecimals(rng->bounded(10000, 9999999) / 100.0, 2));
                }
                item.value = values;
                continue;
            }

            if (item.diCode.startsWith("0x033002") && item.diCode != "0x03300200")
            {
                QVariantList values;
                values.append(compactDateTimeToSeconds(QDateTime::currentDateTime().addSecs(-rng->bounded(60 * 60, 180 * 24 * 60 * 60))));
                values.append(QStringLiteral("%1").arg(rng->bounded(0, 100000000), 8, 10, QChar('0')));
                for (int demandIndex = 0; demandIndex < 24; ++demandIndex)
                {
                    values.append(roundToDecimals(rng->bounded(1000, 250000) / 10000.0, 4));
                    values.append(compactDateTimeToMinutes(QDateTime::currentDateTime().addSecs(-rng->bounded(30 * 60, 180 * 24 * 60 * 60))));
                }
                item.value = values;
                continue;
            }

            if (item.diCode.startsWith("0x033003") && item.diCode != "0x03300300")
            {
                const QStringList targetDiCodes = {
                    QStringLiteral("03010000"), QStringLiteral("03110000"), QStringLiteral("03120000"),
                    QStringLiteral("03300000"), QStringLiteral("03300100"), QStringLiteral("03300200"),
                    QStringLiteral("03300300"), QStringLiteral("03300400"), QStringLiteral("03300500"),
                    QStringLiteral("03300600")};
                item.value = QVariantList{
                    compactDateTimeToSeconds(QDateTime::currentDateTime().addSecs(-rng->bounded(60 * 60, 180 * 24 * 60 * 60))),
                    QStringLiteral("%1").arg(rng->bounded(0, 100000000), 8, 10, QChar('0')),
                    targetDiCodes.at(rng->bounded(targetDiCodes.size()))};
                continue;
            }

            if (item.diCode.startsWith("0x033004") && item.diCode != "0x03300400")
            {
                const QDateTime afterTime = QDateTime::currentDateTime().addSecs(-rng->bounded(60 * 60, 60 * 24 * 60 * 60));
                const QDateTime beforeTime = afterTime.addSecs(-rng->bounded(60, 6 * 60 * 60));
                item.value = QVariantList{
                    QStringLiteral("%1").arg(rng->bounded(0, 100000000), 8, 10, QChar('0')),
                    compactDateTimeToSeconds(beforeTime),
                    compactDateTimeToSeconds(afterTime)};
                continue;
            }

            if (item.diCode.startsWith("0x033005") && item.diCode != "0x03300500")
            {
                QVariantList values;
                values.append(compactDateTimeToSeconds(QDateTime::currentDateTime().addSecs(-rng->bounded(60 * 60, 180 * 24 * 60 * 60))));
                values.append(QStringLiteral("%1").arg(rng->bounded(0, 100000000), 8, 10, QChar('0')));
                for (int setIndex = 0; setIndex < 2; ++setIndex)
                {
                    for (int tableIndex = 0; tableIndex < kMaxDayScheduleTableCount; ++tableIndex)
                    {
                        values.append(buildDayScheduleTableValue(rng->bounded(1, 200), setIndex, tableIndex));
                    }
                }
                item.value = values;
                continue;
            }

            if (item.diCode.startsWith("0x033006") && item.diCode != "0x03300600")
            {
                QVariantList values;
                values.append(compactDateTimeToSeconds(QDateTime::currentDateTime().addSecs(-rng->bounded(60 * 60, 180 * 24 * 60 * 60))));
                values.append(QStringLiteral("%1").arg(rng->bounded(0, 100000000), 8, 10, QChar('0')));
                for (int setIndex = 0; setIndex < 2; ++setIndex)
                {
                    for (int zoneIndex = 0; zoneIndex < kMaxYearZoneCount; ++zoneIndex)
                    {
                        values.append(buildYearZoneEntryValue(rng->bounded(1, 200), setIndex, zoneIndex));
                    }
                }
                item.value = values;
                continue;
            }

            if ((item.diCode.startsWith("0x03300D") && item.diCode != "0x03300D00") ||
                (item.diCode.startsWith("0x03300E") && item.diCode != "0x03300E00"))
            {
                const QDateTime endTime = QDateTime::currentDateTime().addSecs(-rng->bounded(60 * 60, 180 * 24 * 60 * 60));
                const QDateTime startTime = endTime.addSecs(-rng->bounded(10 * 60, 12 * 60 * 60));
                QVariantList values;
                values.append(compactDateTimeToSeconds(startTime));
                values.append(compactDateTimeToSeconds(endTime));
                const double baseValue = item.diCode.startsWith("0x03300D") ? 15.0 : 25.0;
                for (int energyIndex = 0; energyIndex < 12; ++energyIndex)
                {
                    values.append(roundToDecimals(baseValue + rng->bounded(0, 5000) / 100.0 + energyIndex * 2.5, 2));
                }
                item.value = values;
                continue;
            }

            if (item.format == "AA,AA,AA,AA,AA" && (item.description.contains(QStringLiteral("鏃堕棿")) || item.description.contains(QStringLiteral("鍙戠敓"))))
            {
                const QDateTime randomTime = QDateTime::currentDateTime().addSecs(-rng->bounded(0, 30 * 24 * 60 * 60));
                item.value = compactDateTimeToMinutes(randomTime);
                continue;
            }

            if (item.diCode.startsWith("0x01") && item.diCode.endsWith("01") && item.format == "AA,AA,AA,AA,AA")
            {
                const QDateTime randomTime = QDateTime::currentDateTime().addSecs(-rng->bounded(1, 24 * 60 * 60 + 1));
                item.value = compactDateTimeToMinutes(randomTime);
                continue;
            }

            if (item.diCode.startsWith("0x06") && item.format == "AA,AA")
            {
                const int duration = rng->bounded(1, 60);
                const int unitCode = (rng->bounded(0, 2) == 0) ? 2 : 3;
                item.value = QStringLiteral("%1%2")
                                 .arg(duration, 2, 10, QChar('0'))
                                 .arg(unitCode, 2, 10, QChar('0'));
                continue;
            }

            const QVariant originalValue = item.value;
            const NumericFormatSpec formatSpec = parseNumericFormatSpec(item.format);
            bool variableValueHandled = false;

            if (item.diCode.startsWith("0x0202") || item.diCode == "0x02800001")
            {
                item.value = rng->bounded(0, 10000) / 1000.0;
                variableValueHandled = true;
            }
            else if (item.diCode.startsWith("0x0203"))
            {
                item.value = rng->bounded(0, 300001) / 10000.0;
                variableValueHandled = true;
            }
            else if (item.diCode.startsWith("0x0204"))
            {
                item.value = rng->bounded(-300000, 300001) / 10000.0;
                variableValueHandled = true;
            }
            else if (item.diCode.startsWith("0x0205"))
            {
                item.value = rng->bounded(0, 300001) / 10000.0;
                variableValueHandled = true;
            }
            else if (item.diCode.startsWith("0x0206"))
            {
                item.value = formatSpec.allowNegative
                                 ? rng->bounded(-1000, 1001) / 1000.0
                                 : rng->bounded(0, 1001) / 1000.0;
                variableValueHandled = true;
            }
            else if (item.diCode.startsWith("0x0207"))
            {
                item.value = rng->bounded(0, 3600) / 10.0;
                variableValueHandled = true;
            }
            else if (item.diCode == "0x02800002")
            {
                item.value = 50.0 + (rng->bounded(-50, 51) / 10.0);
                variableValueHandled = true;
            }
            else if (item.diCode == "0x02800003")
            {
                item.value = rng->bounded(0, 300001) / 10000.0;
                variableValueHandled = true;
            }
            else if (item.diCode == "0x02800004")
            {
                item.value = rng->bounded(0, 300001) / 10000.0;
                variableValueHandled = true;
            }
            else if (item.diCode == "0x02800005")
            {
                item.value = rng->bounded(0, 300001) / 10000.0;
                variableValueHandled = true;
            }
            else if (item.diCode == "0x02800006")
            {
                item.value = rng->bounded(0, 300001) / 10000.0;
                variableValueHandled = true;
            }
            else if (item.diCode == "0x02800007")
            {
                item.value = 20.0 + (rng->bounded(0, 260) / 10.0);
                variableValueHandled = true;
            }
            else if (item.diCode == "0x02800008" || item.diCode == "0x02800009")
            {
                item.value = 3.0 + (rng->bounded(0, 151) / 100.0);
                variableValueHandled = true;
            }
            else if (item.diCode == "0x0280000A")
            {
                item.value = rng->bounded(1000, 200000);
                variableValueHandled = true;
            }
            else if (item.diCode == "0x0280000B")
            {
                item.value = rng->bounded(3000, 15001) / 10000.0;
                variableValueHandled = true;
            }
            else if (item.diCode.startsWith("0x230100") && !item.diCode.endsWith("FF"))
            {
                item.value = rng->bounded(2500, 8501) / 100.0;
                variableValueHandled = true;
            }

            if (!variableValueHandled && item.diCode.startsWith("0x0201"))
            {
                if (item.diCode.contains("0100") || item.diCode.contains("0200") || item.diCode.contains("0300"))
                {
                    item.value = 220.0 + (rng->bounded(-2200, 2200) / 100.0);
                }
                else if (item.diCode.contains("0101") || item.diCode.contains("0201") || item.diCode.contains("0202"))
                {
                    item.value = rng->bounded(0, 10000) / 1000.0;
                }
                else if (item.diCode.contains("0300"))
                {
                    item.value = rng->bounded(0, 100000) / 10000.0;
                }
                else if (item.diCode.contains("0600"))
                {
                    item.value = formatSpec.allowNegative
                                     ? rng->bounded(-1000, 1000) / 1000.0
                                     : rng->bounded(0, 1000) / 1000.0;
                }
                else if (item.diCode.contains("0701"))
                {
                    item.value = rng->bounded(0, 3600) / 10.0;
                }
                else if (item.diCode.contains("00002"))
                {
                    item.value = 50.0 + (rng->bounded(-50, 50) / 10.0);
                }
                else
                {
                    item.value = rng->bounded(0, 1000) / 10.0;
                }
            }
            else if (!variableValueHandled && isEnergyDataCode(item.diCode))
            {
                item.value = item.value.toDouble() + rng->bounded(0, 100) / 100.0;
            }
            else if (!variableValueHandled)
            {
                item.value = rng->bounded(0, 10000) / 100.0;
            }

            if (formatSpec.valid)
            {
                item.value = normalizeValueForFormat(item.value, formatSpec);
                item.value = normalizedParameterSettingValue(item.diCode, item.value);
            }
            else
            {
                item.value = originalValue;
            }
        }

        synchronizeDemandTotals(items);
        synchronizeInstantaneousPowerTotals(items);
        synchronizeCurrentEnergyTotals(items);
        synchronizeFreezeTotals(items);
        refreshLatestFreezeCompositeBlocks(items);
    }
}
MeterModel::MeterModel(QObject *parent)
    : QObject(parent), m_meterAddress("202101190552"), m_operatorCode("00000000"), m_password("00000000"), m_includePrefixFE(true), m_meterPhaseType(MeterPhaseType::ThreePhase)
{
    initializeDefaultDataItems();
}

MeterModel::~MeterModel()
{
}

void MeterModel::setMeterAddress(const QString &address)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_meterAddress != address)
        {
            m_meterAddress = address;
            if (m_dataItems.contains("0x04000401"))
            {
                m_dataItems["0x04000401"].value = address;
            }
            changed = true;
        }
    }

    if (changed)
    {
        emit addressChanged(address);
    }
}

QString MeterModel::getMeterAddress() const
{
    QMutexLocker locker(&m_mutex);
    return m_meterAddress;
}

void MeterModel::setOperatorCode(const QString &code)
{
    QMutexLocker locker(&m_mutex);
    m_operatorCode = code;
}

QString MeterModel::getOperatorCode() const
{
    QMutexLocker locker(&m_mutex);
    return m_operatorCode;
}

void MeterModel::setPassword(const QString &password)
{
    QMutexLocker locker(&m_mutex);
    m_password = password;
}

QString MeterModel::getPassword() const
{
    QMutexLocker locker(&m_mutex);
    return m_password;
}

void MeterModel::setIncludePrefixFE(bool include)
{
    QMutexLocker locker(&m_mutex);
    m_includePrefixFE = include;
}

bool MeterModel::getIncludePrefixFE() const
{
    QMutexLocker locker(&m_mutex);
    return m_includePrefixFE;
}

void MeterModel::setMeterPhaseType(MeterPhaseType meterPhaseType)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_meterPhaseType != meterPhaseType)
        {
            m_meterPhaseType = meterPhaseType;
            changed = true;
        }
    }

    if (changed)
    {
        emit dataBatchChanged();
    }
}

MeterPhaseType MeterModel::getMeterPhaseType() const
{
    QMutexLocker locker(&m_mutex);
    return m_meterPhaseType;
}

bool MeterModel::isSinglePhase() const
{
    return getMeterPhaseType() == MeterPhaseType::SinglePhase;
}

bool MeterModel::isDataItemSupported(const QString &diCode) const
{
    QMutexLocker locker(&m_mutex);
    return isDataItemSupportedUnlocked(diCode);
}

bool MeterModel::isDataItemSupportedUnlocked(const QString &diCode) const
{
    if (!m_dataItems.contains(diCode))
    {
        return false;
    }

    if (!isParameterStructureCodeSupported(m_dataItems, diCode))
    {
        return false;
    }

    if (m_meterPhaseType != MeterPhaseType::SinglePhase)
    {
        return isRateControlledCodeSupported(m_dataItems, diCode);
    }

    return !isSinglePhaseUnsupportedCode(diCode) && isRateControlledCodeSupported(m_dataItems, diCode);
}

DataItem MeterModel::getDataItem(const QString &diCode) const
{
    QMutexLocker locker(&m_mutex);
    return isDataItemSupportedUnlocked(diCode) ? m_dataItems.value(diCode, DataItem()) : DataItem();
}

QVariant MeterModel::getDataValue(const QString &diCode) const
{
    QMutexLocker locker(&m_mutex);
    if (isDataItemSupportedUnlocked(diCode))
    {
        // 检查是否为数据块
        if (m_dataItems[diCode].unit == "-")
        {
            // 数据块，返回包含相关数据项的列表
            QVariantList values;

            // 根据数据块类型，添加相关数据项
            if (diCode == "0x0201FF00")
            {
                // 电压数据块，包含A相、B相、C相电压
                if (m_dataItems.contains("0x02010300"))
                    values.append(m_dataItems["0x02010300"].value);
                if (m_dataItems.contains("0x02010200"))
                    values.append(m_dataItems["0x02010200"].value);
                if (m_dataItems.contains("0x02010100"))
                    values.append(m_dataItems["0x02010100"].value);
            }
            else if (diCode == "0x0202FF00")
            {
                // 电流数据块，包含A相、B相、C相电流
                if (m_dataItems.contains("0x02020300"))
                    values.append(m_dataItems["0x02020300"].value);
                if (m_dataItems.contains("0x02020200"))
                    values.append(m_dataItems["0x02020200"].value);
                if (m_dataItems.contains("0x02020100"))
                    values.append(m_dataItems["0x02020100"].value);
            }
            else if (diCode == "0x0203FF00")
            {
                // 功率数据块，包含A相、B相、C相功率
                if (m_dataItems.contains("0x02030300"))
                    values.append(m_dataItems["0x02030300"].value);
                if (m_dataItems.contains("0x02030200"))
                    values.append(m_dataItems["0x02030200"].value);
                if (m_dataItems.contains("0x02030100"))
                    values.append(m_dataItems["0x02030100"].value);
                if (m_dataItems.contains("0x02030000"))
                    values.append(m_dataItems["0x02030000"].value);
            }
            else if (diCode == "0x0204FF00")
            {
                if (m_dataItems.contains("0x02040300"))
                    values.append(m_dataItems["0x02040300"].value);
                if (m_dataItems.contains("0x02040200"))
                    values.append(m_dataItems["0x02040200"].value);
                if (m_dataItems.contains("0x02040100"))
                    values.append(m_dataItems["0x02040100"].value);
                if (m_dataItems.contains("0x02040000"))
                    values.append(m_dataItems["0x02040000"].value);
            }
            else if (diCode == "0x0205FF00")
            {
                if (m_dataItems.contains("0x02050300"))
                    values.append(m_dataItems["0x02050300"].value);
                if (m_dataItems.contains("0x02050200"))
                    values.append(m_dataItems["0x02050200"].value);
                if (m_dataItems.contains("0x02050100"))
                    values.append(m_dataItems["0x02050100"].value);
                if (m_dataItems.contains("0x02050000"))
                    values.append(m_dataItems["0x02050000"].value);
            }
            else if (diCode == "0x0206FF00")
            {
                // 功率因数数据块，包含总功率因数
                if (m_dataItems.contains("0x02060300"))
                    values.append(m_dataItems["0x02060300"].value);
                if (m_dataItems.contains("0x02060200"))
                    values.append(m_dataItems["0x02060200"].value);
                if (m_dataItems.contains("0x02060100"))
                    values.append(m_dataItems["0x02060100"].value);
                if (m_dataItems.contains("0x02060000"))
                    values.append(m_dataItems["0x02060000"].value);
            }
            else if (diCode == "0x0207FF00")
            {
                // 相角数据块，包含A相、B相、C相相角
                if (m_dataItems.contains("0x02070300"))
                    values.append(m_dataItems["0x02070300"].value);
                if (m_dataItems.contains("0x02070200"))
                    values.append(m_dataItems["0x02070200"].value);
                if (m_dataItems.contains("0x02070100"))
                    values.append(m_dataItems["0x02070100"].value);
            }
            else if (diCode == "0x0208FF00")
            {
                if (m_dataItems.contains("0x02080300"))
                    values.append(m_dataItems["0x02080300"].value);
                if (m_dataItems.contains("0x02080200"))
                    values.append(m_dataItems["0x02080200"].value);
                if (m_dataItems.contains("0x02080100"))
                    values.append(m_dataItems["0x02080100"].value);
            }
            else if (diCode == "0x0209FF00")
            {
                if (m_dataItems.contains("0x02090300"))
                    values.append(m_dataItems["0x02090300"].value);
                if (m_dataItems.contains("0x02090200"))
                    values.append(m_dataItems["0x02090200"].value);
                if (m_dataItems.contains("0x02090100"))
                    values.append(m_dataItems["0x02090100"].value);
            }
            else if (isHarmonicFamilyCode(diCode, QStringLiteral("0x020A"), nullptr, nullptr, nullptr) ||
                     isHarmonicFamilyCode(diCode, QStringLiteral("0x020B"), nullptr, nullptr, nullptr))
            {
                appendValuesForCodes(m_dataItems, harmonicPayloadCodes(diCode), values);
            }
            else if (diCode == "0x230100FF")
            {
                appendValuesForCodes(m_dataItems,
                                     {QStringLiteral("0x23010008"),
                                      QStringLiteral("0x23010007"),
                                      QStringLiteral("0x23010006"),
                                      QStringLiteral("0x23010005"),
                                      QStringLiteral("0x23010004"),
                                      QStringLiteral("0x23010003"),
                                      QStringLiteral("0x23010002"),
                                      QStringLiteral("0x23010001")},
                                     values);
            }
            else if (diCode.endsWith("FF00") && isMultiRateEnergyCode(diCode))
            {
                appendValuesForCodes(m_dataItems, currentEnergyBlockCodes(m_dataItems, diCode), values);
            }
            else if (diCode.endsWith(QStringLiteral("FF00")) && isDemandDataCode(diCode))
            {
                appendValuesForCodes(m_dataItems, currentDemandBlockCodes(m_dataItems, diCode.left(6)), values);
            }
            else if (isFreezeEnergyBlockCode(diCode))
            {
                const QString basePrefix = diCode.left(8);
                appendValuesForCodes(m_dataItems, currentFreezeEnergyBlockCodes(m_dataItems, basePrefix), values);
            }

            if (!values.isEmpty())
            {
                return values;
            }
            return m_dataItems[diCode].value;
        }
        else
        {
            // 普通数据项，直接返回值
            return m_dataItems[diCode].value;
        }
    }
    return QVariant();
}

bool MeterModel::setDataValue(const QString &diCode, const QVariant &value)
{
    bool updated = false;
    bool requiresBatchRefresh = false;
    QVariant emittedValue = value;
    {
        QMutexLocker locker(&m_mutex);
        if (isDataItemSupportedUnlocked(diCode))
        {
            emittedValue = normalizedParameterSettingValue(diCode, value);

            m_dataItems[diCode].value = emittedValue;
            if (applyEnergyBlockValues(m_dataItems, diCode, emittedValue.toList()))
            {
                requiresBatchRefresh = true;
            }
            if (isMultiRateEnergyCode(diCode))
            {
                synchronizeCurrentEnergyTotals(m_dataItems);
            }
            if (isDemandDataCode(diCode))
            {
                synchronizeDemandTotals(m_dataItems);
            }
            if (diCode.startsWith("0x0203") || diCode.startsWith("0x0204") || diCode.startsWith("0x0205"))
            {
                synchronizeInstantaneousPowerTotals(m_dataItems);
            }
            if (isFreezeEnergyFamilyCode(diCode))
            {
                synchronizeFreezeTotals(m_dataItems);
            }
            if (diCode.startsWith(QStringLiteral("0x05")))
            {
                refreshLatestFreezeCompositeBlocks(m_dataItems);
            }
            if (diCode.startsWith(QStringLiteral("0x040100")) ||
                diCode.startsWith(QStringLiteral("0x040200")) ||
                diCode.startsWith(QStringLiteral("0x0403")))
            {
                refreshParameterScheduleData(m_dataItems);
                requiresBatchRefresh = true;
            }
            if (diCode == "0x04000201" ||
                diCode == "0x04000202" ||
                diCode == "0x04000203" ||
                diCode == "0x04000204" ||
                diCode == "0x04000205")
            {
                refreshParameterScheduleData(m_dataItems);
                refreshRateDependentBlockFormats(m_dataItems);
                synchronizeDemandTotals(m_dataItems);
                synchronizeInstantaneousPowerTotals(m_dataItems);
                synchronizeCurrentEnergyTotals(m_dataItems);
                synchronizeFreezeTotals(m_dataItems);
                refreshLatestFreezeCompositeBlocks(m_dataItems);
                requiresBatchRefresh = true;
            }
            if (diCode == "0x04000401")
            {
                m_meterAddress = emittedValue.toString();
            }
            updated = true;
        }
    }

    if (updated)
    {
        emit dataChanged(diCode, emittedValue);
        if (diCode == "0x04000401")
        {
            emit addressChanged(emittedValue.toString());
        }
        if (requiresBatchRefresh)
        {
            emit dataBatchChanged();
        }
    }

    return updated;
}

void MeterModel::applyPersistedData(const QMap<QString, QVariant> &dataSnapshot)
{
    if (dataSnapshot.isEmpty())
    {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        for (auto it = dataSnapshot.constBegin(); it != dataSnapshot.constEnd(); ++it)
        {
            if (m_dataItems.contains(it.key()))
            {
                m_dataItems[it.key()].value = normalizedParameterSettingValue(it.key(), it.value());
            }
        }

        if (m_dataItems.contains("0x04000401"))
        {
            m_dataItems["0x04000401"].value = m_meterAddress;
        }

        refreshParameterScheduleData(m_dataItems);
        refreshRateDependentBlockFormats(m_dataItems);
        synchronizeDemandTotals(m_dataItems);
        synchronizeInstantaneousPowerTotals(m_dataItems);
        synchronizeCurrentEnergyTotals(m_dataItems);
        synchronizeFreezeTotals(m_dataItems);
        refreshLatestFreezeCompositeBlocks(m_dataItems);
    }

    emit dataBatchChanged();
}

QList<DataItem> MeterModel::getDataItemsByCategory(DataCategory category) const
{
    QMutexLocker locker(&m_mutex);
    QList<DataItem> items;

    QString prefix;
    switch (category)
    {
    case DataCategory::Energy:
        prefix.clear();
        break;
    case DataCategory::Demand:
        prefix.clear();
        break;
    case DataCategory::Variable:
        prefix = "0x02";
        break;
    case DataCategory::Event:
        prefix = "0x03";
        break;
    case DataCategory::Parameter:
        prefix = "0x04";
        break;
    case DataCategory::Freeze:
        prefix = "0x05";
        break;
    case DataCategory::LoadRecord:
        prefix = "0x06";
        break;
    }

    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        const bool categoryMatched = (category == DataCategory::Energy)
                                         ? isEnergyDataCode(it.key())
                                         : (category == DataCategory::Demand)
                                               ? isDemandDataCode(it.key())
                                               : it.key().startsWith(prefix);
        if (categoryMatched && isDataItemSupportedUnlocked(it.key()))
        {
            items.append(it.value());
        }
    }

    return items;
}

QList<DataItem> MeterModel::getAllDataItems() const
{
    QMutexLocker locker(&m_mutex);
    QList<DataItem> items;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (isDataItemSupportedUnlocked(it.key()))
        {
            items.append(it.value());
        }
    }
    return items;
}

void MeterModel::resetAllData()
{
    {
        QMutexLocker locker(&m_mutex);
        m_dataItems.clear();
        initializeDefaultDataItems();
    }
    emit dataBatchChanged();
}

void MeterModel::randomizeData()
{
    QMutexLocker locker(&m_mutex);
    randomizeDataItems(m_dataItems, m_meterAddress, m_meterPhaseType);
    locker.unlock();
    emit dataBatchChanged();
}

void MeterModel::randomizeData(const QList<QString> &diCodes)
{
    QMutexLocker locker(&m_mutex);
    const QSet<QString> targetCodes(diCodes.begin(), diCodes.end());
    if (targetCodes.isEmpty())
    {
        return;
    }
    randomizeDataItems(m_dataItems, m_meterAddress, m_meterPhaseType, &targetCodes);
    locker.unlock();
    emit dataBatchChanged();
}

void MeterModel::initializeDefaultDataItems()
{
    initializeEnergyData();
    initializeDemandData();
    initializeVariableData();
    initializeEventData();
    initializeParameterData();
    refreshParameterScheduleData(m_dataItems);
    initializeFreezeData();
    initializeLoadRecordData();
    ensureCurrentEnergyRateItems(m_dataItems);
    ensureDemandRateItems(m_dataItems);
    ensureFreezeRateItems(m_dataItems);
    refreshParameterScheduleData(m_dataItems);
    refreshRateDependentBlockFormats(m_dataItems);
    synchronizeDemandTotals(m_dataItems);
    synchronizeInstantaneousPowerTotals(m_dataItems);
    synchronizeCurrentEnergyTotals(m_dataItems);
    synchronizeFreezeTotals(m_dataItems);
    refreshLatestFreezeCompositeBlocks(m_dataItems);
}

void MeterModel::initializeEnergyData()
{
    // 电能量数据 (0x00xxxxxx)

    // 1. 组合有功电能 [0000]
    m_dataItems["0x00000000"] = DataItem("0x00000000", "(当前)组合有功总电能", 1.2, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00000100"] = DataItem("0x00000100", "(当前)组合有功费率1电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00000200"] = DataItem("0x00000200", "(当前)组合有功费率2电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00000300"] = DataItem("0x00000300", "(当前)组合有功费率3电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00000400"] = DataItem("0x00000400", "(当前)组合有功费率4电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00000500"] = DataItem("0x00000500", "(当前)组合有功费率5电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0000FF00"] = DataItem("0x0000FF00", "(当前)组合有功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);
    // 2. 正向有功电能 [0001]
    m_dataItems["0x00010000"] = DataItem("0x00010000", "(当前)正向有功总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010100"] = DataItem("0x00010100", "(当前)正向有功费率1电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010200"] = DataItem("0x00010200", "(当前)正向有功费率2电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010300"] = DataItem("0x00010300", "(当前)正向有功费率3电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010400"] = DataItem("0x00010400", "(当前)正向有功费率4电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010500"] = DataItem("0x00010500", "(当前)正向有功费率5电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0001FF00"] = DataItem("0x0001FF00", "(当前)正向有功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 3. 反向有功电能 [0002]
    m_dataItems["0x00020000"] = DataItem("0x00020000", "(当前)反向有功总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020100"] = DataItem("0x00020100", "(当前)反向有功费率1电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020200"] = DataItem("0x00020200", "(当前)反向有功费率2电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020300"] = DataItem("0x00020300", "(当前)反向有功费率3电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020400"] = DataItem("0x00020400", "(当前)反向有功费率4电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020500"] = DataItem("0x00020500", "(当前)反向有功费率5电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0002FF00"] = DataItem("0x0002FF00", "(当前)反向有功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 4. 组合无功 1 电能 [0003]
    m_dataItems["0x00030000"] = DataItem("0x00030000", "(当前)组合无功1总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030100"] = DataItem("0x00030100", "(当前)组合无功1费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030200"] = DataItem("0x00030200", "(当前)组合无功1费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030300"] = DataItem("0x00030300", "(当前)组合无功1费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030400"] = DataItem("0x00030400", "(当前)组合无功1费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030500"] = DataItem("0x00030500", "(当前)组合无功1费率5电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0003FF00"] = DataItem("0x0003FF00", "(当前)组合无功1电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 5. 组合无功 2 电能 [0004]
    m_dataItems["0x00040000"] = DataItem("0x00040000", "(当前)组合无功2总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040100"] = DataItem("0x00040100", "(当前)组合无功2费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040200"] = DataItem("0x00040200", "(当前)组合无功2费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040300"] = DataItem("0x00040300", "(当前)组合无功2费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040400"] = DataItem("0x00040400", "(当前)组合无功2费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040500"] = DataItem("0x00040500", "(当前)组合无功2费率5电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0004FF00"] = DataItem("0x0004FF00", "(当前)组合无功2电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 6. 四象限无功电能 [0005-0008]
    // 第一象限无功电能 [0005]
    m_dataItems["0x00050000"] = DataItem("0x00050000", "(当前)第一象限无功总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050100"] = DataItem("0x00050100", "(当前)第一象限无功费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050200"] = DataItem("0x00050200", "(当前)第一象限无功费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050300"] = DataItem("0x00050300", "(当前)第一象限无功费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050400"] = DataItem("0x00050400", "(当前)第一象限无功费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050500"] = DataItem("0x00050500", "(当前)第一象限无功费率5电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0005FF00"] = DataItem("0x0005FF00", "(当前)第一象限无功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 第二象限无功电能 [0006]
    m_dataItems["0x00060000"] = DataItem("0x00060000", "(当前)第二象限无功总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060100"] = DataItem("0x00060100", "(当前)第二象限无功费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060200"] = DataItem("0x00060200", "(当前)第二象限无功费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060300"] = DataItem("0x00060300", "(当前)第二象限无功费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060400"] = DataItem("0x00060400", "(当前)第二象限无功费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060500"] = DataItem("0x00060500", "(当前)第二象限无功费率5电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0006FF00"] = DataItem("0x0006FF00", "(当前)第二象限无功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 第三象限无功电能 [0007]
    m_dataItems["0x00070000"] = DataItem("0x00070000", "(当前)第三象限无功总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070100"] = DataItem("0x00070100", "(当前)第三象限无功费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070200"] = DataItem("0x00070200", "(当前)第三象限无功费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070300"] = DataItem("0x00070300", "(当前)第三象限无功费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070400"] = DataItem("0x00070400", "(当前)第三象限无功费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070500"] = DataItem("0x00070500", "(当前)第三象限无功费率5电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0007FF00"] = DataItem("0x0007FF00", "(当前)第三象限无功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 第四象限无功电能 [0008]
    m_dataItems["0x00080000"] = DataItem("0x00080000", "(当前)第四象限无功总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080100"] = DataItem("0x00080100", "(当前)第四象限无功费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080200"] = DataItem("0x00080200", "(当前)第四象限无功费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080300"] = DataItem("0x00080300", "(当前)第四象限无功费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080400"] = DataItem("0x00080400", "(当前)第四象限无功费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080500"] = DataItem("0x00080500", "(当前)第四象限无功费率5电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0008FF00"] = DataItem("0x0008FF00", "(当前)第四象限无功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 7. 视在电能 [0009-000A]
    // 正向视在电能 [0009]
    m_dataItems["0x00090000"] = DataItem("0x00090000", "(当前)正向视在总电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00090100"] = DataItem("0x00090100", "(当前)正向视在费率1电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00090200"] = DataItem("0x00090200", "(当前)正向视在费率2电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00090300"] = DataItem("0x00090300", "(当前)正向视在费率3电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00090400"] = DataItem("0x00090400", "(当前)正向视在费率4电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00090500"] = DataItem("0x00090500", "(当前)正向视在费率5电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0009FF00"] = DataItem("0x0009FF00", "(当前)正向视在电能数据块", 0.0, "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 反向视在电能 [000A]
    m_dataItems["0x000A0000"] = DataItem("0x000A0000", "(当前)反向视在总电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000A0100"] = DataItem("0x000A0100", "(当前)反向视在费率1电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000A0200"] = DataItem("0x000A0200", "(当前)反向视在费率2电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000A0300"] = DataItem("0x000A0300", "(当前)反向视在费率3电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000A0400"] = DataItem("0x000A0400", "(当前)反向视在费率4电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000A0500"] = DataItem("0x000A0500", "(当前)反向视在费率5电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000AFF00"] = DataItem("0x000AFF00", "(当前)反向视在电能数据块", 0.0, "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 8. 其他电能数据 [0080-0086]
    m_dataItems["0x00800000"] = DataItem("0x00800000", "(当前)关联总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00810000"] = DataItem("0x00810000", "(当前)正向有功基波总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00820000"] = DataItem("0x00820000", "(当前)反向有功基波总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00830000"] = DataItem("0x00830000", "(当前)正向有功谐波总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00840000"] = DataItem("0x00840000", "(当前)反向有功谐波总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00850000"] = DataItem("0x00850000", "(当前)铜损有功总电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x00860000"] = DataItem("0x00860000", "(当前)铁损有功总电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);

    // 9. A 相电能数据 [0015-001E, 0094-009A]
    m_dataItems["0x00150000"] = DataItem("0x00150000", "(当前)A相正向有功电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00160000"] = DataItem("0x00160000", "(当前)A相反向有功电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00170000"] = DataItem("0x00170000", "(当前)A相组合无功1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00180000"] = DataItem("0x00180000", "(当前)A相组合无功2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00190000"] = DataItem("0x00190000", "(当前)A相第一象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x001A0000"] = DataItem("0x001A0000", "(当前)A相第二象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x001B0000"] = DataItem("0x001B0000", "(当前)A相第三象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x001C0000"] = DataItem("0x001C0000", "(当前)A相第四象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x001D0000"] = DataItem("0x001D0000", "(当前)A相正向视在电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x001E0000"] = DataItem("0x001E0000", "(当前)A相反向视在电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00940000"] = DataItem("0x00940000", "(当前)A相关联电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00950000"] = DataItem("0x00950000", "(当前)A相正向有功基波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00960000"] = DataItem("0x00960000", "(当前)A相反向有功基波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00970000"] = DataItem("0x00970000", "(当前)A相正向有功谐波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00980000"] = DataItem("0x00980000", "(当前)A相反向有功谐波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00990000"] = DataItem("0x00990000", "(当前)A相铜损有功电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x009A0000"] = DataItem("0x009A0000", "(当前)A相铁损有功电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);

    // 10. B 相电能数据 [0029-0032, 00A8-00AE]
    m_dataItems["0x00290000"] = DataItem("0x00290000", "(当前)B相正向有功电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x002A0000"] = DataItem("0x002A0000", "(当前)B相反向有功电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x002B0000"] = DataItem("0x002B0000", "(当前)B相组合无功1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x002C0000"] = DataItem("0x002C0000", "(当前)B相组合无功2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x002D0000"] = DataItem("0x002D0000", "(当前)B相第一象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x002E0000"] = DataItem("0x002E0000", "(当前)B相第二象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x002F0000"] = DataItem("0x002F0000", "(当前)B相第三象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00300000"] = DataItem("0x00300000", "(当前)B相第四象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00310000"] = DataItem("0x00310000", "(当前)B相正向视在电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00320000"] = DataItem("0x00320000", "(当前)B相反向视在电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00A80000"] = DataItem("0x00A80000", "(当前)B相关联电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00A90000"] = DataItem("0x00A90000", "(当前)B相正向有功基波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00AA0000"] = DataItem("0x00AA0000", "(当前)B相反向有功基波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00AB0000"] = DataItem("0x00AB0000", "(当前)B相正向有功谐波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00AC0000"] = DataItem("0x00AC0000", "(当前)B相反向有功谐波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00AD0000"] = DataItem("0x00AD0000", "(当前)B相铜损有功电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x00AE0000"] = DataItem("0x00AE0000", "(当前)B相铁损有功电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);

    // 11. C 相电能数据 [003D-0046, 00BC-00C2]
    m_dataItems["0x003D0000"] = DataItem("0x003D0000", "(当前)C相正向有功电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x003E0000"] = DataItem("0x003E0000", "(当前)C相反向有功电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x003F0000"] = DataItem("0x003F0000", "(当前)C相组合无功1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00400000"] = DataItem("0x00400000", "(当前)C相组合无功2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00410000"] = DataItem("0x00410000", "(当前)C相第一象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00420000"] = DataItem("0x00420000", "(当前)C相第二象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00430000"] = DataItem("0x00430000", "(当前)C相第三象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00440000"] = DataItem("0x00440000", "(当前)C相第四象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00450000"] = DataItem("0x00450000", "(当前)C相正向视在电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00460000"] = DataItem("0x00460000", "(当前)C相反向视在电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00BC0000"] = DataItem("0x00BC0000", "(当前)C相关联电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00BD0000"] = DataItem("0x00BD0000", "(当前)C相正向有功基波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00BE0000"] = DataItem("0x00BE0000", "(当前)C相反向有功基波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00BF0000"] = DataItem("0x00BF0000", "(当前)C相正向有功谐波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00C00000"] = DataItem("0x00C00000", "(当前)C相反向有功谐波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00C10000"] = DataItem("0x00C10000", "(当前)C相铜损有功电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x00C20000"] = DataItem("0x00C20000", "(当前)C相铁损有功电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);

    initializeSettlementDayEnergyData(m_dataItems);
    synchronizeCurrentEnergyTotals(m_dataItems);
}

void MeterModel::initializeDemandData()
{
    // 最大需量及发生时间 (0x01xxxxxx)
    const QDateTime now = QDateTime::currentDateTime();

    int familyIndex = 0;
    for (const DemandFamilyDefinition &family : currentDemandFamilies())
    {
        initializeCurrentDemandFamily(m_dataItems, family, now, familyIndex);
        ++familyIndex;
    }
    initializeSettlementDayDemandData(m_dataItems);
    synchronizeDemandTotals(m_dataItems);
}

void MeterModel::initializeVariableData()
{
    // 变量数据 (0x02xxxxxx)
    // 电压
    m_dataItems["0x02010100"] = DataItem("0x02010100", "A相电压", 20.1, "V", "AA,xx", false, 1);
    m_dataItems["0x02010200"] = DataItem("0x02010200", "B相电压", 220.2, "V", "AA,xx", false, 1);
    m_dataItems["0x02010300"] = DataItem("0x02010300", "C相电压", 220.3, "V", "AA,xx", false, 1);
    m_dataItems["0x0201FF00"] = DataItem("0x0201FF00", "电压数据块", QVariantList(), "-", "AA,xx:AA,xx:AA,xx", false, 1);

    // 电流
    m_dataItems["0x02020100"] = DataItem("0x02020100", "A相电流", 1.01, "A", "AA,xx,AA", false, 3);
    m_dataItems["0x02020200"] = DataItem("0x02020200", "B相电流", 1.023, "A", "AA,xx,AA", false, 3);
    m_dataItems["0x02020300"] = DataItem("0x02020300", "C相电流", 1.1, "A", "AA,xx,AA", false, 3);
    m_dataItems["0x0202FF00"] = DataItem("0x0202FF00", "电流数据块", QVariantList(), "-", "AA,xx,AA:AA,xx,AA:AA,xx,AA", false, 3);

    // 功率
    m_dataItems["0x02030000"] = DataItem("0x02030000", "瞬时总有功功率", 0.0, "kW", "F9,.,AA,AA", false, 4);
    m_dataItems["0x02030100"] = DataItem("0x02030100", "瞬时A相有功功率", 0.0, "kW", "F9,.,AA,AA", false, 4);
    m_dataItems["0x02030200"] = DataItem("0x02030200", "瞬时B相有功功率", 0.0, "kW", "F9,.,AA,AA", false, 4);
    m_dataItems["0x02030300"] = DataItem("0x02030300", "瞬时C相有功功率", 0.0, "kW", "F9,.,AA,AA", false, 4);
    m_dataItems["0x0203FF00"] = DataItem("0x0203FF00", "瞬时有功功率数据块", QVariantList(), "-", "F9,.,AA,AA:F9,.,AA,AA:F9,.,AA,AA:F9,.,AA,AA", false, 4);

    m_dataItems["0x02040000"] = DataItem("0x02040000", "瞬时总无功功率", 0.0, "kvar", "F9,.,AA,AA", false, 4);
    m_dataItems["0x02040100"] = DataItem("0x02040100", "瞬时A相无功功率", 0.0, "kvar", "F9,.,AA,AA", false, 4);
    m_dataItems["0x02040200"] = DataItem("0x02040200", "瞬时B相无功功率", 0.0, "kvar", "F9,.,AA,AA", false, 4);
    m_dataItems["0x02040300"] = DataItem("0x02040300", "瞬时C相无功功率", 0.0, "kvar", "F9,.,AA,AA", false, 4);
    m_dataItems["0x0204FF00"] = DataItem("0x0204FF00", "瞬时无功功率数据块", QVariantList(), "-", "F9,.,AA,AA:F9,.,AA,AA:F9,.,AA,AA:F9,.,AA,AA", false, 4);

    m_dataItems["0x02050000"] = DataItem("0x02050000", "瞬时总视在功率", 0.0, "kVA", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02050100"] = DataItem("0x02050100", "瞬时A相视在功率", 0.0, "kVA", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02050200"] = DataItem("0x02050200", "瞬时B相视在功率", 0.0, "kVA", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02050300"] = DataItem("0x02050300", "瞬时C相视在功率", 0.0, "kVA", "AA,.,AA,AA", false, 4);
    m_dataItems["0x0205FF00"] = DataItem("0x0205FF00", "瞬时视在功率数据块", QVariantList(), "-", "AA,.,AA,AA:AA,.,AA,AA:AA,.,AA,AA:AA,.,AA,AA", false, 4);

    // 功率因数
    m_dataItems["0x02060000"] = DataItem("0x02060000", "总功率因数", 1.0, "none", "xx,AA", false, 3);
    m_dataItems["0x02060100"] = DataItem("0x02060100", "A相功率因数", 1.0, "none", "xx,AA", false, 3);
    m_dataItems["0x02060200"] = DataItem("0x02060200", "B相功率因数", 1.0, "none", "xx,AA", false, 3);
    m_dataItems["0x02060300"] = DataItem("0x02060300", "C相功率因数", 1.0, "none", "xx,AA", false, 3);
    m_dataItems["0x0206FF00"] = DataItem("0x0206FF00", "功率因数数据块", QVariantList(), "-", "xx,AA:xx,AA:xx,AA:xx,AA", false, 3);

    // 相角
    m_dataItems["0x02070100"] = DataItem("0x02070100", "A相相角", 0.0, "°", "AA,xx", false, 1);
    m_dataItems["0x02070200"] = DataItem("0x02070200", "B相相角", 0.0, "°", "AA,xx", false, 1);
    m_dataItems["0x02070300"] = DataItem("0x02070300", "C相相角", 0.0, "°", "AA,xx", false, 1);
    m_dataItems["0x0207FF00"] = DataItem("0x0207FF00", "相角数据块", QVariantList(), "-", "AA,xx:AA,xx:AA,xx", false, 1);

    // 波形失真度
    m_dataItems["0x02080100"] = DataItem("0x02080100", "A相电压波形失真度", 1.25, "%", "AA,.,AA", false, 2);
    m_dataItems["0x02080200"] = DataItem("0x02080200", "B相电压波形失真度", 1.18, "%", "AA,.,AA", false, 2);
    m_dataItems["0x02080300"] = DataItem("0x02080300", "C相电压波形失真度", 1.31, "%", "AA,.,AA", false, 2);
    m_dataItems["0x0208FF00"] = DataItem("0x0208FF00", "电压波形失真度数据块", QVariantList(), "-", "AA,.,AA:AA,.,AA:AA,.,AA", false, 2);
    m_dataItems["0x02090100"] = DataItem("0x02090100", "A相电流波形失真度", 2.46, "%", "AA,.,AA", false, 2);
    m_dataItems["0x02090200"] = DataItem("0x02090200", "B相电流波形失真度", 2.38, "%", "AA,.,AA", false, 2);
    m_dataItems["0x02090300"] = DataItem("0x02090300", "C相电流波形失真度", 2.52, "%", "AA,.,AA", false, 2);
    m_dataItems["0x0209FF00"] = DataItem("0x0209FF00", "电流波形失真度数据块", QVariantList(), "-", "AA,.,AA:AA,.,AA:AA,.,AA", false, 2);

    // 谐波含量
    for (int phaseIndex = 1; phaseIndex <= 3; ++phaseIndex)
    {
        initializePhaseHarmonicFamily(m_dataItems, QStringLiteral("0x020A"), QStringLiteral("电压"), phaseIndex, 1.05, 0.18);
        initializePhaseHarmonicFamily(m_dataItems, QStringLiteral("0x020B"), QStringLiteral("电流"), phaseIndex, 1.28, 0.22);
    }

    // 零相电流
    m_dataItems["0x02800001"] = DataItem("0x02800001", "零相电流", 0.0, "A", "AA,xx,AA", false, 3);
    // 频率
    m_dataItems["0x02800002"] = DataItem("0x02800002", "电网频率", 50.0, "Hz", "AA,.,AA", false, 2);
    m_dataItems["0x02800003"] = DataItem("0x02800003", "一分钟有功总平均功率", 12.3456, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02800004"] = DataItem("0x02800004", "当前有功需量", 10.8765, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02800005"] = DataItem("0x02800005", "当前无功需量", 8.4321, "kvar", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02800006"] = DataItem("0x02800006", "当前视在需量", 13.2109, "kVA", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02800007"] = DataItem("0x02800007", "表内温度", 26.5, "℃", "AA,xx", false, 1);
    m_dataItems["0x02800008"] = DataItem("0x02800008", "时钟电池电压(内部)", 3.27, "V", "AA,.,AA", false, 2);
    m_dataItems["0x02800009"] = DataItem("0x02800009", "停电抄表电池电压(外部)", 3.18, "V", "AA,.,AA", false, 2);
    m_dataItems["0x0280000A"] = DataItem("0x0280000A", "内部电池工作时间", 8640, "分钟", "AA,AA,AA,AA", false, 0);
    m_dataItems["0x0280000B"] = DataItem("0x0280000B", "当前阶梯电价", 0.5688, "元/kWh", "AA,AA,.,AA,AA", false, 4);
    m_dataItems["0x23010001"] = DataItem("0x23010001", "A相上端子温度", 32.50, "℃", "AA,AA,.,AA", false, 2);
    m_dataItems["0x23010002"] = DataItem("0x23010002", "B相上端子温度", 33.10, "℃", "AA,AA,.,AA", false, 2);
    m_dataItems["0x23010003"] = DataItem("0x23010003", "C相上端子温度", 31.80, "℃", "AA,AA,.,AA", false, 2);
    m_dataItems["0x23010004"] = DataItem("0x23010004", "N相上端子温度", 30.60, "℃", "AA,AA,.,AA", false, 2);
    m_dataItems["0x23010005"] = DataItem("0x23010005", "A相下端子温度", 34.20, "℃", "AA,AA,.,AA", false, 2);
    m_dataItems["0x23010006"] = DataItem("0x23010006", "B相下端子温度", 34.90, "℃", "AA,AA,.,AA", false, 2);
    m_dataItems["0x23010007"] = DataItem("0x23010007", "C相下端子温度", 33.70, "℃", "AA,AA,.,AA", false, 2);
    m_dataItems["0x23010008"] = DataItem("0x23010008", "N相下端子温度", 31.40, "℃", "AA,AA,.,AA", false, 2);
    m_dataItems["0x230100FF"] = DataItem("0x230100FF", "所有端子温度数据块", QVariantList(), "-", "AA,AA,.,AA:AA,AA,.,AA:AA,AA,.,AA:AA,AA,.,AA:AA,AA,.,AA:AA,AA,.,AA:AA,AA,.,AA:AA,AA,.,AA", false, 2);
    synchronizeInstantaneousPowerTotals(m_dataItems);
}

void MeterModel::initializeEventData()
{
    // 事件记录 (0x03xxxxxx)
    const QDateTime now = QDateTime::currentDateTime();
    const QStringList phaseNames = {QStringLiteral("A相"),
                                    QStringLiteral("B相"),
                                    QStringLiteral("C相")};
    const QStringList demandLimitNames = {
        QStringLiteral("正向有功"),
        QStringLiteral("反向有功"),
        QStringLiteral("第一象限无功"),
        QStringLiteral("第二象限无功"),
        QStringLiteral("第三象限无功"),
        QStringLiteral("第四象限无功")};
    const QString countFormat = QStringLiteral("AA,AA,AA");
    const QString timeToSecondsFormat = QStringLiteral("AA,AA,AA,AA,AA,AA");
    const QString timeToMinutesFormat = QStringLiteral("AA,AA,AA,AA,AA");
    const QString operatorCodeFormat = QStringLiteral("AA,AA,AA,AA");
    const QString diCodeFormat = QStringLiteral("AA,AA,AA,AA");
    const QString energyFormat = QStringLiteral("AA,AA,AA,.,AA");
    const QString demandFormat = QStringLiteral("AA,.,AAAA");
    const QString powerFormat = QStringLiteral("AA,.,AAAA");
    const QString currentFormat = QStringLiteral("AA,.,AAA");
    const QString voltageFormat = QStringLiteral("AA,AA,.,A");
    const QString powerFactorFormat = QStringLiteral("AA,.,A");
    const QString percentFormat = QStringLiteral("AA,.,AA");
    const QString largePercentFormat = QStringLiteral("AA,AA,.,AA");
    const QString monthTimeFormat = QStringLiteral("AA,AA,AA,AA");
    const QString scheduleTableFormat = repeatedByteFormat(kMaxDayScheduleCount * 3);
    const QString yearZoneEntryFormat = QStringLiteral("AA,AA,AA");
    const int programmedDiCount = 12;

    auto phaseEventCode = [](const QString &familyPrefix, int historyIndex) {
        return familyPrefix + upperHexByte(historyIndex);
    };

    auto defaultOperatorCode = [this](int historyIndex) {
        if (!m_operatorCode.isEmpty())
        {
            return m_operatorCode;
        }
        return QStringLiteral("%1").arg((historyIndex * 1234567) % 100000000, 8, 10, QChar('0'));
    };

    auto appendEnergySnapshot = [](QVariantList &values, double baseValue) {
        for (int i = 0; i < 24; ++i)
        {
            values.append(roundToDecimals(baseValue + i * 7.35, 2));
        }
    };

    auto appendDemandSnapshot = [&](QVariantList &values, int historyIndex, double baseValue) {
        for (int i = 0; i < 24; ++i)
        {
            values.append(roundToDecimals(baseValue + i * 0.12, 4));
            values.append(compactDateTimeToMinutes(now.addSecs(-(historyIndex * 9 + i + 1) * 30 * 60)));
        }
    };

    auto appendPeriodTableSnapshot = [](QVariantList &values, int historyIndex) {
        for (int setIndex = 0; setIndex < 2; ++setIndex)
        {
            for (int tableIndex = 0; tableIndex < kMaxDayScheduleTableCount; ++tableIndex)
            {
                values.append(buildDayScheduleTableValue(historyIndex, setIndex, tableIndex));
            }
        }
    };

    auto appendYearZoneSnapshot = [](QVariantList &values, int historyIndex) {
        for (int setIndex = 0; setIndex < 2; ++setIndex)
        {
            for (int zoneIndex = 0; zoneIndex < kMaxYearZoneCount; ++zoneIndex)
            {
                values.append(buildYearZoneEntryValue(historyIndex, setIndex, zoneIndex));
            }
        }
    };

    auto appendSequenceEnergySnapshot = [](QVariantList &values, double baseValue) {
        for (int i = 0; i < 16; ++i)
        {
            values.append(roundToDecimals(baseValue + i * 3.15, 2));
        }
    };

    auto appendCoverEventEnergySnapshot = [](QVariantList &values, double baseValue) {
        for (int i = 0; i < 12; ++i)
        {
            values.append(roundToDecimals(baseValue + i * 4.25, 2));
        }
    };

    QStringList lossVoltageFormatParts = {
        timeToSecondsFormat,
        timeToSecondsFormat,
        energyFormat,
        energyFormat,
        energyFormat,
        energyFormat};
    for (int dataPhase = 0; dataPhase < 3; ++dataPhase)
    {
        Q_UNUSED(dataPhase);
        lossVoltageFormatParts.append(energyFormat);
        lossVoltageFormatParts.append(energyFormat);
        lossVoltageFormatParts.append(energyFormat);
        lossVoltageFormatParts.append(energyFormat);
        lossVoltageFormatParts.append(voltageFormat);
        lossVoltageFormatParts.append(currentFormat);
        lossVoltageFormatParts.append(powerFormat);
        lossVoltageFormatParts.append(powerFormat);
        lossVoltageFormatParts.append(powerFactorFormat);
    }
    const QString lossVoltageRecordFormat = lossVoltageFormatParts.join(QStringLiteral(":"));
    const QString demandLimitTotalsFormat = repeatedFormatParts(countFormat, demandLimitNames.size());
    const QString demandLimitRecordFormat = QStringList{timeToSecondsFormat,
                                                        timeToSecondsFormat,
                                                        demandFormat,
                                                        timeToMinutesFormat}
                                                .join(QStringLiteral(":"));
    const QString programRecordFormat =
        QStringList{timeToSecondsFormat,
                    operatorCodeFormat,
                    repeatedFormatParts(diCodeFormat, programmedDiCount)}
            .join(QStringLiteral(":"));
    const QString meterClearRecordFormat =
        QStringList{timeToSecondsFormat, operatorCodeFormat, repeatedFormatParts(energyFormat, 24)}
            .join(QStringLiteral(":"));
    const QString demandClearRecordFormat =
        QStringList{timeToSecondsFormat,
                    operatorCodeFormat,
                    repeatedFormatParts(QStringList{demandFormat, timeToMinutesFormat}.join(QStringLiteral(":")), 24)}
            .join(QStringLiteral(":"));
    const QString eventClearRecordFormat =
        QStringList{timeToSecondsFormat, operatorCodeFormat, diCodeFormat}.join(QStringLiteral(":"));
    const QString timeCalRecordFormat =
        QStringList{operatorCodeFormat, timeToSecondsFormat, timeToSecondsFormat}.join(QStringLiteral(":"));
    const QString periodTableProgramRecordFormat =
        QStringList{timeToSecondsFormat, operatorCodeFormat, repeatedFormatParts(scheduleTableFormat, 16)}
            .join(QStringLiteral(":"));
    const QString yearZoneProgramRecordFormat =
        QStringList{timeToSecondsFormat, operatorCodeFormat, repeatedFormatParts(yearZoneEntryFormat, 28)}
            .join(QStringLiteral(":"));
    const QString coverEventRecordFormat =
        QStringList{timeToSecondsFormat, timeToSecondsFormat, repeatedFormatParts(energyFormat, 12)}
            .join(QStringLiteral(":"));
    const QString weekdayProgramRecordFormat =
        QStringList{timeToSecondsFormat, operatorCodeFormat, QStringLiteral("AA")}.join(QStringLiteral(":"));
    const QString holidayProgramRecordFormat =
        QStringList{timeToSecondsFormat, operatorCodeFormat, repeatedFormatParts(QStringLiteral("AA,AA,AA,AA"), kMaxHolidayCount)}
            .join(QStringLiteral(":"));
    const QString singleByteProgramRecordFormat = weekdayProgramRecordFormat;
    const QString settlementDayProgramRecordFormat =
        QStringList{timeToSecondsFormat, operatorCodeFormat, repeatedFormatParts(QStringLiteral("AA,AA"), 3)}
            .join(QStringLiteral(":"));
    const QString fullLossRecordFormat =
        QStringList{timeToSecondsFormat, currentFormat, timeToSecondsFormat}.join(QStringLiteral(":"));
    const QString simpleStartEndRecordFormat =
        QStringList{timeToSecondsFormat, timeToSecondsFormat}.join(QStringLiteral(":"));
    const QString reverseSequenceRecordFormat =
        QStringList{timeToSecondsFormat, timeToSecondsFormat, repeatedFormatParts(energyFormat, 16)}
            .join(QStringLiteral(":"));
    const QString voltageUnbalanceRecordFormat =
        QStringList{timeToSecondsFormat, timeToSecondsFormat, percentFormat, repeatedFormatParts(energyFormat, 16)}
            .join(QStringLiteral(":"));
    const QString totalVoltageComplianceFormat =
        QStringList{countFormat,
                    largePercentFormat,
                    largePercentFormat,
                    countFormat,
                    countFormat,
                    powerFactorFormat,
                    monthTimeFormat,
                    powerFactorFormat,
                    monthTimeFormat}
            .join(QStringLiteral(":"));
    const QString phaseVoltageComplianceFormat =
        QStringList{countFormat,
                    percentFormat,
                    percentFormat,
                    countFormat,
                    countFormat,
                    powerFactorFormat,
                    monthTimeFormat,
                    powerFactorFormat,
                    monthTimeFormat}
            .join(QStringLiteral(":"));

    auto initializePhaseVoltageEventFamily = [&](const QString &totalPrefix,
                                                 const QString &historyPrefixBase,
                                                 const QString &eventName,
                                                 double baseVoltageOffset) {
        for (int phaseIndex = 0; phaseIndex < phaseNames.size(); ++phaseIndex)
        {
            const QString totalCode = totalPrefix + upperHexByte(phaseIndex);
            m_dataItems[totalCode] = DataItem(
                totalCode,
                QStringLiteral("%1%2总次数，总累计时间").arg(phaseNames[phaseIndex], eventName),
                QVariantList{10 + phaseIndex * 2, 240 + phaseIndex * 30},
                QStringLiteral("次,分"),
                QStringList{countFormat, countFormat}.join(QStringLiteral(":")),
                false,
                0);

            const QString historyPrefix = historyPrefixBase + upperHexByte(phaseIndex + 1);
            for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
            {
                QVariantList values;
                const QDateTime endTime = now.addSecs(-(historyIndex * 8 + phaseIndex + 1) * 60 * 60);
                const QDateTime startTime = endTime.addSecs(-(historyIndex * 11 + 12) * 60);
                values.append(compactDateTimeToSeconds(startTime));
                values.append(compactDateTimeToSeconds(endTime));

                values.append(roundToDecimals(18.0 + historyIndex * 1.6, 2));
                values.append(roundToDecimals(2.0 + historyIndex * 0.3, 2));
                values.append(roundToDecimals(5.0 + historyIndex * 0.28, 2));
                values.append(roundToDecimals(4.2 + historyIndex * 0.24, 2));

                for (int dataPhase = 0; dataPhase < 3; ++dataPhase)
                {
                    const double baseVoltage = (dataPhase == phaseIndex)
                                                   ? baseVoltageOffset + historyIndex * 0.9
                                                   : 219.0 + dataPhase * 1.5 - historyIndex * 0.4;
                    values.append(roundToDecimals(7.0 + historyIndex * 0.55 + dataPhase * 0.7, 2));
                    values.append(roundToDecimals(1.0 + historyIndex * 0.12 + dataPhase * 0.15, 2));
                    values.append(roundToDecimals(2.1 + historyIndex * 0.17 + dataPhase * 0.18, 2));
                    values.append(roundToDecimals(1.8 + historyIndex * 0.14 + dataPhase * 0.16, 2));
                    values.append(roundToDecimals(baseVoltage, 1));
                    values.append(roundToDecimals(2.6 + historyIndex * 0.1 + dataPhase * 0.06, 3));
                    values.append(roundToDecimals(1.1 + historyIndex * 0.07 + dataPhase * 0.05, 4));
                    values.append(roundToDecimals(0.5 + historyIndex * 0.04 + dataPhase * 0.03, 4));
                    values.append(roundToDecimals(0.95 - historyIndex * 0.004 - dataPhase * 0.008, 3));
                }

                const QString code = phaseEventCode(historyPrefix, historyIndex);
                m_dataItems[code] = DataItem(
                    code,
                    QStringLiteral("（上%1次）%2%3记录").arg(historyIndex).arg(phaseNames[phaseIndex], eventName),
                    values,
                    QStringLiteral("-"),
                    lossVoltageRecordFormat,
                    false,
                    0);
            }
        }
    };

    auto initializePhaseElectricalEventFamily = [&](const QString &totalPrefix,
                                                    const QString &historyPrefixBase,
                                                    const QString &eventName,
                                                    double abnormalCurrentBase) {
        for (int phaseIndex = 0; phaseIndex < phaseNames.size(); ++phaseIndex)
        {
            const QString totalCode = totalPrefix + upperHexByte(phaseIndex);
            m_dataItems[totalCode] = DataItem(
                totalCode,
                QStringLiteral("%1%2总次数，总累计时间").arg(phaseNames[phaseIndex], eventName),
                QVariantList{8 + phaseIndex * 2, 190 + phaseIndex * 25},
                QStringLiteral("次,分"),
                QStringList{countFormat, countFormat}.join(QStringLiteral(":")),
                false,
                0);

            const QString historyPrefix = historyPrefixBase + upperHexByte(phaseIndex + 1);
            for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
            {
                QVariantList values;
                const QDateTime endTime = now.addSecs(-(historyIndex * 9 + phaseIndex + 3) * 60 * 60);
                const QDateTime startTime = endTime.addSecs(-(historyIndex * 10 + 16) * 60);
                values.append(compactDateTimeToSeconds(startTime));
                values.append(compactDateTimeToSeconds(endTime));

                values.append(roundToDecimals(16.0 + historyIndex * 1.45, 2));
                values.append(roundToDecimals(1.5 + historyIndex * 0.22, 2));
                values.append(roundToDecimals(4.8 + historyIndex * 0.24, 2));
                values.append(roundToDecimals(3.9 + historyIndex * 0.21, 2));

                for (int dataPhase = 0; dataPhase < 3; ++dataPhase)
                {
                    const double current = (dataPhase == phaseIndex)
                                               ? abnormalCurrentBase + historyIndex * 0.08
                                               : 2.4 + dataPhase * 0.15 + historyIndex * 0.05;
                    values.append(roundToDecimals(6.5 + historyIndex * 0.5 + dataPhase * 0.6, 2));
                    values.append(roundToDecimals(0.8 + historyIndex * 0.1 + dataPhase * 0.12, 2));
                    values.append(roundToDecimals(1.9 + historyIndex * 0.15 + dataPhase * 0.16, 2));
                    values.append(roundToDecimals(1.6 + historyIndex * 0.13 + dataPhase * 0.14, 2));
                    values.append(roundToDecimals(220.0 + dataPhase * 1.2 - historyIndex * 0.2, 1));
                    values.append(roundToDecimals(current, 3));
                    values.append(roundToDecimals(0.8 + historyIndex * 0.06 + dataPhase * 0.04, 4));
                    values.append(roundToDecimals(0.4 + historyIndex * 0.035 + dataPhase * 0.025, 4));
                    values.append(roundToDecimals(0.96 - historyIndex * 0.003 - dataPhase * 0.006, 3));
                }

                const QString code = phaseEventCode(historyPrefix, historyIndex);
                m_dataItems[code] = DataItem(
                    code,
                    QStringLiteral("（上%1次）%2%3记录").arg(historyIndex).arg(phaseNames[phaseIndex], eventName),
                    values,
                    QStringLiteral("-"),
                    lossVoltageRecordFormat,
                    false,
                    0);
            }
        }
    };

    auto initializePhaseSequenceEventFamily = [&](const QString &totalPrefix,
                                                  const QString &historyPrefixBase,
                                                  const QString &eventName,
                                                  double baseEnergy) {
        for (int phaseIndex = 0; phaseIndex < phaseNames.size(); ++phaseIndex)
        {
            const QString totalCode = totalPrefix + upperHexByte(phaseIndex);
            m_dataItems[totalCode] = DataItem(
                totalCode,
                QStringLiteral("%1%2总次数，总累计时间").arg(phaseNames[phaseIndex], eventName),
                QVariantList{4 + phaseIndex, 85 + phaseIndex * 18},
                QStringLiteral("次,分"),
                QStringList{countFormat, countFormat}.join(QStringLiteral(":")),
                false,
                0);

            const QString historyPrefix = historyPrefixBase + upperHexByte(phaseIndex + 1);
            for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
            {
                QVariantList values;
                const QDateTime endTime = now.addSecs(-(historyIndex * 15 + phaseIndex + 4) * 60 * 60);
                const QDateTime startTime = endTime.addSecs(-(historyIndex * 9 + 14) * 60);
                values.append(compactDateTimeToSeconds(startTime));
                values.append(compactDateTimeToSeconds(endTime));
                appendSequenceEnergySnapshot(values, baseEnergy + phaseIndex * 3.0 + historyIndex * 1.8);

                const QString code = phaseEventCode(historyPrefix, historyIndex);
                m_dataItems[code] = DataItem(
                    code,
                    QStringLiteral("（上%1次）%2%3记录").arg(historyIndex).arg(phaseNames[phaseIndex], eventName),
                    values,
                    QStringLiteral("-"),
                    reverseSequenceRecordFormat,
                    false,
                    0);
            }
        }
    };

    initializePhaseVoltageEventFamily(QStringLiteral("0x030100"), QStringLiteral("0x0301"), QStringLiteral("失压"), 168.0);
    initializePhaseVoltageEventFamily(QStringLiteral("0x030200"), QStringLiteral("0x0302"), QStringLiteral("欠压"), 172.0);
    initializePhaseVoltageEventFamily(QStringLiteral("0x030300"), QStringLiteral("0x0303"), QStringLiteral("过压"), 248.0);
    initializePhaseVoltageEventFamily(QStringLiteral("0x030400"), QStringLiteral("0x0304"), QStringLiteral("断相"), 0.0);

    m_dataItems["0x03050000"] = DataItem(
        "0x03050000",
        "全失压总次数，总累计时间",
        QVariantList{6, 180},
        "次,分",
        QStringList{countFormat, countFormat}.join(QStringLiteral(":")),
        false,
        0);
    for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
    {
        const QString code = QStringLiteral("0x030500") + upperHexByte(historyIndex);
        const QDateTime endTime = now.addSecs(-(historyIndex * 14 + 2) * 60 * 60);
        const QDateTime startTime = endTime.addSecs(-(historyIndex * 15 + 20) * 60);
        m_dataItems[code] = DataItem(
            code,
            QStringLiteral("（上%1次）全失压记录").arg(historyIndex),
            QVariantList{
                compactDateTimeToSeconds(startTime),
                roundToDecimals(0.08 + historyIndex * 0.012, 3),
                compactDateTimeToSeconds(endTime)},
            QStringLiteral("-"),
            fullLossRecordFormat,
            false,
            0);
    }

    m_dataItems["0x03060000"] = DataItem(
        "0x03060000",
        "辅助电源失电总次数，总累计时间",
        QVariantList{4, 95},
        "次,分",
        QStringList{countFormat, countFormat}.join(QStringLiteral(":")),
        false,
        0);
    for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
    {
        const QString code = QStringLiteral("0x030600") + upperHexByte(historyIndex);
        const QDateTime endTime = now.addSecs(-(historyIndex * 16 + 1) * 60 * 60);
        const QDateTime startTime = endTime.addSecs(-(historyIndex * 9 + 10) * 60);
        m_dataItems[code] = DataItem(
            code,
            QStringLiteral("（上%1次）辅助电源失电记录").arg(historyIndex),
            QVariantList{compactDateTimeToSeconds(startTime), compactDateTimeToSeconds(endTime)},
            QStringLiteral("-"),
            simpleStartEndRecordFormat,
            false,
            0);
    }

    m_dataItems["0x03070000"] = DataItem(
        "0x03070000",
        "电压逆相序总次数，总累计时间",
        QVariantList{3, 72},
        "次,分",
        QStringList{countFormat, countFormat}.join(QStringLiteral(":")),
        false,
        0);
    for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
    {
        const QString code = QStringLiteral("0x030700") + upperHexByte(historyIndex);
        QVariantList values;
        const QDateTime endTime = now.addSecs(-(historyIndex * 17 + 3) * 60 * 60);
        const QDateTime startTime = endTime.addSecs(-(historyIndex * 11 + 8) * 60);
        values.append(compactDateTimeToSeconds(startTime));
        values.append(compactDateTimeToSeconds(endTime));
        appendSequenceEnergySnapshot(values, 10.0 + historyIndex * 2.2);
        m_dataItems[code] = DataItem(
            code,
            QStringLiteral("（上%1次）电压逆相序记录").arg(historyIndex),
            values,
            QStringLiteral("-"),
            reverseSequenceRecordFormat,
            false,
            0);
    }

    m_dataItems["0x03080000"] = DataItem(
        "0x03080000",
        "电流逆相序总次数，总累计时间",
        QVariantList{2, 48},
        "次,分",
        QStringList{countFormat, countFormat}.join(QStringLiteral(":")),
        false,
        0);
    for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
    {
        const QString code = QStringLiteral("0x030800") + upperHexByte(historyIndex);
        QVariantList values;
        const QDateTime endTime = now.addSecs(-(historyIndex * 19 + 2) * 60 * 60);
        const QDateTime startTime = endTime.addSecs(-(historyIndex * 10 + 6) * 60);
        values.append(compactDateTimeToSeconds(startTime));
        values.append(compactDateTimeToSeconds(endTime));
        appendSequenceEnergySnapshot(values, 8.0 + historyIndex * 1.9);
        m_dataItems[code] = DataItem(
            code,
            QStringLiteral("（上%1次）电流逆相序记录").arg(historyIndex),
            values,
            QStringLiteral("-"),
            reverseSequenceRecordFormat,
            false,
            0);
    }

    m_dataItems["0x03090000"] = DataItem(
        "0x03090000",
        "电压不平衡总次数，总累计时间",
        QVariantList{5, 110},
        "次,分",
        QStringList{countFormat, countFormat}.join(QStringLiteral(":")),
        false,
        0);
    for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
    {
        const QString code = QStringLiteral("0x030900") + upperHexByte(historyIndex);
        QVariantList values;
        const QDateTime endTime = now.addSecs(-(historyIndex * 13 + 5) * 60 * 60);
        const QDateTime startTime = endTime.addSecs(-(historyIndex * 12 + 7) * 60);
        values.append(compactDateTimeToSeconds(startTime));
        values.append(compactDateTimeToSeconds(endTime));
        values.append(roundToDecimals(1.5 + historyIndex * 0.18, 2));
        appendSequenceEnergySnapshot(values, 9.5 + historyIndex * 2.0);
        m_dataItems[code] = DataItem(
            code,
            QStringLiteral("（上%1次）电压不平衡记录").arg(historyIndex),
            values,
            QStringLiteral("-"),
            voltageUnbalanceRecordFormat,
            false,
            0);
    }

    m_dataItems["0x030A0000"] = DataItem(
        "0x030A0000",
        "电流不平衡总次数，总累计时间",
        QVariantList{4, 96},
        "次,分",
        QStringList{countFormat, countFormat}.join(QStringLiteral(":")),
        false,
        0);
    for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
    {
        const QString code = QStringLiteral("0x030A00") + upperHexByte(historyIndex);
        QVariantList values;
        const QDateTime endTime = now.addSecs(-(historyIndex * 14 + 6) * 60 * 60);
        const QDateTime startTime = endTime.addSecs(-(historyIndex * 10 + 11) * 60);
        values.append(compactDateTimeToSeconds(startTime));
        values.append(compactDateTimeToSeconds(endTime));
        values.append(roundToDecimals(2.2 + historyIndex * 0.2, 2));
        appendSequenceEnergySnapshot(values, 8.8 + historyIndex * 1.9);
        m_dataItems[code] = DataItem(
            code,
            QStringLiteral("（上%1次）电流不平衡记录").arg(historyIndex),
            values,
            QStringLiteral("-"),
            voltageUnbalanceRecordFormat,
            false,
            0);
    }

    initializePhaseElectricalEventFamily(QStringLiteral("0x030B00"), QStringLiteral("0x030B"), QStringLiteral("失流"), 0.08);
    initializePhaseElectricalEventFamily(QStringLiteral("0x030C00"), QStringLiteral("0x030C"), QStringLiteral("过流"), 9.50);
    initializePhaseElectricalEventFamily(QStringLiteral("0x030D00"), QStringLiteral("0x030D"), QStringLiteral("断流"), 0.00);
    initializePhaseSequenceEventFamily(QStringLiteral("0x030E00"), QStringLiteral("0x030E"), QStringLiteral("潮流反向"), 11.0);
    initializePhaseSequenceEventFamily(QStringLiteral("0x030F00"), QStringLiteral("0x030F"), QStringLiteral("过载"), 13.5);

    auto appendVoltageComplianceStats = [&](const QString &code,
                                            const QString &description,
                                            bool totalItem,
                                            int monthIndex,
                                            int phaseIndex) {
        const int monitorMinutes = 43000 - monthIndex * 2300 - qMax(0, phaseIndex) * 500;
        const double qualifiedRate = totalItem ? (97.40 - monthIndex * 0.18) : (98.20 - monthIndex * 0.22 - phaseIndex * 0.15);
        const double overLimitRate = totalItem ? (1.25 + monthIndex * 0.04) : (0.82 + monthIndex * 0.05 + phaseIndex * 0.03);
        const int overUpperMinutes = 120 + monthIndex * 8 + qMax(0, phaseIndex) * 5;
        const int overLowerMinutes = 95 + monthIndex * 7 + qMax(0, phaseIndex) * 4;
        const double highestVoltage = 241.0 - monthIndex * 0.6 - qMax(0, phaseIndex) * 0.4;
        const double lowestVoltage = 201.0 + monthIndex * 0.5 + qMax(0, phaseIndex) * 0.3;
        const QString highestTime = QDateTime(now.date().addMonths(-monthIndex), QTime(9 + (monthIndex % 8), 20)).toString(QStringLiteral("MMddHHmm"));
        const QString lowestTime = QDateTime(now.date().addMonths(-monthIndex), QTime(18 - (monthIndex % 6), 35)).toString(QStringLiteral("MMddHHmm"));

        m_dataItems[code] = DataItem(
            code,
            description,
            QVariantList{
                monitorMinutes,
                roundToDecimals(qualifiedRate, 2),
                roundToDecimals(overLimitRate, 2),
                overUpperMinutes,
                overLowerMinutes,
                roundToDecimals(highestVoltage, 1),
                highestTime,
                roundToDecimals(lowestVoltage, 1),
                lowestTime},
            QStringLiteral("-"),
            totalItem ? totalVoltageComplianceFormat : phaseVoltageComplianceFormat,
            false,
            totalItem ? 2 : 2);
    };

    appendVoltageComplianceStats(QStringLiteral("0x03100000"), QStringLiteral("（本月）电压合格率统计数据"), true, 0, -1);
    for (int historyIndex = 1; historyIndex <= 12; ++historyIndex)
    {
        appendVoltageComplianceStats(QStringLiteral("0x031000") + upperHexByte(historyIndex),
                                     QStringLiteral("（上%1月）电压合格率统计数据").arg(historyIndex),
                                     true,
                                     historyIndex,
                                     -1);
    }

    for (int phaseIndex = 0; phaseIndex < phaseNames.size(); ++phaseIndex)
    {
        const QString phasePrefix = QStringLiteral("0x0310") + upperHexByte(phaseIndex + 1);
        appendVoltageComplianceStats(phasePrefix + QStringLiteral("00"),
                                     QStringLiteral("（本月）%1电压合格率统计数据").arg(phaseNames[phaseIndex]),
                                     false,
                                     0,
                                     phaseIndex);
        for (int historyIndex = 1; historyIndex <= 12; ++historyIndex)
        {
            appendVoltageComplianceStats(phasePrefix + upperHexByte(historyIndex),
                                         QStringLiteral("（上%1月）%2电压合格率统计数据").arg(historyIndex).arg(phaseNames[phaseIndex]),
                                         false,
                                         historyIndex,
                                         phaseIndex);
        }
    }

    m_dataItems["0x03110000"] = DataItem("0x03110000", "掉电总次数", 9, "次", "AA,AA,AA", false, 0);
    for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
    {
        const QString code = QStringLiteral("0x031100") + upperHexByte(historyIndex);
        const QDateTime endTime = now.addSecs(-(historyIndex * 12 + 3) * 60 * 60);
        const QDateTime startTime = endTime.addSecs(-(historyIndex * 8 + 20) * 60);
        m_dataItems[code] = DataItem(
            code,
            QStringLiteral("（上%1次）掉电记录").arg(historyIndex),
            QVariantList{compactDateTimeToSeconds(startTime), compactDateTimeToSeconds(endTime)},
            QStringLiteral("-"),
            QStringList{timeToSecondsFormat, timeToSecondsFormat}.join(QStringLiteral(":")),
            false,
            0);
    }

    m_dataItems["0x03120000"] = DataItem(
        "0x03120000",
        "需量超限总次数",
        QVariantList{7, 5, 4, 3, 2, 1},
        "次",
        demandLimitTotalsFormat,
        false,
        0);

    for (int demandIndex = 0; demandIndex < demandLimitNames.size(); ++demandIndex)
    {
        for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
        {
            const QString code = QStringLiteral("0x0312") + upperHexByte(demandIndex + 1) + upperHexByte(historyIndex);
            const QDateTime endTime = now.addSecs(-(historyIndex * 10 + demandIndex + 2) * 60 * 60);
            const QDateTime startTime = endTime.addSecs(-(historyIndex * 12 + 18) * 60);
            const QDateTime maxDemandTime = startTime.addSecs((historyIndex * 8 + demandIndex + 1) * 60);
            const double demandValue = roundToDecimals(3.2 + demandIndex * 0.45 + historyIndex * 0.18, 4);
            m_dataItems[code] = DataItem(
                code,
                QStringLiteral("（上%1次）%2需量超限记录").arg(historyIndex).arg(demandLimitNames[demandIndex]),
                QVariantList{
                    compactDateTimeToSeconds(startTime),
                    compactDateTimeToSeconds(endTime),
                    demandValue,
                    compactDateTimeToMinutes(maxDemandTime)},
                QStringLiteral("-"),
                demandLimitRecordFormat,
                false,
                0);
        }
    }

    m_dataItems["0x03300000"] = DataItem("0x03300000", "编程总次数", 8, "次", "AA,AA,AA", false, 0);
    m_dataItems["0x03300100"] = DataItem("0x03300100", "电表清零总次数", 5, "次", "AA,AA,AA", false, 0);
    m_dataItems["0x03300200"] = DataItem("0x03300200", "需量清零总次数", 4, "次", "AA,AA,AA", false, 0);
    m_dataItems["0x03300300"] = DataItem("0x03300300", "事件清零总次数", 3, "次", "AA,AA,AA", false, 0);
    m_dataItems["0x03300400"] = DataItem("0x03300400", "校时总次数", 6, "次", "AA,AA,AA", false, 0);
    m_dataItems["0x03300500"] = DataItem("0x03300500", "时段表编程总次数", 2, "次", "AA,AA,AA", false, 0);
    m_dataItems["0x03300600"] = DataItem("0x03300600", "时区表编程总次数", 1, "次", "AA,AA,AA", false, 0);
    m_dataItems["0x03300700"] = DataItem("0x03300700", "周休日编程总次数", 2, "次", "AA,AA,AA", false, 0);
    m_dataItems["0x03300800"] = DataItem("0x03300800", "节假日编程总次数", 1, "次", "AA,AA,AA", false, 0);
    m_dataItems["0x03300900"] = DataItem("0x03300900", "有功组合方式编程总次数", 2, "次", "AA,AA,AA", false, 0);
    m_dataItems["0x03300A00"] = DataItem("0x03300A00", "无功组合方式1编程总次数", 2, "次", "AA,AA,AA", false, 0);
    m_dataItems["0x03300B00"] = DataItem("0x03300B00", "无功组合方式2编程总次数", 2, "次", "AA,AA,AA", false, 0);
    m_dataItems["0x03300C00"] = DataItem("0x03300C00", "结算日编程总次数", 2, "次", "AA,AA,AA", false, 0);
    m_dataItems["0x03300D00"] = DataItem("0x03300D00", "开表盖总次数", 3, "次", "AA,AA,AA", false, 0);
    m_dataItems["0x03300E00"] = DataItem("0x03300E00", "开端钮盒总次数", 2, "次", "AA,AA,AA", false, 0);

    const QStringList programmedDiCodes = {
        QStringLiteral("03010000"), QStringLiteral("03010001"), QStringLiteral("03010002"),
        QStringLiteral("03110000"), QStringLiteral("03120000"), QStringLiteral("03300000"),
        QStringLiteral("03300100"), QStringLiteral("03300200"), QStringLiteral("03300300"),
        QStringLiteral("03300400"), QStringLiteral("03300500"), QStringLiteral("03300600")};

    for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
    {
        const QString operatorCode = defaultOperatorCode(historyIndex);
        const QDateTime eventTime = now.addSecs(-(historyIndex * 5 + 2) * 60 * 60);

        QVariantList programmingRecord;
        programmingRecord.append(compactDateTimeToSeconds(eventTime));
        programmingRecord.append(operatorCode);
        for (const QString &diCode : programmedDiCodes)
        {
            programmingRecord.append(diCode);
        }
        const QString programmingCode = QStringLiteral("0x033000") + upperHexByte(historyIndex);
        m_dataItems[programmingCode] = DataItem(
            programmingCode,
            QStringLiteral("（上%1次）编程记录").arg(historyIndex),
            programmingRecord,
            QStringLiteral("-"),
            programRecordFormat,
            false,
            0);

        QVariantList meterClearRecord;
        meterClearRecord.append(compactDateTimeToSeconds(now.addSecs(-(historyIndex * 8 + 1) * 60 * 60)));
        meterClearRecord.append(operatorCode);
        appendEnergySnapshot(meterClearRecord, 120.0 + historyIndex * 12.5);
        const QString meterClearCode = QStringLiteral("0x033001") + upperHexByte(historyIndex);
        m_dataItems[meterClearCode] = DataItem(
            meterClearCode,
            QStringLiteral("（上%1次）电表清零记录").arg(historyIndex),
            meterClearRecord,
            QStringLiteral("-"),
            meterClearRecordFormat,
            false,
            0);

        QVariantList demandClearRecord;
        demandClearRecord.append(compactDateTimeToSeconds(now.addSecs(-(historyIndex * 9 + 2) * 60 * 60)));
        demandClearRecord.append(operatorCode);
        appendDemandSnapshot(demandClearRecord, historyIndex, 1.8 + historyIndex * 0.2);
        const QString demandClearCode = QStringLiteral("0x033002") + upperHexByte(historyIndex);
        m_dataItems[demandClearCode] = DataItem(
            demandClearCode,
            QStringLiteral("（上%1次）需量清零记录").arg(historyIndex),
            demandClearRecord,
            QStringLiteral("-"),
            demandClearRecordFormat,
            false,
            0);

        const QString eventClearCode = QStringLiteral("0x033003") + upperHexByte(historyIndex);
        m_dataItems[eventClearCode] = DataItem(
            eventClearCode,
            QStringLiteral("（上%1次）事件清零记录").arg(historyIndex),
            QVariantList{compactDateTimeToSeconds(now.addSecs(-(historyIndex * 11 + 3) * 60 * 60)),
                         operatorCode,
                         programmedDiCodes.at((historyIndex - 1) % programmedDiCodes.size())},
            QStringLiteral("-"),
            eventClearRecordFormat,
            false,
            0);

        const QDateTime afterTime = now.addSecs(-(historyIndex * 4 + 1) * 60 * 60);
        const QDateTime beforeTime = afterTime.addSecs(-(historyIndex * 6 + 12) * 60);
        const QString timeCalCode = QStringLiteral("0x033004") + upperHexByte(historyIndex);
        m_dataItems[timeCalCode] = DataItem(
            timeCalCode,
            QStringLiteral("（上%1次）校时记录").arg(historyIndex),
            QVariantList{operatorCode,
                         compactDateTimeToSeconds(beforeTime),
                         compactDateTimeToSeconds(afterTime)},
            QStringLiteral("-"),
            timeCalRecordFormat,
            false,
            0);

        QVariantList periodTableRecord;
        periodTableRecord.append(compactDateTimeToSeconds(now.addSecs(-(historyIndex * 6 + 4) * 60 * 60)));
        periodTableRecord.append(operatorCode);
        appendPeriodTableSnapshot(periodTableRecord, historyIndex);
        const QString periodTableCode = QStringLiteral("0x033005") + upperHexByte(historyIndex);
        m_dataItems[periodTableCode] = DataItem(
            periodTableCode,
            QStringLiteral("（上%1次）时段表编程记录").arg(historyIndex),
            periodTableRecord,
            QStringLiteral("-"),
            periodTableProgramRecordFormat,
            false,
            0);

        QVariantList yearZoneRecord;
        yearZoneRecord.append(compactDateTimeToSeconds(now.addSecs(-(historyIndex * 7 + 5) * 60 * 60)));
        yearZoneRecord.append(operatorCode);
        appendYearZoneSnapshot(yearZoneRecord, historyIndex);
        const QString yearZoneCode = QStringLiteral("0x033006") + upperHexByte(historyIndex);
        m_dataItems[yearZoneCode] = DataItem(
            yearZoneCode,
            QStringLiteral("（上%1次）时区表编程记录").arg(historyIndex),
            yearZoneRecord,
            QStringLiteral("-"),
            yearZoneProgramRecordFormat,
            false,
            0);

        const QString weekdayCode = QStringLiteral("0x033007") + upperHexByte(historyIndex);
        m_dataItems[weekdayCode] = DataItem(
            weekdayCode,
            QStringLiteral("（上%1次）周休日编程记录").arg(historyIndex),
            QVariantList{compactDateTimeToSeconds(now.addSecs(-(historyIndex * 8 + 6) * 60 * 60)),
                         operatorCode,
                         (historyIndex % kMaxDayScheduleTableCount) + 1},
            QStringLiteral("-"),
            weekdayProgramRecordFormat,
            false,
            0);

        QVariantList holidayRecord;
        holidayRecord.append(compactDateTimeToSeconds(now.addSecs(-(historyIndex * 9 + 7) * 60 * 60)));
        holidayRecord.append(operatorCode);
        for (int holidayIndex = 0; holidayIndex < kMaxHolidayCount; ++holidayIndex)
        {
            const QDate holidayDate(now.date().year(), (holidayIndex % 12) + 1, qMin((holidayIndex % 28) + 1, 28));
            holidayRecord.append(buildHolidayEntryValue(holidayDate, (holidayIndex % kMaxDayScheduleTableCount) + 1));
        }
        const QString holidayCode = QStringLiteral("0x033008") + upperHexByte(historyIndex);
        m_dataItems[holidayCode] = DataItem(
            holidayCode,
            QStringLiteral("（上%1次）节假日编程记录").arg(historyIndex),
            holidayRecord,
            QStringLiteral("-"),
            holidayProgramRecordFormat,
            false,
            0);

        const QList<QPair<QString, QString>> combinationProgramItems = {
            {QStringLiteral("0x033009"), QStringLiteral("有功组合方式编程")},
            {QStringLiteral("0x03300A"), QStringLiteral("无功组合方式1编程")},
            {QStringLiteral("0x03300B"), QStringLiteral("无功组合方式2编程")}};
        for (int programIndex = 0; programIndex < combinationProgramItems.size(); ++programIndex)
        {
            const QString code = combinationProgramItems.at(programIndex).first + upperHexByte(historyIndex);
            m_dataItems[code] = DataItem(
                code,
                QStringLiteral("（上%1次）%2记录").arg(historyIndex).arg(combinationProgramItems.at(programIndex).second),
                QVariantList{compactDateTimeToSeconds(now.addSecs(-(historyIndex * 10 + programIndex + 8) * 60 * 60)),
                             operatorCode,
                             (historyIndex + programIndex) % 10},
                QStringLiteral("-"),
                singleByteProgramRecordFormat,
                false,
                0);
        }

        const QString settlementDayCode = QStringLiteral("0x03300C") + upperHexByte(historyIndex);
        m_dataItems[settlementDayCode] = DataItem(
            settlementDayCode,
            QStringLiteral("（上%1次）结算日编程记录").arg(historyIndex),
            QVariantList{compactDateTimeToSeconds(now.addSecs(-(historyIndex * 11 + 9) * 60 * 60)),
                         operatorCode,
                         QStringLiteral("0100"),
                         QStringLiteral("1500"),
                         QStringLiteral("9999")},
            QStringLiteral("-"),
            settlementDayProgramRecordFormat,
            false,
            0);

        const QDateTime coverOpenEndTime = now.addSecs(-(historyIndex * 10 + 1) * 60 * 60);
        const QDateTime coverOpenStartTime = coverOpenEndTime.addSecs(-(historyIndex * 6 + 8) * 60);
        QVariantList coverOpenRecord;
        coverOpenRecord.append(compactDateTimeToSeconds(coverOpenStartTime));
        coverOpenRecord.append(compactDateTimeToSeconds(coverOpenEndTime));
        appendCoverEventEnergySnapshot(coverOpenRecord, 14.64 + historyIndex * 3.2);
        const QString coverOpenCode = QStringLiteral("0x03300D") + upperHexByte(historyIndex);
        m_dataItems[coverOpenCode] = DataItem(
            coverOpenCode,
            QStringLiteral("（上%1次）开表盖事件记录").arg(historyIndex),
            coverOpenRecord,
            QStringLiteral("-"),
            coverEventRecordFormat,
            false,
            0);

        const QDateTime terminalCoverEndTime = now.addSecs(-(historyIndex * 12 + 2) * 60 * 60);
        const QDateTime terminalCoverStartTime = terminalCoverEndTime.addSecs(-(historyIndex * 7 + 9) * 60);
        QVariantList terminalCoverRecord;
        terminalCoverRecord.append(compactDateTimeToSeconds(terminalCoverStartTime));
        terminalCoverRecord.append(compactDateTimeToSeconds(terminalCoverEndTime));
        appendCoverEventEnergySnapshot(terminalCoverRecord, 21.30 + historyIndex * 3.6);
        const QString terminalCoverCode = QStringLiteral("0x03300E") + upperHexByte(historyIndex);
        m_dataItems[terminalCoverCode] = DataItem(
            terminalCoverCode,
            QStringLiteral("（上%1次）开端钮盒事件记录").arg(historyIndex),
            terminalCoverRecord,
            QStringLiteral("-"),
            coverEventRecordFormat,
            false,
            0);
    }

    for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
    {
        const QString suffix = upperHexByte(historyIndex);
        const QDateTime purchaseTime = now.addDays(-historyIndex * 18).addSecs(-historyIndex * 3600);
        const int purchaseCount = 20 + historyIndex;
        const double purchaseEnergy = roundToDecimals(50.0 + historyIndex * 12.5, 2);
        const double remainingEnergyBefore = roundToDecimals(120.0 - historyIndex * 3.5, 2);
        const double remainingEnergyAfter = roundToDecimals(remainingEnergyBefore + purchaseEnergy, 2);
        const double cumulativeEnergy = roundToDecimals(800.0 + historyIndex * 55.0, 2);

        m_dataItems[QStringLiteral("0x033201") + suffix] = DataItem(QStringLiteral("0x033201") + suffix,
                                                                     QStringLiteral("（上%1次）购电日期").arg(historyIndex),
                                                                     compactDateTimeToMinutes(purchaseTime),
                                                                     QStringLiteral("-"),
                                                                     timeToMinutesFormat,
                                                                     false,
                                                                     0);
        m_dataItems[QStringLiteral("0x033202") + suffix] = DataItem(QStringLiteral("0x033202") + suffix,
                                                                     QStringLiteral("（上%1次）购电后总购电次数").arg(historyIndex),
                                                                     purchaseCount,
                                                                     QStringLiteral("次"),
                                                                     QStringLiteral("AA,AA"),
                                                                     false,
                                                                     0);
        m_dataItems[QStringLiteral("0x033203") + suffix] = DataItem(QStringLiteral("0x033203") + suffix,
                                                                     QStringLiteral("（上%1次）购电量").arg(historyIndex),
                                                                     purchaseEnergy,
                                                                     QStringLiteral("kWh"),
                                                                     energyFormat,
                                                                     false,
                                                                     2);
        m_dataItems[QStringLiteral("0x033204") + suffix] = DataItem(QStringLiteral("0x033204") + suffix,
                                                                     QStringLiteral("（上%1次）购电前剩余电量").arg(historyIndex),
                                                                     remainingEnergyBefore,
                                                                     QStringLiteral("kWh"),
                                                                     energyFormat,
                                                                     false,
                                                                     2);
        m_dataItems[QStringLiteral("0x033205") + suffix] = DataItem(QStringLiteral("0x033205") + suffix,
                                                                     QStringLiteral("（上%1次）购电后剩余电量").arg(historyIndex),
                                                                     remainingEnergyAfter,
                                                                     QStringLiteral("kWh"),
                                                                     energyFormat,
                                                                     false,
                                                                     2);
        m_dataItems[QStringLiteral("0x033206") + suffix] = DataItem(QStringLiteral("0x033206") + suffix,
                                                                     QStringLiteral("（上%1次）购电后累计购电量").arg(historyIndex),
                                                                     cumulativeEnergy,
                                                                     QStringLiteral("kWh"),
                                                                     energyFormat,
                                                                     false,
                                                                     2);

        const double purchaseAmount = roundToDecimals(35.0 + historyIndex * 8.8, 2);
        const double balanceBefore = roundToDecimals(160.0 - historyIndex * 6.0, 2);
        const double balanceAfter = roundToDecimals(balanceBefore + purchaseAmount, 2);
        const double cumulativeAmount = roundToDecimals(1200.0 + historyIndex * 88.0, 2);
        m_dataItems[QStringLiteral("0x033301") + suffix] = DataItem(QStringLiteral("0x033301") + suffix,
                                                                     QStringLiteral("（上%1次）购电金额日期").arg(historyIndex),
                                                                     compactDateTimeToMinutes(purchaseTime),
                                                                     QStringLiteral("-"),
                                                                     timeToMinutesFormat,
                                                                     false,
                                                                     0);
        m_dataItems[QStringLiteral("0x033302") + suffix] = DataItem(QStringLiteral("0x033302") + suffix,
                                                                     QStringLiteral("（上%1次）购电金额后总购电次数").arg(historyIndex),
                                                                     purchaseCount,
                                                                     QStringLiteral("次"),
                                                                     QStringLiteral("AA,AA"),
                                                                     false,
                                                                     0);
        m_dataItems[QStringLiteral("0x033303") + suffix] = DataItem(QStringLiteral("0x033303") + suffix,
                                                                     QStringLiteral("（上%1次）购电金额").arg(historyIndex),
                                                                     purchaseAmount,
                                                                     QStringLiteral("元"),
                                                                     energyFormat,
                                                                     false,
                                                                     2);
        m_dataItems[QStringLiteral("0x033304") + suffix] = DataItem(QStringLiteral("0x033304") + suffix,
                                                                     QStringLiteral("（上%1次）购电前剩余金额").arg(historyIndex),
                                                                     balanceBefore,
                                                                     QStringLiteral("元"),
                                                                     energyFormat,
                                                                     false,
                                                                     2);
        m_dataItems[QStringLiteral("0x033305") + suffix] = DataItem(QStringLiteral("0x033305") + suffix,
                                                                     QStringLiteral("（上%1次）购电后剩余金额").arg(historyIndex),
                                                                     balanceAfter,
                                                                     QStringLiteral("元"),
                                                                     energyFormat,
                                                                     false,
                                                                     2);
        m_dataItems[QStringLiteral("0x033306") + suffix] = DataItem(QStringLiteral("0x033306") + suffix,
                                                                     QStringLiteral("（上%1次）累计购电金额").arg(historyIndex),
                                                                     cumulativeAmount,
                                                                     QStringLiteral("元"),
                                                                     energyFormat,
                                                                     false,
                                                                     2);
    }
}

void MeterModel::initializeParameterData()
{
    // 参变量 (0x04xxxxxx)
    const QDateTime now = QDateTime::currentDateTime();
    const int configuredYearZoneCount = 2;
    const int configuredDayScheduleTableCount = 2;
    const int configuredDayScheduleCount = 8;
    const int configuredRateCount = 5;
    const int configuredHolidayCount = 12;
    const int configuredStepCount = 3;
    const int configuredAutoScreenCount = 6;
    const int configuredKeyScreenCount = 4;

    m_dataItems["0x04000101"] = DataItem("0x04000101", "当前日期(含星期代码)", compactDateWithWeek(now.date()), "-", "AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000102"] = DataItem("0x04000102", "当前时间", now.time().toString(QStringLiteral("HHmmss")), "-", "AA,AA,AA", true, 0);
    m_dataItems["0x04000103"] = DataItem("0x04000103", "最大需量周期", 15, "分钟", "AA", true, 0);
    m_dataItems["0x04000104"] = DataItem("0x04000104", "滑差时间", 1, "分钟", "AA", true, 0);
    m_dataItems["0x04000105"] = DataItem("0x04000105", "校表脉冲宽度", 100, "毫秒", "AA,AA", true, 0);
    m_dataItems["0x04000106"] = DataItem("0x04000106", "两套时区表切换时间", compactDateTimeToMinutes(now.addDays(30)), "-", "AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000107"] = DataItem("0x04000107", "两套日时段表切换时间", compactDateTimeToMinutes(now.addDays(60)), "-", "AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000108"] = DataItem("0x04000108", "两套费率电价切换时间", compactDateTimeToMinutes(now.addDays(90)), "-", "AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000109"] = DataItem("0x04000109", "两套梯度切换时间", compactDateTimeToMinutes(now.addDays(120)), "-", "AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000201"] = DataItem("0x04000201", "年时区数p", configuredYearZoneCount, "个", "AA", true, 0);
    m_dataItems["0x04000202"] = DataItem("0x04000202", "日时段表数q", configuredDayScheduleTableCount, "个", "AA", true, 0);
    m_dataItems["0x04000203"] = DataItem("0x04000203", "日时段数m", configuredDayScheduleCount, "个", "AA", true, 0);
    m_dataItems["0x04000204"] = DataItem("0x04000204", "费率数k", configuredRateCount, "个", "AA", true, 0);
    m_dataItems["0x04000205"] = DataItem("0x04000205", "公共假日数n", configuredHolidayCount, "个", "AA,AA", true, 0);
    m_dataItems["0x04000206"] = DataItem("0x04000206", "谐波分析次数", 1, "次", "AA", true, 0);
    m_dataItems["0x04000207"] = DataItem("0x04000207", "梯度数", configuredStepCount, "个", "AA", true, 0);

    m_dataItems["0x04000301"] = DataItem("0x04000301", "自动循环显示屏数", configuredAutoScreenCount, "个", "AA", true, 0);
    m_dataItems["0x04000302"] = DataItem("0x04000302", "每屏显示时间", 5, "秒", "AA", true, 0);
    m_dataItems["0x04000303"] = DataItem("0x04000303", "显示电能小数位数", 2, "位", "AA", true, 0);
    m_dataItems["0x04000304"] = DataItem("0x04000304", "显示功率(最大需量)小数位数", 4, "位", "AA", true, 0);
    m_dataItems["0x04000305"] = DataItem("0x04000305", "按键循环显示屏数", configuredKeyScreenCount, "个", "AA", true, 0);
    m_dataItems["0x04000306"] = DataItem("0x04000306", "电流互感器变比", QStringLiteral("000200"), "-", "AA,AA,AA", true, 0);
    m_dataItems["0x04000307"] = DataItem("0x04000307", "电压互感器变比", QStringLiteral("000001"), "-", "AA,AA,AA", true, 0);

    m_dataItems["0x04000401"] = DataItem("0x04000401", "通信地址", m_meterAddress, "-", "AA,AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000402"] = DataItem("0x04000402", "表号", QStringLiteral("000000000001"), "-", "AA,AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000403"] = DataItem("0x04000403", "资产管理编码(ASCII码)", QStringLiteral("20260429000000000000000000000001"), "-", repeatedByteFormat(32), true, 0);
    m_dataItems["0x04000404"] = DataItem("0x04000404", "额定电压(ASCII码)", QStringLiteral("000000220000"), "-", repeatedByteFormat(6), true, 0);
    m_dataItems["0x04000405"] = DataItem("0x04000405", "额定电流/基本电流(ASCII码)", QStringLiteral("000000050000"), "-", repeatedByteFormat(6), true, 0);
    m_dataItems["0x04000406"] = DataItem("0x04000406", "最大电流(ASCII码)", QStringLiteral("000000600000"), "-", repeatedByteFormat(6), true, 0);
    m_dataItems["0x04000407"] = DataItem("0x04000407", "有功准确度等级(ASCII码)", QStringLiteral("0001"), "-", repeatedByteFormat(4), true, 0);
    m_dataItems["0x04000408"] = DataItem("0x04000408", "无功准确度等级(ASCII码)", QStringLiteral("0002"), "-", repeatedByteFormat(4), true, 0);
    m_dataItems["0x04000409"] = DataItem("0x04000409", "电表有功常数", QStringLiteral("1600"), "imp/kWh", "AA,AA", true, 0);
    m_dataItems["0x0400040A"] = DataItem("0x0400040A", "电表无功常数", QStringLiteral("1600"), "imp/kvarh", "AA,AA", true, 0);
    m_dataItems["0x0400040B"] = DataItem("0x0400040B", "电表型号(ASCII码)", QStringLiteral("DTZ0000001"), "-", repeatedByteFormat(10), true, 0);
    m_dataItems["0x0400040C"] = DataItem("0x0400040C", "生产日期(ASCII码)", QStringLiteral("2026042900"), "-", repeatedByteFormat(10), true, 0);
    m_dataItems["0x0400040D"] = DataItem("0x0400040D", "协议版本号(ASCII码)", QStringLiteral("6452007000000000"), "-", repeatedByteFormat(16), true, 0);
    m_dataItems["0x0400040E"] = DataItem("0x0400040E", "客户编号", QStringLiteral("000001234567"), "-", "AA,AA,AA,AA,AA,AA", true, 0);

    QVariantList statusWordBlock;
    for (int i = 1; i <= 7; ++i)
    {
        const QString diCode = QStringLiteral("0x040005%1").arg(i, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        const int value = (i == 1) ? 1 : 0;
        m_dataItems[diCode] = DataItem(diCode, QStringLiteral("电表运行状态字%1").arg(i), value, "-", "AA,AA", true, 0);
        statusWordBlock.append(value);
    }
    m_dataItems["0x040005FF"] = DataItem("0x040005FF", "电表运行状态字数据块", statusWordBlock, "-", repeatedFormatParts("AA,AA", 7), true, 0);

    m_dataItems["0x04000601"] = DataItem("0x04000601", "有功组合方式特征字", 1, "-", "AA", true, 0);
    m_dataItems["0x04000602"] = DataItem("0x04000602", "无功组合方式1特征字", 1, "-", "AA", true, 0);
    m_dataItems["0x04000603"] = DataItem("0x04000603", "无功组合方式2特征字", 1, "-", "AA", true, 0);

    m_dataItems["0x04000701"] = DataItem("0x04000701", "调制型红外光口通信速率特征字", 8, "-", "AA", true, 0);
    m_dataItems["0x04000702"] = DataItem("0x04000702", "接触式红外光口通信速率特征字", 8, "-", "AA", true, 0);
    m_dataItems["0x04000703"] = DataItem("0x04000703", "通信口1通信速率特征字", 8, "-", "AA", true, 0);
    m_dataItems["0x04000704"] = DataItem("0x04000704", "通信口2通信速率特征字", 8, "-", "AA", true, 0);
    m_dataItems["0x04000705"] = DataItem("0x04000705", "通信口3通信速率特征字", 8, "-", "AA", true, 0);

    m_dataItems["0x04000801"] = DataItem("0x04000801", "周休日特征字", 96, "-", "AA", true, 0);
    m_dataItems["0x04000802"] = DataItem("0x04000802", "周休日采用的日时段表号", 1, "-", "AA", true, 0);

    m_dataItems["0x04000901"] = DataItem("0x04000901", "负荷记录模式字", 1, "-", "AA", true, 0);
    m_dataItems["0x04000902"] = DataItem("0x04000902", "定时冻结数据模式字", 1, "-", "AA", true, 0);
    m_dataItems["0x04000903"] = DataItem("0x04000903", "瞬时冻结数据模式字", 1, "-", "AA", true, 0);
    m_dataItems["0x04000904"] = DataItem("0x04000904", "约定冻结数据模式字", 1, "-", "AA", true, 0);
    m_dataItems["0x04000905"] = DataItem("0x04000905", "整点冻结数据模式字", 1, "-", "AA", true, 0);
    m_dataItems["0x04000906"] = DataItem("0x04000906", "日冻结数据模式字", 1, "-", "AA", true, 0);

    m_dataItems["0x04000A01"] = DataItem("0x04000A01", "负荷记录起始时间", QStringLiteral("01010000"), "-", "AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000A02"] = DataItem("0x04000A02", "第1类负荷记录间隔时间", 15, "分钟", "AA,AA", true, 0);
    m_dataItems["0x04000A03"] = DataItem("0x04000A03", "第2类负荷记录间隔时间", 30, "分钟", "AA,AA", true, 0);
    m_dataItems["0x04000A04"] = DataItem("0x04000A04", "第3类负荷记录间隔时间", 60, "分钟", "AA,AA", true, 0);
    m_dataItems["0x04000A05"] = DataItem("0x04000A05", "第4类负荷记录间隔时间", 60, "分钟", "AA,AA", true, 0);
    m_dataItems["0x04000A06"] = DataItem("0x04000A06", "第5类负荷记录间隔时间", 60, "分钟", "AA,AA", true, 0);
    m_dataItems["0x04000A07"] = DataItem("0x04000A07", "第6类负荷记录间隔时间", 60, "分钟", "AA,AA", true, 0);

    m_dataItems["0x04000B01"] = DataItem("0x04000B01", "每月第1结算日", QStringLiteral("0100"), "-", "AA,AA", true, 0);
    m_dataItems["0x04000B02"] = DataItem("0x04000B02", "每月第2结算日", QStringLiteral("1500"), "-", "AA,AA", true, 0);
    m_dataItems["0x04000B03"] = DataItem("0x04000B03", "每月第3结算日", QStringLiteral("9999"), "-", "AA,AA", true, 0);

    m_dataItems["0x04001201"] = DataItem("0x04001201", "整点冻结起始时间", compactDateTimeToMinutes(QDateTime(QDate(now.date().year(), 1, 1), QTime(0, 0))), "-", "AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04001202"] = DataItem("0x04001202", "整点冻结时间间隔", 60, "分钟", "AA", true, 0);
    m_dataItems["0x04001203"] = DataItem("0x04001203", "日冻结时间", QStringLiteral("0000"), "-", "AA,AA", true, 0);
    m_dataItems["0x04001301"] = DataItem("0x04001301", "无线通信在线及信号强弱指示", 132, "-", "AA", true, 0);
    m_dataItems["0x04001401"] = DataItem("0x04001401", "跳闸延时时间", 15, "分钟", "AA,AA", true, 0);

    QVariantList firstSetYearZones;
    QVariantList secondSetYearZones;
    for (int zoneIndex = 0; zoneIndex < kMaxYearZoneCount; ++zoneIndex)
    {
        firstSetYearZones.append(buildYearZoneEntryValue(11, 0, zoneIndex));
        secondSetYearZones.append(buildYearZoneEntryValue(37, 1, zoneIndex));
    }
    m_dataItems["0x04010000"] = DataItem("0x04010000", "第一套时区表数据", firstSetYearZones, "-", repeatedFormatParts("AA,AA,AA", kMaxYearZoneCount), true, 0);
    m_dataItems["0x04020000"] = DataItem("0x04020000", "第二套时区表数据", secondSetYearZones, "-", repeatedFormatParts("AA,AA,AA", kMaxYearZoneCount), true, 0);

    for (int tableIndex = 0; tableIndex < kMaxDayScheduleTableCount; ++tableIndex)
    {
        const QString firstDiCode = QStringLiteral("0x040100%1").arg(tableIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        const QString secondDiCode = QStringLiteral("0x040200%1").arg(tableIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        m_dataItems[firstDiCode] = DataItem(firstDiCode,
                                            QStringLiteral("第一套第%1日时段表数据").arg(tableIndex + 1),
                                            QVariantList(),
                                            "-",
                                            repeatedFormatParts("AA,AA,AA", kMaxDayScheduleCount),
                                            true,
                                            0);
        m_dataItems[secondDiCode] = DataItem(secondDiCode,
                                             QStringLiteral("第二套第%1日时段表数据").arg(tableIndex + 1),
                                             QVariantList(),
                                             "-",
                                             repeatedFormatParts("AA,AA,AA", kMaxDayScheduleCount),
                                             true,
                                             0);
        const QString firstValue = buildDayScheduleTableValue(19, 0, tableIndex);
        const QString secondValue = buildDayScheduleTableValue(43, 1, tableIndex);
        QVariantList firstTableEntries;
        QVariantList secondTableEntries;
        for (int slotIndex = 0; slotIndex < kMaxDayScheduleCount; ++slotIndex)
        {
            firstTableEntries.append(firstValue.mid(slotIndex * 6, 6));
            secondTableEntries.append(secondValue.mid(slotIndex * 6, 6));
        }
        m_dataItems[firstDiCode].value = firstTableEntries;
        m_dataItems[secondDiCode].value = secondTableEntries;
    }

    for (int holidayIndex = 0; holidayIndex < configuredHolidayCount; ++holidayIndex)
    {
        const QString diCode = QStringLiteral("0x0403%1").arg(holidayIndex + 1, 4, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        const QDate holidayDate(now.date().year(), (holidayIndex % 12) + 1, qMin(holidayIndex + 1, 28));
        const int scheduleTableIndex = (holidayIndex % configuredDayScheduleTableCount) + 1;
        m_dataItems[diCode] = DataItem(diCode,
                                       QStringLiteral("第%1公共假日日期及日时段表号").arg(holidayIndex + 1),
                                       buildHolidayEntryValue(holidayDate, scheduleTableIndex),
                                       "-",
                                       "AA,AA,AA,AA",
                                       true,
                                       0);
    }

    const QStringList autoScreenTargets = {
        QStringLiteral("0x00000000"),
        QStringLiteral("0x00010000"),
        QStringLiteral("0x02010100"),
        QStringLiteral("0x02020100"),
        QStringLiteral("0x02030000"),
        QStringLiteral("0x02060000")};
    for (int screenIndex = 0; screenIndex < autoScreenTargets.size(); ++screenIndex)
    {
        const QString diCode = QStringLiteral("0x040401%1").arg(screenIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        m_dataItems[diCode] = DataItem(diCode,
                                       QStringLiteral("自动循环显示第%1屏显示数据项(是否分屏)").arg(screenIndex + 1),
                                       buildDisplayScreenEntryValue(autoScreenTargets.at(screenIndex), screenIndex % 2),
                                       "-",
                                       "AA,AA,AA,AA,AA",
                                       true,
                                       0);
    }

    const QStringList keyScreenTargets = {
        QStringLiteral("0x00020000"),
        QStringLiteral("0x02010200"),
        QStringLiteral("0x02020200"),
        QStringLiteral("0x02040000")};
    for (int screenIndex = 0; screenIndex < keyScreenTargets.size(); ++screenIndex)
    {
        const QString diCode = QStringLiteral("0x040402%1").arg(screenIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        m_dataItems[diCode] = DataItem(diCode,
                                       QStringLiteral("按键循环显示第%1屏显示数据项(是否分屏)").arg(screenIndex + 1),
                                       buildDisplayScreenEntryValue(keyScreenTargets.at(screenIndex), screenIndex % 2),
                                       "-",
                                       "AA,AA,AA,AA,AA",
                                       true,
                                       0);
    }

    const QList<double> firstRatePrices = {0.5688, 0.6288, 0.6888, 0.7488, 0.8088};
    const QList<double> secondRatePrices = {0.5888, 0.6488, 0.7088, 0.7688, 0.8288};
    for (int rateIndex = 0; rateIndex < configuredRateCount; ++rateIndex)
    {
        const QString firstDiCode = QStringLiteral("0x040501%1").arg(rateIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        const QString secondDiCode = QStringLiteral("0x040502%1").arg(rateIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        m_dataItems[firstDiCode] = DataItem(firstDiCode, QStringLiteral("第一套费率电价%1").arg(rateIndex + 1), firstRatePrices.at(rateIndex), "元", "AA,AA,.,AA,AA", true, 4);
        m_dataItems[secondDiCode] = DataItem(secondDiCode, QStringLiteral("第二套费率电价%1").arg(rateIndex + 1), secondRatePrices.at(rateIndex), "元", "AA,AA,.,AA,AA", true, 4);
    }

    const QList<double> firstStepValues = {1800.0, 3200.0, 5000.0};
    const QList<double> firstStepPrices = {0.5688, 0.6188, 0.6888};
    const QList<double> secondStepValues = {2000.0, 3500.0, 5200.0};
    const QList<double> secondStepPrices = {0.5888, 0.6388, 0.7088};
    for (int stepIndex = 0; stepIndex < configuredStepCount; ++stepIndex)
    {
        const QString firstValueDiCode = QStringLiteral("0x040600%1").arg(stepIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        const QString firstPriceDiCode = QStringLiteral("0x040601%1").arg(stepIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        const QString secondValueDiCode = QStringLiteral("0x040602%1").arg(stepIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        const QString secondPriceDiCode = QStringLiteral("0x040603%1").arg(stepIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        m_dataItems[firstValueDiCode] = DataItem(firstValueDiCode, QStringLiteral("第一套第%1阶梯值").arg(stepIndex + 1), firstStepValues.at(stepIndex), "kWh", "AA,AA,AA,.,AA", true, 2);
        m_dataItems[firstPriceDiCode] = DataItem(firstPriceDiCode, QStringLiteral("第一套阶梯电价%1").arg(stepIndex + 1), firstStepPrices.at(stepIndex), "元", "AA,AA,.,AA,AA", true, 4);
        m_dataItems[secondValueDiCode] = DataItem(secondValueDiCode, QStringLiteral("第二套第%1阶梯值").arg(stepIndex + 1), secondStepValues.at(stepIndex), "kWh", "AA,AA,AA,.,AA", true, 2);
        m_dataItems[secondPriceDiCode] = DataItem(secondPriceDiCode, QStringLiteral("第二套阶梯电价%1").arg(stepIndex + 1), secondStepPrices.at(stepIndex), "元", "AA,AA,.,AA,AA", true, 4);
    }

    m_dataItems["0x04090101"] = DataItem("0x04090101", "失压事件电压触发上限", 60.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090102"] = DataItem("0x04090102", "失压事件电压恢复下限", 180.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090103"] = DataItem("0x04090103", "失压事件电流触发下限", 1.5, "A", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090104"] = DataItem("0x04090104", "失压事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090201"] = DataItem("0x04090201", "欠压事件电压触发上限", 180.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090202"] = DataItem("0x04090202", "欠压事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090301"] = DataItem("0x04090301", "过压事件电压触发下限", 260.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090302"] = DataItem("0x04090302", "过压事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090401"] = DataItem("0x04090401", "断相事件电压触发上限", 60.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090402"] = DataItem("0x04090402", "断相事件电流触发上限", 1.0, "A", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090403"] = DataItem("0x04090403", "断相事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090501"] = DataItem("0x04090501", "电压不平衡率限值", 20.0, "%", "AA,.,AA", true, 2);
    m_dataItems["0x04090502"] = DataItem("0x04090502", "电压不平衡率判定延时时间", 30, "秒", "AA", true, 0);
    m_dataItems["0x04090601"] = DataItem("0x04090601", "电流不平衡率限值", 30.0, "%", "AA,.,AA", true, 2);
    m_dataItems["0x04090602"] = DataItem("0x04090602", "电流不平衡率判定延时时间", 30, "秒", "AA", true, 0);
    m_dataItems["0x04090701"] = DataItem("0x04090701", "失流事件电压触发下限", 180.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090702"] = DataItem("0x04090702", "失流事件电流触发上限", 0.8, "A", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090703"] = DataItem("0x04090703", "失流事件电流触发下限", 0.1, "A", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090704"] = DataItem("0x04090704", "失流事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090801"] = DataItem("0x04090801", "过流事件电流触发下限", 80.0, "A", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090802"] = DataItem("0x04090802", "过流事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090901"] = DataItem("0x04090901", "断流事件电压触发下限", 180.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090902"] = DataItem("0x04090902", "断流事件电流触发上限", 0.5, "A", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090903"] = DataItem("0x04090903", "断流事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090A01"] = DataItem("0x04090A01", "潮流反向事件有功功率触发下限", 1.5, "kW", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090A02"] = DataItem("0x04090A02", "潮流反向事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090B01"] = DataItem("0x04090B01", "过载事件有功功率触发下限", 15.0, "kW", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090B02"] = DataItem("0x04090B02", "过载事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090C01"] = DataItem("0x04090C01", "电压考核上限", 242.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090C02"] = DataItem("0x04090C02", "电压考核下限", 198.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090D01"] = DataItem("0x04090D01", "有功需量超限事件需量触发下限", 10.0, "kW", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090D02"] = DataItem("0x04090D02", "无功需量超限事件需量触发下限", 6.0, "kVar", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090D03"] = DataItem("0x04090D03", "需量超限事件判定延时时间", 60, "秒", "AA", true, 0);
    m_dataItems["0x04090E01"] = DataItem("0x04090E01", "总功率因数超下限阀值", 0.85, "-", "xx,AA", true, 3);
    m_dataItems["0x04090E02"] = DataItem("0x04090E02", "总功率因数超下限判定延时时间", 60, "秒", "AA", true, 0);
    m_dataItems["0x04090F01"] = DataItem("0x04090F01", "电流严重不平衡限值", 40.0, "%", "AA,.,AA", true, 2);
    m_dataItems["0x04090F02"] = DataItem("0x04090F02", "电流严重不平衡触发延时时间", 60, "秒", "AA", true, 0);
}

void MeterModel::initializeFreezeData()
{
    // 冻结数据 (0x05xxxxxx)
    const QDateTime now = QDateTime::currentDateTime();
    for (const FreezeCategoryDefinition &category : latestFreezeCategoryDefinitions())
    {
        const QDateTime freezeTime = defaultLatestFreezeSnapshotTime(category, now);
        initializeLatestFreezeEnergyCategory(m_dataItems, category, freezeTime);
    }

    initializeLatestHourlyFreezeCategory(m_dataItems, now.addSecs(-60 * 60));
    synchronizeFreezeTotals(m_dataItems);
    refreshLatestFreezeCompositeBlocks(m_dataItems);
}

void MeterModel::initializeLoadRecordData()
{
    // 负荷记录 (0x06xxxxxx)
    m_dataItems["0x06010101"] = DataItem("0x06010101", "预约远程跳闸控制(数值+单位代码)", QStringLiteral("3002"), "-", "AA,AA", true, 0);
    m_dataItems["0x06020101"] = DataItem("0x06020101", "预约远程合闸控制(数值+单位代码)", QStringLiteral("1502"), "-", "AA,AA", true, 0);
    m_dataItems["0x06030101"] = DataItem("0x06030101", "预约模拟试跳控制(数值+单位代码)", QStringLiteral("0502"), "-", "AA,AA", true, 0);
}

QList<QString> MeterModel::getEnergyDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (isEnergyDataCode(it.key()) && !isHistoricalEnergyCode(it.key()) && isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getSettlementDayEnergyDataCodes(int settlementDay) const
{
    if (!isValidSettlementDayIndex(settlementDay) || settlementDay == 0)
    {
        return {};
    }

    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (!isEnergyDataCode(it.key()) || !isDataItemSupportedUnlocked(it.key()))
        {
            continue;
        }

        bool ok = false;
        const int prefixValue = prefixedCodeValue(it.key(), &ok);
        if (ok && settlementDayFromPrefixValue(prefixValue) == settlementDay)
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getDemandDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (isDemandDataCode(it.key()) && isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getSettlementDayDemandDataCodes(int settlementDay) const
{
    if (!isValidSettlementDayIndex(settlementDay))
    {
        return {};
    }

    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (!isDemandDataCode(it.key()) || !isDataItemSupportedUnlocked(it.key()))
        {
            continue;
        }

        bool ok = false;
        const int prefixValue = prefixedCodeValue(it.key(), &ok);
        if (ok && settlementDayFromPrefixValue(prefixValue) == settlementDay)
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getMaxDemandDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (isDemandDataCode(it.key()) && isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getVariableDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        bool ok = false;
        const quint32 codeValue = it.key().toUInt(&ok, 16);
        if (ok &&
            codeValue >= 0x02000000 &&
            codeValue < 0x02900000 &&
            isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getEventDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (it.key().startsWith("0x03") && isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getParameterDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (it.key().startsWith("0x04") && isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getFreezeDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (it.key().startsWith("0x05") && isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getFreezeDataCodes(const QString &freezeCategory) const
{
    QMutexLocker locker(&m_mutex);

    QString prefix;
    if (freezeCategory == QStringLiteral("定时冻结"))
    {
        prefix = QStringLiteral("0x0500");
    }
    else if (freezeCategory == QStringLiteral("瞬时冻结"))
    {
        prefix = QStringLiteral("0x0501");
    }
    else if (freezeCategory == QStringLiteral("时区切换冻结"))
    {
        prefix = QStringLiteral("0x0502");
    }
    else if (freezeCategory == QStringLiteral("日时段切换冻结"))
    {
        prefix = QStringLiteral("0x0503");
    }
    else if (freezeCategory == QStringLiteral("整点冻结"))
    {
        prefix = QStringLiteral("0x0504");
    }
    else if (freezeCategory == QStringLiteral("分时费率切换冻结"))
    {
        prefix = QStringLiteral("0x0505");
    }
    else if (freezeCategory == QStringLiteral("日冻结"))
    {
        prefix = QStringLiteral("0x0506");
    }
    else if (freezeCategory == QStringLiteral("阶梯切换冻结"))
    {
        prefix = QStringLiteral("0x0507");
    }

    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (!it.key().startsWith(QStringLiteral("0x05")) || !isDataItemSupportedUnlocked(it.key()))
        {
            continue;
        }

        if (!prefix.isEmpty() && !it.key().startsWith(prefix))
        {
            continue;
        }

        codes.append(it.key());
    }

    return codes;
}

QList<QString> MeterModel::getLoadRecordDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (it.key().startsWith("0x06") && isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getStatusDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if ((it.key().startsWith("0x05") || it.key().startsWith("0x06")) && isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QString MeterModel::getDataName(const QString &diCode) const
{
    QMutexLocker locker(&m_mutex);
    if (isDataItemSupportedUnlocked(diCode))
    {
        return m_dataItems[diCode].description;
    }
    return QString();
}

QString MeterModel::getDataUnit(const QString &diCode) const
{
    QMutexLocker locker(&m_mutex);
    if (isDataItemSupportedUnlocked(diCode))
    {
        return m_dataItems[diCode].unit;
    }
    return QString();
}
