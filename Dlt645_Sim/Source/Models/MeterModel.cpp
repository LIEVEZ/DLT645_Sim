#include "MeterModel.h"
#include <QRandomGenerator>
#include <QDateTime>
#include <QDebug>
#include <QSet>
#include <cmath>

namespace
{
    constexpr int kDefaultRateCount = 5;
    constexpr int kMaxRateCount = 63;
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

    const QStringList &currentEnergyPrefixes()
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

    const QStringList &dailyFreezePrefixes()
    {
        static const QStringList prefixes = {
            QStringLiteral("0x050601"),
            QStringLiteral("0x050602"),
            QStringLiteral("0x050603"),
            QStringLiteral("0x050604")};
        return prefixes;
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

    QStringList dailyFreezeRateSuffixes(int rateCount)
    {
        QStringList suffixes;
        suffixes.reserve(rateCount);
        for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
        {
            suffixes.append(upperHexByte(rateIndex + 1));
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

    bool isDailyFreezeRateCode(const QString &diCode, int *rateIndex = nullptr)
    {
        if (diCode.size() != 10)
        {
            return false;
        }

        const QString prefix = diCode.left(8);
        if (!dailyFreezePrefixes().contains(prefix))
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
            isDailyFreezeRateCode(diCode, &rateIndex))
        {
            return rateIndex <= supportedRateCount;
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

    QStringList currentDailyFreezeBlockCodes(const QMap<QString, DataItem> &items, const QString &diPrefix)
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

    void ensureDailyFreezeRateItems(QMap<QString, DataItem> &items)
    {
        for (const QString &prefix : dailyFreezePrefixes())
        {
            const QString templateCode = prefix + QStringLiteral("02");
            if (!items.contains(templateCode))
            {
                continue;
            }

            const DataItem templateItem = items.value(templateCode);
            for (int rateIndex = 1; rateIndex <= kMaxRateCount; ++rateIndex)
            {
                const QString code = prefix + upperHexByte(rateIndex + 1);
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

        for (const QString &prefix : dailyFreezePrefixes())
        {
            const QString blockCode = prefix + QStringLiteral("01");
            const QString templateCode = prefix + QStringLiteral("02");
            if (items.contains(blockCode) && items.contains(templateCode))
            {
                items[blockCode].format = repeatedFormat(items[templateCode].format, rateCount + 1);
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

        static const QStringList singlePhaseUnsupportedPrefixes = {
            QStringLiteral("0x038202"),
            QStringLiteral("0x038203"),
            QStringLiteral("0x038205"),
            QStringLiteral("0x038206"),
            QStringLiteral("0x038302"),
            QStringLiteral("0x038303"),
            QStringLiteral("0x038305"),
            QStringLiteral("0x038306")};
        for (const QString &prefix : singlePhaseUnsupportedPrefixes)
        {
            if (diCode.startsWith(prefix))
            {
                return true;
            }
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

    void synchronizeDailyFreezeTotals(QMap<QString, DataItem> &items)
    {
        const int rateCount = currentRateCount(items);
        synchronizeGroupTotal(items, "0x05060100", prefixedCodes("0x050601", dailyFreezeRateSuffixes(rateCount)));
        synchronizeGroupTotal(items, "0x05060200", prefixedCodes("0x050602", dailyFreezeRateSuffixes(rateCount)));
        synchronizeGroupTotal(items, "0x05060300", prefixedCodes("0x050603", dailyFreezeRateSuffixes(rateCount)));
        synchronizeGroupTotal(items, "0x05060400", prefixedCodes("0x050604", dailyFreezeRateSuffixes(rateCount)));
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

            if (item.diCode == "0x04000000" || item.diCode == "0x04000401")
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

            if (item.diCode.startsWith("0x0382") || item.diCode.startsWith("0x0383"))
            {
                const QDateTime eventTime = QDateTime::currentDateTime().addSecs(-rng->bounded(0, 90 * 24 * 60 * 60));
                bool subgroupOk = false;
                const int subgroup = item.diCode.mid(6, 2).toInt(&subgroupOk, 16);
                const bool isMinRecord = item.diCode.startsWith("0x0383");

                if (!subgroupOk || subgroup == 0)
                {
                    const int phaseCode = (meterPhaseType == MeterPhaseType::SinglePhase) ? 1 : rng->bounded(1, 4);
                    int currentValue = rng->bounded(50, 1201);
                    if (isMinRecord)
                    {
                        currentValue = rng->bounded(1, 201);
                    }

                    item.value = QVariantList{compactDateTimeToSeconds(eventTime), currentValue, phaseCode};
                    continue;
                }

                if (subgroup >= 0x01 && subgroup <= 0x03)
                {
                    const double voltageValue = isMinRecord
                                                    ? rng->bounded(1600, 2201) / 10.0
                                                    : rng->bounded(2200, 2651) / 10.0;
                    item.value = QVariantList{compactDateTimeToSeconds(eventTime), voltageValue};
                    continue;
                }

                const double currentValue = isMinRecord
                                                ? rng->bounded(5, 301) / 10.0
                                                : rng->bounded(150, 1201) / 10.0;
                item.value = QVariantList{compactDateTimeToSeconds(eventTime), currentValue};
                continue;
            }

            if (item.diCode.startsWith("0x039000") ||
                item.diCode.startsWith("0x039001"))
            {
                const QDateTime eventTime = QDateTime::currentDateTime().addSecs(-rng->bounded(0, 90 * 24 * 60 * 60));
                const int phaseCode = (meterPhaseType == MeterPhaseType::SinglePhase) ? 1 : rng->bounded(1, 4);
                int currentValue = rng->bounded(10, 801);

                item.value = QVariantList{compactDateTimeToSeconds(eventTime), currentValue, phaseCode};
                continue;
            }

            if (item.diCode.startsWith("0x0388"))
            {
                const QDateTime endTime = QDateTime::currentDateTime().addSecs(-rng->bounded(0, 90 * 24 * 60 * 60));
                const QDateTime startTime = endTime.addSecs(-rng->bounded(60, 48 * 60 * 60));
                const int phaseCode = (meterPhaseType == MeterPhaseType::SinglePhase) ? 1 : rng->bounded(1, 4);
                const int currentValue = rng->bounded(100, 1501);
                item.value = QVariantList{compactDateTimeToSeconds(endTime),
                                          compactDateTimeToSeconds(startTime),
                                          currentValue,
                                          phaseCode};
                continue;
            }

            if (item.diCode.startsWith("0x038F"))
            {
                const QDateTime startTime = QDateTime::currentDateTime().addSecs(-rng->bounded(0, 90 * 24 * 60 * 60));
                const QDateTime endTime = startTime.addSecs(rng->bounded(60, 48 * 60 * 60));
                const int phaseCode = (meterPhaseType == MeterPhaseType::SinglePhase) ? 1 : rng->bounded(1, 4);
                const int currentValue = rng->bounded(80, 1201);
                item.value = QVariantList{currentValue,
                                          phaseCode,
                                          compactDateTimeToSeconds(startTime),
                                          compactDateTimeToSeconds(endTime)};
                continue;
            }

            if (item.diCode.startsWith("0x038D"))
            {
                const QDateTime eventTime = QDateTime::currentDateTime().addSecs(-rng->bounded(0, 90 * 24 * 60 * 60));
                const bool singlePhase = (meterPhaseType == MeterPhaseType::SinglePhase);
                const double currentA = rng->bounded(0, 8001) / 10.0;
                const double currentB = singlePhase ? 0.0 : rng->bounded(0, 8001) / 10.0;
                const double currentC = singlePhase ? 0.0 : rng->bounded(0, 8001) / 10.0;
                const int phaseCode = singlePhase ? 1 : rng->bounded(1, 4);
                const int selfCheckModeCode = rng->bounded(1, 256);
                const int selfCheckResultCode = rng->bounded(1, 5);
                item.value = QVariantList{currentC,
                                          currentB,
                                          currentA,
                                          phaseCode,
                                          selfCheckModeCode,
                                          compactDateTimeToSeconds(eventTime),
                                          selfCheckResultCode};
                continue;
            }

            if (item.diCode.startsWith("0x038E"))
            {
                const QDateTime eventTime = QDateTime::currentDateTime().addSecs(-rng->bounded(0, 90 * 24 * 60 * 60));
                const bool singlePhase = (meterPhaseType == MeterPhaseType::SinglePhase);
                const double currentA = rng->bounded(0, 8001) / 10.0;
                const double currentB = singlePhase ? 0.0 : rng->bounded(0, 8001) / 10.0;
                const double currentC = singlePhase ? 0.0 : rng->bounded(0, 8001) / 10.0;
                const double voltageA = 220.0 + (rng->bounded(-100, 101) / 10.0);
                const double voltageB = singlePhase ? 0.0 : 220.0 + (rng->bounded(-100, 101) / 10.0);
                const double voltageC = singlePhase ? 0.0 : 220.0 + (rng->bounded(-100, 101) / 10.0);
                const int residualCurrent = rng->bounded(50, 1501);
                const int phaseCode = singlePhase ? 1 : rng->bounded(1, 4);
                const int faultReasonCode = rng->bounded(1, 256);
                item.value = QVariantList{currentC,
                                          currentB,
                                          currentA,
                                          voltageC,
                                          voltageB,
                                          voltageA,
                                          residualCurrent,
                                          compactDateTimeToSeconds(eventTime),
                                          phaseCode,
                                          faultReasonCode};
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

            if (item.diCode.startsWith("0x0202") || item.diCode == "0x02080001")
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
            else if (item.diCode == "0x02900000")
            {
                item.value = rng->bounded(1, 4);
                variableValueHandled = true;
            }
            else if (item.diCode == "0x02900100")
            {
                item.value = rng->bounded(0, 3001);
                variableValueHandled = true;
            }
            else if (item.diCode == "0x02900200")
            {
                item.value = rng->bounded(30, 1001);
                variableValueHandled = true;
            }
            else if (item.diCode == "0x02910100")
            {
                item.value = rng->bounded(30, 1001);
                variableValueHandled = true;
            }
            else if (item.diCode == "0x02910200")
            {
                item.value = rng->bounded(50, 2001);
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
            }
            else
            {
                item.value = originalValue;
            }
        }

        synchronizeDemandTotals(items);
        synchronizeInstantaneousPowerTotals(items);
        synchronizeCurrentEnergyTotals(items);
        synchronizeDailyFreezeTotals(items);
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
            if (m_dataItems.contains("0x04000000"))
            {
                m_dataItems["0x04000000"].value = address;
            }
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
            else if (diCode == "0x0290FF00")
            {
                appendValuesForCodes(m_dataItems,
                                     {QStringLiteral("0x02900200"),
                                      QStringLiteral("0x02900100"),
                                      QStringLiteral("0x02900000")},
                                     values);
            }
            else if (diCode == "0x0291FF00")
            {
                appendValuesForCodes(m_dataItems,
                                     {QStringLiteral("0x02910200"),
                                      QStringLiteral("0x02910100")},
                                     values);
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
            else if (diCode == "0x038100FF")
            {
                appendValuesForCodes(m_dataItems,
                                     {QStringLiteral("0x03810007"),
                                      QStringLiteral("0x03810006"),
                                      QStringLiteral("0x03810005"),
                                      QStringLiteral("0x03810004"),
                                      QStringLiteral("0x03810003"),
                                      QStringLiteral("0x03810002"),
                                      QStringLiteral("0x03810001"),
                                      QStringLiteral("0x03810000")},
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
            else if (diCode == "0x05060101" || diCode == "0x05060201" || diCode == "0x05060301" || diCode == "0x05060401")
            {
                const QString basePrefix = diCode.left(8);
                appendValuesForCodes(m_dataItems, currentDailyFreezeBlockCodes(m_dataItems, basePrefix), values);
            }

            if (!values.isEmpty())
            {
                qDebug() << "MeterModel::getDataValue: " << diCode << " " << values;
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
            if (diCode == "0x04000204")
            {
                emittedValue = normalizedRateCount(value.toInt());
            }

            m_dataItems[diCode].value = emittedValue;
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
            if (diCode.startsWith("0x050601") || diCode.startsWith("0x050602") || diCode.startsWith("0x050603") || diCode.startsWith("0x050604"))
            {
                synchronizeDailyFreezeTotals(m_dataItems);
            }
            if (diCode == "0x04000204")
            {
                refreshRateDependentBlockFormats(m_dataItems);
                synchronizeDemandTotals(m_dataItems);
                synchronizeInstantaneousPowerTotals(m_dataItems);
                synchronizeCurrentEnergyTotals(m_dataItems);
                synchronizeDailyFreezeTotals(m_dataItems);
                requiresBatchRefresh = true;
            }
            updated = true;
        }
    }

    if (updated)
    {
        emit dataChanged(diCode, emittedValue);
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
                m_dataItems[it.key()].value = it.value();
            }
        }

        if (m_dataItems.contains("0x04000000"))
        {
            m_dataItems["0x04000000"].value = m_meterAddress;
        }
        if (m_dataItems.contains("0x04000401"))
        {
            m_dataItems["0x04000401"].value = m_meterAddress;
        }

        refreshRateDependentBlockFormats(m_dataItems);
        synchronizeDemandTotals(m_dataItems);
        synchronizeInstantaneousPowerTotals(m_dataItems);
        synchronizeCurrentEnergyTotals(m_dataItems);
        synchronizeDailyFreezeTotals(m_dataItems);
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
    initializeFreezeData();
    initializeLoadRecordData();
    ensureCurrentEnergyRateItems(m_dataItems);
    ensureDemandRateItems(m_dataItems);
    ensureDailyFreezeRateItems(m_dataItems);
    refreshRateDependentBlockFormats(m_dataItems);
    synchronizeDemandTotals(m_dataItems);
    synchronizeInstantaneousPowerTotals(m_dataItems);
    synchronizeCurrentEnergyTotals(m_dataItems);
    synchronizeDailyFreezeTotals(m_dataItems);
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
    m_dataItems["0x0201FF00"] = DataItem("0x02010400", "电压数据块", QVariantList(), "-", "AA,xx:AA,xx:AA,xx", false, 1);

    // 电流
    m_dataItems["0x02020100"] = DataItem("0x02020100", "A相电流", 1.01, "A", "AA,xx,AA", false, 3);
    m_dataItems["0x02020200"] = DataItem("0x02020200", "B相电流", 1.023, "A", "AA,xx,AA", false, 3);
    m_dataItems["0x02020300"] = DataItem("0x02020300", "C相电流", 1.1, "A", "AA,xx,AA", false, 3);
    m_dataItems["0x0202FF00"] = DataItem("0x0202FF00", "电流数据块", QVariantList(), "-", "AA,xx,AA:AA,xx,AA:AA,xx,AA", false, 3);

    // 功率
    m_dataItems["0x02030000"] = DataItem("0x02030000", "瞬时总有功功率", 0.0, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02030100"] = DataItem("0x02030100", "瞬时A相有功功率", 0.0, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02030200"] = DataItem("0x02030200", "瞬时B相有功功率", 0.0, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02030300"] = DataItem("0x02030300", "瞬时C相有功功率", 0.0, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x0203FF00"] = DataItem("0x0203FF00", "瞬时有功功率数据块", QVariantList(), "-", "AA,.,AA,AA:AA,.,AA,AA:AA,.,AA,AA:AA,.,AA,AA", false, 4);

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
    m_dataItems["0x0207FF00"] = DataItem("0x0207FF00", "相角数据块", QVariantList(), "-", "AA,xx:AA,xx:AA,xx:AA,xx", false, 1);

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
    m_dataItems["0x02080001"] = DataItem("0x02080001", "零相电流", 0.0, "A", "AA,xx,AA", false, 3);
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
    m_dataItems["0x02900000"] = DataItem("0x02900000", "当前剩余电流最大相代码(1=A,2=B,3=C)", 1, "-", "AA", false, 0);
    m_dataItems["0x02900100"] = DataItem("0x02900100", "当前剩余电流", 120, "mA", "AA,AA", false, 0);
    m_dataItems["0x02900200"] = DataItem("0x02900200", "当前剩余电流门限值", 300, "mA", "AA,AA", false, 0);
    m_dataItems["0x0290FF00"] = DataItem("0x0290FF00", "当前剩余电流数据块", QVariantList(), "-", "AA,AA:AA,AA:AA", false, 0);
    m_dataItems["0x02910100"] = DataItem("0x02910100", "当前额定剩余电流动作值", 300, "mA", "AA,AA", false, 0);
    m_dataItems["0x02910200"] = DataItem("0x02910200", "当前额定极限不驱动时间", 200, "ms", "AA,AA", false, 0);
    m_dataItems["0x0291FF00"] = DataItem("0x0291FF00", "当前额定剩余电流参数块", QVariantList(), "-", "AA,AA:AA,AA", false, 0);
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
    m_dataItems["0x03300100"] = DataItem("0x03300100", "数据清零总次数", 0, "次", "AA,AA,AA", false, 0);
    m_dataItems["0x03810000"] = DataItem("0x03810000", "总跳闸次数", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810001"] = DataItem("0x03810001", "闭锁性跳闸次数", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810002"] = DataItem("0x03810002", "剩余电流保护跳闸次数", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810003"] = DataItem("0x03810003", "电流保护跳闸次数", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810004"] = DataItem("0x03810004", "电压保护跳闸次数", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810005"] = DataItem("0x03810005", "手动闭锁跳闸次数", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810006"] = DataItem("0x03810006", "缺零保护跳闸次数", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810007"] = DataItem("0x03810007", "试验跳闸次数（定时、远程、按键）", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x038100FF"] = DataItem("0x038100FF", "跳闸次数数据块", QVariantList(), "-", "AA,AA:AA,AA:AA,AA:AA,AA:AA,AA:AA,AA:AA,AA:AA,AA", false, 0);
    m_dataItems["0x03810101"] = DataItem("0x03810101", "退出剩余电流保护次数", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810201"] = DataItem("0x03810201", "保护器运行时间总累计", 0, "分钟", "AA,AA,AA,AA", false, 0);

    for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
    {
        const QString suffix = upperHexByte(historyIndex);
        const int phaseCode = ((historyIndex - 1) % 3) + 1;
        const QStringList phaseNames = {QStringLiteral("A相"),
                                        QStringLiteral("B相"),
                                        QStringLiteral("C相")};

        const QDateTime maxEventTime = now.addSecs(-historyIndex * 18 * 60 * 60);
        m_dataItems[QStringLiteral("0x038200") + suffix] = DataItem(
            QStringLiteral("0x038200") + suffix,
            QStringLiteral("上%1次最大剩余电流记录").arg(historyIndex),
            QVariantList{compactDateTimeToSeconds(maxEventTime), 960 - historyIndex * 35, phaseCode},
            "-",
            "AA,AA,AA,AA,AA,AA:AA,AA:AA",
            false,
            0);

        const QDateTime minEventTime = now.addSecs(-(historyIndex * 18 + 6) * 60 * 60);
        m_dataItems[QStringLiteral("0x038300") + suffix] = DataItem(
            QStringLiteral("0x038300") + suffix,
            QStringLiteral("上%1次最小剩余电流记录").arg(historyIndex),
            QVariantList{compactDateTimeToSeconds(minEventTime), 20 + historyIndex * 6, phaseCode},
            "-",
            "AA,AA,AA,AA,AA,AA:AA,AA:AA",
            false,
            0);

        for (int phaseOffset = 0; phaseOffset < phaseNames.size(); ++phaseOffset)
        {
            const QString maxVoltageCode = QStringLiteral("0x0382") + upperHexByte(phaseOffset + 1) + suffix;
            const QDateTime maxVoltageTime = now.addSecs(-(historyIndex * 16 + phaseOffset) * 60 * 60);
            m_dataItems[maxVoltageCode] = DataItem(
                maxVoltageCode,
                QStringLiteral("上%1次%2最大电压记录").arg(historyIndex).arg(phaseNames[phaseOffset]),
                QVariantList{compactDateTimeToSeconds(maxVoltageTime),
                             236.0 + historyIndex * 1.4 + phaseOffset * 2.1},
                "-",
                "AA,AA,AA,AA,AA,AA:AA,xx",
                false,
                0);

            const QString minVoltageCode = QStringLiteral("0x0383") + upperHexByte(phaseOffset + 1) + suffix;
            const QDateTime minVoltageTime = now.addSecs(-(historyIndex * 16 + phaseOffset + 4) * 60 * 60);
            m_dataItems[minVoltageCode] = DataItem(
                minVoltageCode,
                QStringLiteral("上%1次%2最小电压记录").arg(historyIndex).arg(phaseNames[phaseOffset]),
                QVariantList{compactDateTimeToSeconds(minVoltageTime),
                             182.0 - historyIndex * 0.8 + phaseOffset * 1.3},
                "-",
                "AA,AA,AA,AA,AA,AA:AA,xx",
                false,
                0);

            const QString maxCurrentCode = QStringLiteral("0x0382") + upperHexByte(phaseOffset + 4) + suffix;
            const QDateTime maxCurrentTime = now.addSecs(-(historyIndex * 15 + phaseOffset + 2) * 60 * 60);
            m_dataItems[maxCurrentCode] = DataItem(
                maxCurrentCode,
                QStringLiteral("上%1次%2最大电流记录").arg(historyIndex).arg(phaseNames[phaseOffset]),
                QVariantList{compactDateTimeToSeconds(maxCurrentTime),
                             18.0 + historyIndex * 2.6 + phaseOffset * 1.8},
                "-",
                "AA,AA,AA,AA,AA,AA:AA,AA,xx",
                false,
                0);

            const QString minCurrentCode = QStringLiteral("0x0383") + upperHexByte(phaseOffset + 4) + suffix;
            const QDateTime minCurrentTime = now.addSecs(-(historyIndex * 15 + phaseOffset + 6) * 60 * 60);
            m_dataItems[minCurrentCode] = DataItem(
                minCurrentCode,
                QStringLiteral("上%1次%2最小电流记录").arg(historyIndex).arg(phaseNames[phaseOffset]),
                QVariantList{compactDateTimeToSeconds(minCurrentTime),
                             1.5 + historyIndex * 0.4 + phaseOffset * 0.3},
                "-",
                "AA,AA,AA,AA,AA,AA:AA,AA,xx",
                false,
                0);
        }

        const QDateTime overLimitEndTime = now.addSecs(-(historyIndex * 9) * 60 * 60);
        const QDateTime overLimitStartTime = overLimitEndTime.addSecs(-(historyIndex * 20 + 30) * 60);
        m_dataItems[QStringLiteral("0x038800") + suffix] = DataItem(
            QStringLiteral("0x038800") + suffix,
            QStringLiteral("上%1次剩余电流超限记录").arg(historyIndex),
            QVariantList{compactDateTimeToSeconds(overLimitEndTime),
                         compactDateTimeToSeconds(overLimitStartTime),
                         650 - historyIndex * 18,
                         phaseCode},
            "-",
            "AA,AA,AA,AA,AA,AA:AA,AA,AA,AA,AA,AA:AA,AA:AA",
            false,
            0);

        const QDateTime alarmStartTime = now.addSecs(-(historyIndex * 14) * 60 * 60);
        const QDateTime alarmEndTime = alarmStartTime.addSecs((historyIndex * 16 + 20) * 60);
        m_dataItems[QStringLiteral("0x038F00") + suffix] = DataItem(
            QStringLiteral("0x038F00") + suffix,
            QStringLiteral("上%1次剩余电流报警事件记录").arg(historyIndex),
            QVariantList{420 - historyIndex * 11,
                         phaseCode,
                         compactDateTimeToSeconds(alarmStartTime),
                         compactDateTimeToSeconds(alarmEndTime)},
            "-",
            "AA,AA:AA:AA,AA,AA,AA,AA,AA:AA,AA,AA,AA,AA,AA",
            false,
            0);

        const QDateTime record00Time = now.addSecs(-(historyIndex * 11) * 60 * 60);
        m_dataItems[QStringLiteral("0x039000") + suffix] = DataItem(
            QStringLiteral("0x039000") + suffix,
            QStringLiteral("上%1次剩余电流记录(00组)").arg(historyIndex),
            QVariantList{compactDateTimeToSeconds(record00Time), 300 + historyIndex * 9, phaseCode},
            "-",
            "AA,AA,AA,AA,AA,AA:AA,AA:AA",
            false,
            0);

        const QDateTime record01Time = now.addSecs(-(historyIndex * 13) * 60 * 60);
        m_dataItems[QStringLiteral("0x039001") + suffix] = DataItem(
            QStringLiteral("0x039001") + suffix,
            QStringLiteral("上%1次剩余电流记录(01组)").arg(historyIndex),
            QVariantList{compactDateTimeToSeconds(record01Time), 180 + historyIndex * 7, phaseCode},
            "-",
            "AA,AA,AA,AA,AA,AA:AA,AA:AA",
            false,
            0);

        const QDateTime selfCheckTime = now.addSecs(-(historyIndex * 7 + 3) * 60 * 60);
        m_dataItems[QStringLiteral("0x038D00") + suffix] = DataItem(
            QStringLiteral("0x038D00") + suffix,
            QStringLiteral("上%1次保护器自检结果记录").arg(historyIndex),
            QVariantList{55.5 + historyIndex * 1.8,
                         48.2 + historyIndex * 1.5,
                         62.4 + historyIndex * 2.0,
                         phaseCode,
                         historyIndex,
                         compactDateTimeToSeconds(selfCheckTime),
                         (historyIndex % 4) + 1},
            "-",
            "AA,AA,xx:AA,AA,xx:AA,AA,xx:AA:AA:AA,AA,AA,AA,AA,AA:AA",
            false,
            0);

        const QDateTime tripTime = now.addSecs(-(historyIndex * 5 + 2) * 60 * 60);
        m_dataItems[QStringLiteral("0x038E00") + suffix] = DataItem(
            QStringLiteral("0x038E00") + suffix,
            QStringLiteral("上%1次跳闸前事件记录").arg(historyIndex),
            QVariantList{73.6 + historyIndex * 1.4,
                         61.3 + historyIndex * 1.2,
                         88.7 + historyIndex * 1.7,
                         223.5,
                         219.8,
                         228.6,
                         360 + historyIndex * 12,
                         compactDateTimeToSeconds(tripTime),
                         phaseCode,
                         historyIndex + 16},
            "-",
            "AA,AA,xx:AA,AA,xx:AA,AA,xx:AA,xx:AA,xx:AA,xx:AA,AA:AA,AA,AA,AA,AA,AA:AA:AA",
            false,
            0);
    }
}

void MeterModel::initializeParameterData()
{
    // 参变量 (0x04xxxxxx)
    const QDateTime now = QDateTime::currentDateTime();

    m_dataItems["0x04000000"] = DataItem("0x04000000", "电表地址", m_meterAddress, "-", "AA,AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000001"] = DataItem("0x04000001", "波特率特征字(08=2400bps)", 8, "-", "AA", true, 0);
    m_dataItems["0x04000040"] = DataItem("0x04000040", "需量周期", 15, "分钟", "AA", true, 0);
    m_dataItems["0x04000041"] = DataItem("0x04000041", "滑差时间", 1, "分钟", "AA", true, 0);

    m_dataItems["0x04000101"] = DataItem("0x04000101", "当前日期(含星期代码)", compactDateWithWeek(now.date()), "-", "AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000102"] = DataItem("0x04000102", "当前时间", now.time().toString(QStringLiteral("HHmmss")), "-", "AA,AA,AA", true, 0);
    m_dataItems["0x04000105"] = DataItem("0x04000105", "校表脉冲宽度", 100, "毫秒", "AA,AA", true, 0);
    m_dataItems["0x04000106"] = DataItem("0x04000106", "两套时区表切换时间", compactDateTimeToMinutes(now.addDays(30)), "-", "AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000107"] = DataItem("0x04000107", "两套日时段表切换时间", compactDateTimeToMinutes(now.addDays(60)), "-", "AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000108"] = DataItem("0x04000108", "两套费率电价切换时间", compactDateTimeToMinutes(now.addDays(90)), "-", "AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000109"] = DataItem("0x04000109", "两套梯度切换时间", compactDateTimeToMinutes(now.addDays(120)), "-", "AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000201"] = DataItem("0x04000201", "年时区数p", 2, "-", "AA", true, 0);
    m_dataItems["0x04000202"] = DataItem("0x04000202", "日时段表数q", 2, "-", "AA", true, 0);
    m_dataItems["0x04000203"] = DataItem("0x04000203", "日时段数m", 8, "-", "AA", true, 0);
    m_dataItems["0x04000204"] = DataItem("0x04000204", "费率数k", 5, "-", "AA", true, 0);
    m_dataItems["0x04000205"] = DataItem("0x04000205", "公共假日数n", 12, "天", "AA,AA", true, 0);
    m_dataItems["0x04000206"] = DataItem("0x04000206", "谐波分析次数", 1, "次", "AA", true, 0);
    m_dataItems["0x04000207"] = DataItem("0x04000207", "梯度数", 3, "个", "AA", true, 0);
    m_dataItems["0x04000401"] = DataItem("0x04000401", "通信地址", m_meterAddress, "-", "AA,AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000402"] = DataItem("0x04000402", "设备号", QStringLiteral("000000000001"), "-", "AA,AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000703"] = DataItem("0x04000703", "波特率代码", 8, "-", "AA", true, 0);
}

void MeterModel::initializeFreezeData()
{
    // 冻结数据 (0x05xxxxxx)
    const QDateTime lastFreeze = QDateTime::currentDateTime().addDays(-1);

    m_dataItems["0x05060001"] = DataItem("0x05060001", "上1次日冻结时间", compactDateTimeToMinutes(lastFreeze), "-", "AA,AA,AA,AA,AA", false, 0);

    m_dataItems["0x05060100"] = DataItem("0x05060100", "上1次日冻结正向有功总电能", 123.45, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060102"] = DataItem("0x05060102", "上1次日冻结正向有功费率1电能", 25.11, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060103"] = DataItem("0x05060103", "上1次日冻结正向有功费率2电能", 24.22, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060104"] = DataItem("0x05060104", "上1次日冻结正向有功费率3电能", 36.33, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060105"] = DataItem("0x05060105", "上1次日冻结正向有功费率4电能", 37.79, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060106"] = DataItem("0x05060106", "上1次日冻结正向有功费率5电能", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060101"] = DataItem("0x05060101", "上1次日冻结正向有功电能数据块", QVariantList(), "-", "AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA", false, 2);

    m_dataItems["0x05060200"] = DataItem("0x05060200", "上1次日冻结反向有功总电能", 80.15, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060202"] = DataItem("0x05060202", "上1次日冻结反向有功费率1电能", 15.01, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060203"] = DataItem("0x05060203", "上1次日冻结反向有功费率2电能", 18.22, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060204"] = DataItem("0x05060204", "上1次日冻结反向有功费率3电能", 20.33, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060205"] = DataItem("0x05060205", "上1次日冻结反向有功费率4电能", 26.59, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060206"] = DataItem("0x05060206", "上1次日冻结反向有功费率5电能", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060201"] = DataItem("0x05060201", "上1次日冻结反向有功电能数据块", QVariantList(), "-", "AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA", false, 2);

    m_dataItems["0x05060300"] = DataItem("0x05060300", "上1次日冻结组合无功1总电能", 61.28, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060302"] = DataItem("0x05060302", "上1次日冻结组合无功1费率1电能", 12.12, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060303"] = DataItem("0x05060303", "上1次日冻结组合无功1费率2电能", 13.13, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060304"] = DataItem("0x05060304", "上1次日冻结组合无功1费率3电能", 14.14, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060305"] = DataItem("0x05060305", "上1次日冻结组合无功1费率4电能", 21.89, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060306"] = DataItem("0x05060306", "上1次日冻结组合无功1费率5电能", 0.0, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060301"] = DataItem("0x05060301", "上1次日冻结组合无功1电能数据块", QVariantList(), "-", "AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA", false, 2);

    m_dataItems["0x05060400"] = DataItem("0x05060400", "上1次日冻结组合无功2总电能", 55.67, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060402"] = DataItem("0x05060402", "上1次日冻结组合无功2费率1电能", 10.1, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060403"] = DataItem("0x05060403", "上1次日冻结组合无功2费率2电能", 11.2, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060404"] = DataItem("0x05060404", "上1次日冻结组合无功2费率3电能", 12.3, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060405"] = DataItem("0x05060405", "上1次日冻结组合无功2费率4电能", 22.07, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060406"] = DataItem("0x05060406", "上1次日冻结组合无功2费率5电能", 0.0, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060401"] = DataItem("0x05060401", "上1次日冻结组合无功2电能数据块", QVariantList(), "-", "AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA", false, 2);

    synchronizeDailyFreezeTotals(m_dataItems);
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
        if (it.key().startsWith("0x02") && isDataItemSupportedUnlocked(it.key()))
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
