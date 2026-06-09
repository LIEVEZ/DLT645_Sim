#include "MeterModelInitializationEventPrivate.h"

using namespace MeterModelPrivate;

namespace MeterModelEventPrivate
{
void initializeStatisticalEventData(const EventInitializationContext &context, QMap<QString, DataItem> &dataItems)
{
    const QDateTime &now = context.now;
    const QStringList &phaseNames = context.phaseNames;
    const QStringList &demandLimitNames = context.demandLimitNames;
    const QString &timeToSecondsFormat = context.timeToSecondsFormat;
    const QString &demandLimitTotalsFormat = context.demandLimitTotalsFormat;
    const QString &demandLimitRecordFormat = context.demandLimitRecordFormat;
    const QString &totalVoltageComplianceFormat = context.totalVoltageComplianceFormat;
    const QString &phaseVoltageComplianceFormat = context.phaseVoltageComplianceFormat;

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

        dataItems[code] = DataItem(
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

    dataItems["0x03110000"] = DataItem("0x03110000", "掉电总次数", 9, "次", "AA,AA,AA", false, 0);
    for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
    {
        const QString code = QStringLiteral("0x031100") + upperHexByte(historyIndex);
        const QDateTime endTime = now.addSecs(-(historyIndex * 12 + 3) * 60 * 60);
        const QDateTime startTime = endTime.addSecs(-(historyIndex * 8 + 20) * 60);
        dataItems[code] = DataItem(
            code,
            QStringLiteral("（上%1次）掉电记录").arg(historyIndex),
            QVariantList{compactDateTimeToSeconds(startTime), compactDateTimeToSeconds(endTime)},
            QStringLiteral("-"),
            QStringList{timeToSecondsFormat, timeToSecondsFormat}.join(QStringLiteral(":")),
            false,
            0);
    }

    dataItems["0x03120000"] = DataItem(
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
            dataItems[code] = DataItem(
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
}
}