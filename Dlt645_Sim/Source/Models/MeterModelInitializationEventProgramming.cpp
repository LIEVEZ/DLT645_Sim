#include "MeterModelInitializationEventPrivate.h"

using namespace MeterModelPrivate;

namespace MeterModelEventPrivate
{
void initializeProgrammingEventData(const EventInitializationContext &context,
                                    QMap<QString, DataItem> &dataItems,
                                    const QString &configuredOperatorCode)
{
    const QDateTime &now = context.now;
    const QString &timeToMinutesFormat = context.timeToMinutesFormat;
    const QString &energyFormat = context.energyFormat;
    const QString &programRecordFormat = context.programRecordFormat;
    const QString &meterClearRecordFormat = context.meterClearRecordFormat;
    const QString &demandClearRecordFormat = context.demandClearRecordFormat;
    const QString &eventClearRecordFormat = context.eventClearRecordFormat;
    const QString &timeCalRecordFormat = context.timeCalRecordFormat;
    const QString &periodTableProgramRecordFormat = context.periodTableProgramRecordFormat;
    const QString &yearZoneProgramRecordFormat = context.yearZoneProgramRecordFormat;
    const QString &coverEventRecordFormat = context.coverEventRecordFormat;
    const QString &weekdayProgramRecordFormat = context.weekdayProgramRecordFormat;
    const QString &holidayProgramRecordFormat = context.holidayProgramRecordFormat;
    const QString &singleByteProgramRecordFormat = context.singleByteProgramRecordFormat;
    const QString &settlementDayProgramRecordFormat = context.settlementDayProgramRecordFormat;

dataItems["0x03300000"] = DataItem("0x03300000", "编程总次数", 8, "次", "AA,AA,AA", false, 0);
    dataItems["0x03300100"] = DataItem("0x03300100", "电表清零总次数", 5, "次", "AA,AA,AA", false, 0);
    dataItems["0x03300200"] = DataItem("0x03300200", "需量清零总次数", 4, "次", "AA,AA,AA", false, 0);
    dataItems["0x03300300"] = DataItem("0x03300300", "事件清零总次数", 3, "次", "AA,AA,AA", false, 0);
    dataItems["0x03300400"] = DataItem("0x03300400", "校时总次数", 6, "次", "AA,AA,AA", false, 0);
    dataItems["0x03300500"] = DataItem("0x03300500", "时段表编程总次数", 2, "次", "AA,AA,AA", false, 0);
    dataItems["0x03300600"] = DataItem("0x03300600", "时区表编程总次数", 1, "次", "AA,AA,AA", false, 0);
    dataItems["0x03300700"] = DataItem("0x03300700", "周休日编程总次数", 2, "次", "AA,AA,AA", false, 0);
    dataItems["0x03300800"] = DataItem("0x03300800", "节假日编程总次数", 1, "次", "AA,AA,AA", false, 0);
    dataItems["0x03300900"] = DataItem("0x03300900", "有功组合方式编程总次数", 2, "次", "AA,AA,AA", false, 0);
    dataItems["0x03300A00"] = DataItem("0x03300A00", "无功组合方式1编程总次数", 2, "次", "AA,AA,AA", false, 0);
    dataItems["0x03300B00"] = DataItem("0x03300B00", "无功组合方式2编程总次数", 2, "次", "AA,AA,AA", false, 0);
    dataItems["0x03300C00"] = DataItem("0x03300C00", "结算日编程总次数", 2, "次", "AA,AA,AA", false, 0);
    dataItems["0x03300D00"] = DataItem("0x03300D00", "开表盖总次数", 3, "次", "AA,AA,AA", false, 0);
    dataItems["0x03300E00"] = DataItem("0x03300E00", "开端钮盒总次数", 2, "次", "AA,AA,AA", false, 0);

    const QStringList programmedDiCodes = {
        QStringLiteral("03010000"), QStringLiteral("03010001"), QStringLiteral("03010002"),
        QStringLiteral("03110000"), QStringLiteral("03120000"), QStringLiteral("03300000"),
        QStringLiteral("03300100"), QStringLiteral("03300200"), QStringLiteral("03300300"),
        QStringLiteral("03300400"), QStringLiteral("03300500"), QStringLiteral("03300600")};

    for (int historyIndex = 1; historyIndex <= 10; ++historyIndex)
    {
        const QString operatorCode = context.defaultOperatorCode(configuredOperatorCode, historyIndex);
        const QDateTime eventTime = now.addSecs(-(historyIndex * 5 + 2) * 60 * 60);

        QVariantList programmingRecord;
        programmingRecord.append(compactDateTimeToSeconds(eventTime));
        programmingRecord.append(operatorCode);
        for (const QString &diCode : programmedDiCodes)
        {
            programmingRecord.append(diCode);
        }
        const QString programmingCode = QStringLiteral("0x033000") + upperHexByte(historyIndex);
        dataItems[programmingCode] = DataItem(
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
        context.appendEnergySnapshot(meterClearRecord, 120.0 + historyIndex * 12.5);
        const QString meterClearCode = QStringLiteral("0x033001") + upperHexByte(historyIndex);
        dataItems[meterClearCode] = DataItem(
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
        context.appendDemandSnapshot(demandClearRecord, historyIndex, 1.8 + historyIndex * 0.2);
        const QString demandClearCode = QStringLiteral("0x033002") + upperHexByte(historyIndex);
        dataItems[demandClearCode] = DataItem(
            demandClearCode,
            QStringLiteral("（上%1次）需量清零记录").arg(historyIndex),
            demandClearRecord,
            QStringLiteral("-"),
            demandClearRecordFormat,
            false,
            0);

        const QString eventClearCode = QStringLiteral("0x033003") + upperHexByte(historyIndex);
        dataItems[eventClearCode] = DataItem(
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
        dataItems[timeCalCode] = DataItem(
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
        context.appendPeriodTableSnapshot(periodTableRecord, historyIndex);
        const QString periodTableCode = QStringLiteral("0x033005") + upperHexByte(historyIndex);
        dataItems[periodTableCode] = DataItem(
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
        context.appendYearZoneSnapshot(yearZoneRecord, historyIndex);
        const QString yearZoneCode = QStringLiteral("0x033006") + upperHexByte(historyIndex);
        dataItems[yearZoneCode] = DataItem(
            yearZoneCode,
            QStringLiteral("（上%1次）时区表编程记录").arg(historyIndex),
            yearZoneRecord,
            QStringLiteral("-"),
            yearZoneProgramRecordFormat,
            false,
            0);

        const QString weekdayCode = QStringLiteral("0x033007") + upperHexByte(historyIndex);
        dataItems[weekdayCode] = DataItem(
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
        dataItems[holidayCode] = DataItem(
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
            dataItems[code] = DataItem(
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
        dataItems[settlementDayCode] = DataItem(
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
        context.appendCoverEventEnergySnapshot(coverOpenRecord, 14.64 + historyIndex * 3.2);
        const QString coverOpenCode = QStringLiteral("0x03300D") + upperHexByte(historyIndex);
        dataItems[coverOpenCode] = DataItem(
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
        context.appendCoverEventEnergySnapshot(terminalCoverRecord, 21.30 + historyIndex * 3.6);
        const QString terminalCoverCode = QStringLiteral("0x03300E") + upperHexByte(historyIndex);
        dataItems[terminalCoverCode] = DataItem(
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

        dataItems[QStringLiteral("0x033201") + suffix] = DataItem(QStringLiteral("0x033201") + suffix,
                                                                     QStringLiteral("（上%1次）购电日期").arg(historyIndex),
                                                                     compactDateTimeToMinutes(purchaseTime),
                                                                     QStringLiteral("-"),
                                                                     timeToMinutesFormat,
                                                                     false,
                                                                     0);
        dataItems[QStringLiteral("0x033202") + suffix] = DataItem(QStringLiteral("0x033202") + suffix,
                                                                     QStringLiteral("（上%1次）购电后总购电次数").arg(historyIndex),
                                                                     purchaseCount,
                                                                     QStringLiteral("次"),
                                                                     QStringLiteral("AA,AA"),
                                                                     false,
                                                                     0);
        dataItems[QStringLiteral("0x033203") + suffix] = DataItem(QStringLiteral("0x033203") + suffix,
                                                                     QStringLiteral("（上%1次）购电量").arg(historyIndex),
                                                                     purchaseEnergy,
                                                                     QStringLiteral("kWh"),
                                                                     energyFormat,
                                                                     false,
                                                                     2);
        dataItems[QStringLiteral("0x033204") + suffix] = DataItem(QStringLiteral("0x033204") + suffix,
                                                                     QStringLiteral("（上%1次）购电前剩余电量").arg(historyIndex),
                                                                     remainingEnergyBefore,
                                                                     QStringLiteral("kWh"),
                                                                     energyFormat,
                                                                     false,
                                                                     2);
        dataItems[QStringLiteral("0x033205") + suffix] = DataItem(QStringLiteral("0x033205") + suffix,
                                                                     QStringLiteral("（上%1次）购电后剩余电量").arg(historyIndex),
                                                                     remainingEnergyAfter,
                                                                     QStringLiteral("kWh"),
                                                                     energyFormat,
                                                                     false,
                                                                     2);
        dataItems[QStringLiteral("0x033206") + suffix] = DataItem(QStringLiteral("0x033206") + suffix,
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
        dataItems[QStringLiteral("0x033301") + suffix] = DataItem(QStringLiteral("0x033301") + suffix,
                                                                     QStringLiteral("（上%1次）购电金额日期").arg(historyIndex),
                                                                     compactDateTimeToMinutes(purchaseTime),
                                                                     QStringLiteral("-"),
                                                                     timeToMinutesFormat,
                                                                     false,
                                                                     0);
        dataItems[QStringLiteral("0x033302") + suffix] = DataItem(QStringLiteral("0x033302") + suffix,
                                                                     QStringLiteral("（上%1次）购电金额后总购电次数").arg(historyIndex),
                                                                     purchaseCount,
                                                                     QStringLiteral("次"),
                                                                     QStringLiteral("AA,AA"),
                                                                     false,
                                                                     0);
        dataItems[QStringLiteral("0x033303") + suffix] = DataItem(QStringLiteral("0x033303") + suffix,
                                                                     QStringLiteral("（上%1次）购电金额").arg(historyIndex),
                                                                     purchaseAmount,
                                                                     QStringLiteral("元"),
                                                                     energyFormat,
                                                                     false,
                                                                     2);
        dataItems[QStringLiteral("0x033304") + suffix] = DataItem(QStringLiteral("0x033304") + suffix,
                                                                     QStringLiteral("（上%1次）购电前剩余金额").arg(historyIndex),
                                                                     balanceBefore,
                                                                     QStringLiteral("元"),
                                                                     energyFormat,
                                                                     false,
                                                                     2);
        dataItems[QStringLiteral("0x033305") + suffix] = DataItem(QStringLiteral("0x033305") + suffix,
                                                                     QStringLiteral("（上%1次）购电后剩余金额").arg(historyIndex),
                                                                     balanceAfter,
                                                                     QStringLiteral("元"),
                                                                     energyFormat,
                                                                     false,
                                                                     2);
        dataItems[QStringLiteral("0x033306") + suffix] = DataItem(QStringLiteral("0x033306") + suffix,
                                                                     QStringLiteral("（上%1次）累计购电金额").arg(historyIndex),
                                                                     cumulativeAmount,
                                                                     QStringLiteral("元"),
                                                                     energyFormat,
                                                                     false,
                                                                     2);
    }
}
}