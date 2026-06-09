#include "MeterModelInitializationEventPrivate.h"

using namespace MeterModelPrivate;

namespace MeterModelEventPrivate
{
namespace
{
void initializePhaseVoltageEventFamily(const EventInitializationContext &context,
                                       QMap<QString, DataItem> &dataItems,
                                       const QString &totalPrefix,
                                       const QString &historyPrefixBase,
                                       const QString &eventName,
                                       double baseVoltageOffset)
{
    for (int phaseIndex = 0; phaseIndex < context.phaseNames.size(); ++phaseIndex)
    {
        const QString totalCode = totalPrefix + upperHexByte(phaseIndex);
        dataItems[totalCode] = DataItem(
            totalCode,
            QStringLiteral("%1%2总次数，总累计时间").arg(context.phaseNames[phaseIndex], eventName),
            QVariantList{10 + phaseIndex * 2, 240 + phaseIndex * 30},
            QStringLiteral("次,分"),
            QStringList{context.countFormat, context.countFormat}.join(QStringLiteral(":")),
            false,
            0);

        const QString historyPrefix = historyPrefixBase + upperHexByte(phaseIndex + 1);
        for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
        {
            QVariantList values;
            const QDateTime endTime = context.now.addSecs(-(historyIndex * 8 + phaseIndex + 1) * 60 * 60);
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

            const QString code = context.phaseEventCode(historyPrefix, historyIndex);
            dataItems[code] = DataItem(
                code,
                QStringLiteral("（上%1次）%2%3记录").arg(historyIndex).arg(context.phaseNames[phaseIndex], eventName),
                values,
                QStringLiteral("-"),
                context.lossVoltageRecordFormat,
                false,
                0);
        }
    }
}

void initializePhaseElectricalEventFamily(const EventInitializationContext &context,
                                          QMap<QString, DataItem> &dataItems,
                                          const QString &totalPrefix,
                                          const QString &historyPrefixBase,
                                          const QString &eventName,
                                          double abnormalCurrentBase)
{
    for (int phaseIndex = 0; phaseIndex < context.phaseNames.size(); ++phaseIndex)
    {
        const QString totalCode = totalPrefix + upperHexByte(phaseIndex);
        dataItems[totalCode] = DataItem(
            totalCode,
            QStringLiteral("%1%2总次数，总累计时间").arg(context.phaseNames[phaseIndex], eventName),
            QVariantList{8 + phaseIndex * 2, 190 + phaseIndex * 25},
            QStringLiteral("次,分"),
            QStringList{context.countFormat, context.countFormat}.join(QStringLiteral(":")),
            false,
            0);

        const QString historyPrefix = historyPrefixBase + upperHexByte(phaseIndex + 1);
        for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
        {
            QVariantList values;
            const QDateTime endTime = context.now.addSecs(-(historyIndex * 9 + phaseIndex + 3) * 60 * 60);
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

            const QString code = context.phaseEventCode(historyPrefix, historyIndex);
            dataItems[code] = DataItem(
                code,
                QStringLiteral("（上%1次）%2%3记录").arg(historyIndex).arg(context.phaseNames[phaseIndex], eventName),
                values,
                QStringLiteral("-"),
                context.lossVoltageRecordFormat,
                false,
                0);
        }
    }
}

void initializePhaseSequenceEventFamily(const EventInitializationContext &context,
                                        QMap<QString, DataItem> &dataItems,
                                        const QString &totalPrefix,
                                        const QString &historyPrefixBase,
                                        const QString &eventName,
                                        double baseEnergy)
{
    for (int phaseIndex = 0; phaseIndex < context.phaseNames.size(); ++phaseIndex)
    {
        const QString totalCode = totalPrefix + upperHexByte(phaseIndex);
        dataItems[totalCode] = DataItem(
            totalCode,
            QStringLiteral("%1%2总次数，总累计时间").arg(context.phaseNames[phaseIndex], eventName),
            QVariantList{4 + phaseIndex, 85 + phaseIndex * 18},
            QStringLiteral("次,分"),
            QStringList{context.countFormat, context.countFormat}.join(QStringLiteral(":")),
            false,
            0);

        const QString historyPrefix = historyPrefixBase + upperHexByte(phaseIndex + 1);
        for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
        {
            QVariantList values;
            const QDateTime endTime = context.now.addSecs(-(historyIndex * 15 + phaseIndex + 4) * 60 * 60);
            const QDateTime startTime = endTime.addSecs(-(historyIndex * 9 + 14) * 60);
            values.append(compactDateTimeToSeconds(startTime));
            values.append(compactDateTimeToSeconds(endTime));
            context.appendSequenceEnergySnapshot(values, baseEnergy + phaseIndex * 3.0 + historyIndex * 1.8);

            const QString code = context.phaseEventCode(historyPrefix, historyIndex);
            dataItems[code] = DataItem(
                code,
                QStringLiteral("（上%1次）%2%3记录").arg(historyIndex).arg(context.phaseNames[phaseIndex], eventName),
                values,
                QStringLiteral("-"),
                context.reverseSequenceRecordFormat,
                false,
                0);
        }
    }
}
}

void initializePhaseEventData(const EventInitializationContext &context, QMap<QString, DataItem> &dataItems)
{
    const QDateTime &now = context.now;
    const QString &countFormat = context.countFormat;
    const QString &fullLossRecordFormat = context.fullLossRecordFormat;
    const QString &simpleStartEndRecordFormat = context.simpleStartEndRecordFormat;
    const QString &reverseSequenceRecordFormat = context.reverseSequenceRecordFormat;
    const QString &voltageUnbalanceRecordFormat = context.voltageUnbalanceRecordFormat;

    initializePhaseVoltageEventFamily(context, dataItems, QStringLiteral("0x030100"), QStringLiteral("0x0301"), QStringLiteral("失压"), 168.0);
    initializePhaseVoltageEventFamily(context, dataItems, QStringLiteral("0x030200"), QStringLiteral("0x0302"), QStringLiteral("欠压"), 172.0);
    initializePhaseVoltageEventFamily(context, dataItems, QStringLiteral("0x030300"), QStringLiteral("0x0303"), QStringLiteral("过压"), 248.0);
    initializePhaseVoltageEventFamily(context, dataItems, QStringLiteral("0x030400"), QStringLiteral("0x0304"), QStringLiteral("断相"), 0.0);

    dataItems["0x03050000"] = DataItem(
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
        dataItems[code] = DataItem(
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

    dataItems["0x03060000"] = DataItem(
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
        dataItems[code] = DataItem(
            code,
            QStringLiteral("（上%1次）辅助电源失电记录").arg(historyIndex),
            QVariantList{compactDateTimeToSeconds(startTime), compactDateTimeToSeconds(endTime)},
            QStringLiteral("-"),
            simpleStartEndRecordFormat,
            false,
            0);
    }

    dataItems["0x03070000"] = DataItem(
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
        context.appendSequenceEnergySnapshot(values, 10.0 + historyIndex * 2.2);
        dataItems[code] = DataItem(
            code,
            QStringLiteral("（上%1次）电压逆相序记录").arg(historyIndex),
            values,
            QStringLiteral("-"),
            reverseSequenceRecordFormat,
            false,
            0);
    }

    dataItems["0x03080000"] = DataItem(
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
        context.appendSequenceEnergySnapshot(values, 8.0 + historyIndex * 1.9);
        dataItems[code] = DataItem(
            code,
            QStringLiteral("（上%1次）电流逆相序记录").arg(historyIndex),
            values,
            QStringLiteral("-"),
            reverseSequenceRecordFormat,
            false,
            0);
    }

    dataItems["0x03090000"] = DataItem(
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
        context.appendSequenceEnergySnapshot(values, 9.5 + historyIndex * 2.0);
        dataItems[code] = DataItem(
            code,
            QStringLiteral("（上%1次）电压不平衡记录").arg(historyIndex),
            values,
            QStringLiteral("-"),
            voltageUnbalanceRecordFormat,
            false,
            0);
    }

    dataItems["0x030A0000"] = DataItem(
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
        context.appendSequenceEnergySnapshot(values, 8.8 + historyIndex * 1.9);
        dataItems[code] = DataItem(
            code,
            QStringLiteral("（上%1次）电流不平衡记录").arg(historyIndex),
            values,
            QStringLiteral("-"),
            voltageUnbalanceRecordFormat,
            false,
            0);
    }

    initializePhaseElectricalEventFamily(context, dataItems, QStringLiteral("0x030B00"), QStringLiteral("0x030B"), QStringLiteral("失流"), 0.08);
    initializePhaseElectricalEventFamily(context, dataItems, QStringLiteral("0x030C00"), QStringLiteral("0x030C"), QStringLiteral("过流"), 9.50);
    initializePhaseElectricalEventFamily(context, dataItems, QStringLiteral("0x030D00"), QStringLiteral("0x030D"), QStringLiteral("断流"), 0.00);
    initializePhaseSequenceEventFamily(context, dataItems, QStringLiteral("0x030E00"), QStringLiteral("0x030E"), QStringLiteral("潮流反向"), 11.0);
    initializePhaseSequenceEventFamily(context, dataItems, QStringLiteral("0x030F00"), QStringLiteral("0x030F"), QStringLiteral("过载"), 13.5);
}
}