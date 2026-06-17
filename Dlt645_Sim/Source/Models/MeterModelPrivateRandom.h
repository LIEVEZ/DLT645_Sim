#ifndef METERMODELPRIVATERANDOM_H
#define METERMODELPRIVATERANDOM_H

#include "MeterModelPrivateSync.h"

namespace MeterModelPrivate
{
    inline QString randomDigits(QRandomGenerator *rng, int length)
    {
        QString value;
        value.reserve(length);
        for (int index = 0; index < length; ++index)
        {
            value.append(QChar('0' + rng->bounded(0, 10)));
        }
        return value;
    }

    inline QVariant randomizeNumericValueForFormat(const NumericFormatSpec &formatSpec, QRandomGenerator *rng)
    {
        const double maxValue = maxRepresentableValue(formatSpec);
        const double minValue = formatSpec.allowNegative ? -maxValue : 0.0;
        const double scale = std::pow(10.0, formatSpec.decimalDigits);
        const double range = (maxValue - minValue) * scale;

        // 防止整数溢出：如果范围超过 INT_MAX，使用浮点数随机化
        double randomValue;
        if (range > static_cast<double>(INT_MAX))
        {
            // 大范围：直接使用浮点数随机化
            const double randomFraction = rng->bounded(0, 10000) / 10000.0;
            randomValue = minValue + (maxValue - minValue) * randomFraction;
        }
        else
        {
            // 小范围：使用整数步进保证精度
            const int randomSteps = rng->bounded(0, static_cast<int>(range) + 1);
            randomValue = minValue + randomSteps / scale;
        }

        return normalizeValueForFormat(randomValue, formatSpec);
    }

    inline bool isNumericVariantType(const QVariant &value)
    {
        switch (value.type())
        {
        case QVariant::Double:
        case QVariant::Int:
        case QVariant::UInt:
        case QVariant::LongLong:
        case QVariant::ULongLong:
            return true;
        default:
            return false;
        }
    }

    inline QVariant randomizeEventScalarValue(const QVariant &value, const QString &fieldFormat, QRandomGenerator *rng)
    {
        const NumericFormatSpec formatSpec = parseNumericFormatSpec(fieldFormat);
        if (formatSpec.valid && isNumericVariantType(value))
        {
            return randomizeNumericValueForFormat(formatSpec, rng);
        }

        if (value.type() == QVariant::Double)
        {
            return roundToDecimals(rng->bounded(0, 10000) / 100.0, 2);
        }

        if (value.type() == QVariant::Int || value.type() == QVariant::UInt ||
            value.type() == QVariant::LongLong || value.type() == QVariant::ULongLong)
        {
            return rng->bounded(0, 10000);
        }

        const QString text = value.toString();
        if (text.isEmpty())
        {
            return value;
        }

        for (const QChar &ch : text)
        {
            if (!ch.isDigit())
            {
                return value;
            }
        }

        const QDateTime randomTime = QDateTime::currentDateTime().addSecs(-rng->bounded(60, 180 * 24 * 60 * 60));
        if (text.size() == 12)
        {
            return compactDateTimeToSeconds(randomTime);
        }
        if (text.size() == 10)
        {
            return compactDateTimeToMinutes(randomTime);
        }

        return randomDigits(rng, text.size());
    }

    inline bool randomizeEventListItem(DataItem &item, QRandomGenerator *rng)
    {
        if (!item.diCode.startsWith(QStringLiteral("0x03")) || item.value.type() != QVariant::List)
        {
            return false;
        }

        const QVariantList originalValues = item.value.toList();
        if (originalValues.isEmpty())
        {
            return false;
        }

        QVariantList randomizedValues;
        randomizedValues.reserve(originalValues.size());
        const QStringList fieldFormats = item.format.split(QStringLiteral(":"), Qt::KeepEmptyParts);
        for (int valueIndex = 0; valueIndex < originalValues.size(); ++valueIndex)
        {
            const QString fieldFormat = valueIndex < fieldFormats.size() ? fieldFormats.at(valueIndex) : QString();
            randomizedValues.append(randomizeEventScalarValue(originalValues.at(valueIndex), fieldFormat, rng));
        }

        item.value = randomizedValues;
        return true;
    }

    /**
     * @brief 生成事件记录的时间范围和指标数据
     * @param rng 随机数生成器
     * @param metricsCount 指标数量（16）
     * @param includeThirdField 是否包含第三个字段（某些事件类型需要）
     * @return 包含时间范围和指标的 QVariantList
     */
    inline QVariantList buildEventMetricsWithTime(QRandomGenerator *rng, int metricsCount, bool includeThirdField = false)
    {
        QVariantList values;
        const QDateTime endTime = QDateTime::currentDateTime().addSecs(-rng->bounded(2 * 60 * 60, 120 * 24 * 60 * 60));
        const QDateTime startTime = endTime.addSecs(-rng->bounded(10 * 60, 18 * 60 * 60));
        values.append(compactDateTimeToSeconds(startTime));
        values.append(compactDateTimeToSeconds(endTime));

        // 某些事件类型（如 0x030900, 0x030A00）在时间后有一个额外字段
        if (includeThirdField)
        {
            values.append(roundToDecimals(rng->bounded(50, 1200) / 100.0, 2));
        }

        // 添加指标数据
        for (int index = 0; index < metricsCount; ++index)
        {
            values.append(roundToDecimals(rng->bounded(100, 12000) / 100.0, 2));
        }

        return values;
    }

inline void randomizeDataItems(QMap<QString, DataItem> &items,
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

            if (isDataBlockItem(item) && !item.diCode.startsWith(QStringLiteral("0x03")))
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
                item.value = buildEventMetricsWithTime(rng, 16, false);
                continue;
            }

            if (item.diCode.startsWith("0x030900") && item.diCode != "0x03090000")
            {
                item.value = buildEventMetricsWithTime(rng, 16, true);
                continue;
            }

            if (item.diCode.startsWith("0x030A00") && item.diCode != "0x030A0000")
            {
                item.value = buildEventMetricsWithTime(rng, 16, true);
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

            if (randomizeEventListItem(item, rng))
            {
                continue;
            }

            const QVariant originalValue = item.value;
            const NumericFormatSpec formatSpec = parseNumericFormatSpec(item.format);
            bool variableValueHandled = false;

            if (item.diCode.startsWith("0x0202") || item.diCode == "0x02800001")
            {
                item.value = rng->bounded(-10000, 10001) / 1000.0;
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

#endif // METERMODELPRIVATERANDOM_H
