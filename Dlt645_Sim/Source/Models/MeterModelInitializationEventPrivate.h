#ifndef METERMODELINITIALIZATIONEVENTPRIVATE_H
#define METERMODELINITIALIZATIONEVENTPRIVATE_H

#include "MeterModelPrivate.h"

namespace MeterModelEventPrivate
{
using namespace MeterModelPrivate;

struct EventInitializationContext
{
    EventInitializationContext()
        : now(QDateTime::currentDateTime()),
          phaseNames{QStringLiteral("A相"), QStringLiteral("B相"), QStringLiteral("C相")},
          demandLimitNames{QStringLiteral("正向有功"),
                           QStringLiteral("反向有功"),
                           QStringLiteral("第一象限无功"),
                           QStringLiteral("第二象限无功"),
                           QStringLiteral("第三象限无功"),
                           QStringLiteral("第四象限无功")},
          countFormat(QStringLiteral("AA,AA,AA")),
          timeToSecondsFormat(QStringLiteral("AA,AA,AA,AA,AA,AA")),
          timeToMinutesFormat(QStringLiteral("AA,AA,AA,AA,AA")),
          operatorCodeFormat(QStringLiteral("AA,AA,AA,AA")),
          diCodeFormat(QStringLiteral("AA,AA,AA,AA")),
          energyFormat(QStringLiteral("AA,AA,AA,.,AA")),
          demandFormat(QStringLiteral("AA,.,AAAA")),
          powerFormat(QStringLiteral("AA,.,AAAA")),
          currentFormat(QStringLiteral("AA,.,AAA")),
          voltageFormat(QStringLiteral("AA,AA,.,A")),
          powerFactorFormat(QStringLiteral("AA,.,A")),
          percentFormat(QStringLiteral("AA,.,AA")),
          largePercentFormat(QStringLiteral("AA,AA,.,AA")),
          monthTimeFormat(QStringLiteral("AA,AA,AA,AA")),
          scheduleTableFormat(repeatedByteFormat(kMaxDayScheduleCount * 3)),
          yearZoneEntryFormat(QStringLiteral("AA,AA,AA")),
          programmedDiCount(12)
    {
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
        lossVoltageRecordFormat = lossVoltageFormatParts.join(QStringLiteral(":"));
        demandLimitTotalsFormat = repeatedFormatParts(countFormat, demandLimitNames.size());
        demandLimitRecordFormat = QStringList{timeToSecondsFormat,
                                              timeToSecondsFormat,
                                              demandFormat,
                                              timeToMinutesFormat}
                                      .join(QStringLiteral(":"));
        programRecordFormat = QStringList{timeToSecondsFormat,
                                          operatorCodeFormat,
                                          repeatedFormatParts(diCodeFormat, programmedDiCount)}
                                  .join(QStringLiteral(":"));
        meterClearRecordFormat = QStringList{timeToSecondsFormat, operatorCodeFormat, repeatedFormatParts(energyFormat, 24)}
                                    .join(QStringLiteral(":"));
        demandClearRecordFormat = QStringList{timeToSecondsFormat,
                                             operatorCodeFormat,
                                             repeatedFormatParts(QStringList{demandFormat, timeToMinutesFormat}.join(QStringLiteral(":")), 24)}
                                      .join(QStringLiteral(":"));
        eventClearRecordFormat = QStringList{timeToSecondsFormat, operatorCodeFormat, diCodeFormat}.join(QStringLiteral(":"));
        timeCalRecordFormat = QStringList{operatorCodeFormat, timeToSecondsFormat, timeToSecondsFormat}.join(QStringLiteral(":"));
        periodTableProgramRecordFormat = QStringList{timeToSecondsFormat, operatorCodeFormat, repeatedFormatParts(scheduleTableFormat, 16)}
                                             .join(QStringLiteral(":"));
        yearZoneProgramRecordFormat = QStringList{timeToSecondsFormat, operatorCodeFormat, repeatedFormatParts(yearZoneEntryFormat, 28)}
                                          .join(QStringLiteral(":"));
        coverEventRecordFormat = QStringList{timeToSecondsFormat, timeToSecondsFormat, repeatedFormatParts(energyFormat, 12)}
                                     .join(QStringLiteral(":"));
        weekdayProgramRecordFormat = QStringList{timeToSecondsFormat, operatorCodeFormat, QStringLiteral("AA")}.join(QStringLiteral(":"));
        holidayProgramRecordFormat = QStringList{timeToSecondsFormat,
                                                 operatorCodeFormat,
                                                 repeatedFormatParts(QStringLiteral("AA,AA,AA,AA"), kMaxHolidayCount)}
                                         .join(QStringLiteral(":"));
        singleByteProgramRecordFormat = weekdayProgramRecordFormat;
        settlementDayProgramRecordFormat = QStringList{timeToSecondsFormat,
                                                       operatorCodeFormat,
                                                       repeatedFormatParts(QStringLiteral("AA,AA"), 3)}
                                               .join(QStringLiteral(":"));
        fullLossRecordFormat = QStringList{timeToSecondsFormat, currentFormat, timeToSecondsFormat}.join(QStringLiteral(":"));
        simpleStartEndRecordFormat = QStringList{timeToSecondsFormat, timeToSecondsFormat}.join(QStringLiteral(":"));
        reverseSequenceRecordFormat = QStringList{timeToSecondsFormat, timeToSecondsFormat, repeatedFormatParts(energyFormat, 16)}
                                          .join(QStringLiteral(":"));
        voltageUnbalanceRecordFormat = QStringList{timeToSecondsFormat, timeToSecondsFormat, percentFormat, repeatedFormatParts(energyFormat, 16)}
                                           .join(QStringLiteral(":"));
        totalVoltageComplianceFormat = QStringList{countFormat,
                                                   largePercentFormat,
                                                   largePercentFormat,
                                                   countFormat,
                                                   countFormat,
                                                   powerFactorFormat,
                                                   monthTimeFormat,
                                                   powerFactorFormat,
                                                   monthTimeFormat}
                                           .join(QStringLiteral(":"));
        phaseVoltageComplianceFormat = QStringList{countFormat,
                                                   percentFormat,
                                                   percentFormat,
                                                   countFormat,
                                                   countFormat,
                                                   powerFactorFormat,
                                                   monthTimeFormat,
                                                   powerFactorFormat,
                                                   monthTimeFormat}
                                           .join(QStringLiteral(":"));
    }

    QString phaseEventCode(const QString &familyPrefix, int historyIndex) const
    {
        return familyPrefix + upperHexByte(historyIndex);
    }

    QString defaultOperatorCode(const QString &configuredOperatorCode, int historyIndex) const
    {
        if (!configuredOperatorCode.isEmpty())
        {
            return configuredOperatorCode;
        }
        return QStringLiteral("%1").arg((historyIndex * 1234567) % 100000000, 8, 10, QChar('0'));
    }

    void appendEnergySnapshot(QVariantList &values, double baseValue) const
    {
        for (int i = 0; i < 24; ++i)
        {
            values.append(roundToDecimals(baseValue + i * 7.35, 2));
        }
    }

    void appendDemandSnapshot(QVariantList &values, int historyIndex, double baseValue) const
    {
        for (int i = 0; i < 24; ++i)
        {
            values.append(roundToDecimals(baseValue + i * 0.12, 4));
            values.append(compactDateTimeToMinutes(now.addSecs(-(historyIndex * 9 + i + 1) * 30 * 60)));
        }
    }

    void appendPeriodTableSnapshot(QVariantList &values, int historyIndex) const
    {
        for (int setIndex = 0; setIndex < 2; ++setIndex)
        {
            for (int tableIndex = 0; tableIndex < kMaxDayScheduleTableCount; ++tableIndex)
            {
                values.append(buildDayScheduleTableValue(historyIndex, setIndex, tableIndex));
            }
        }
    }

    void appendYearZoneSnapshot(QVariantList &values, int historyIndex) const
    {
        for (int setIndex = 0; setIndex < 2; ++setIndex)
        {
            for (int zoneIndex = 0; zoneIndex < kMaxYearZoneCount; ++zoneIndex)
            {
                values.append(buildYearZoneEntryValue(historyIndex, setIndex, zoneIndex));
            }
        }
    }

    void appendSequenceEnergySnapshot(QVariantList &values, double baseValue) const
    {
        for (int i = 0; i < 16; ++i)
        {
            values.append(roundToDecimals(baseValue + i * 3.15, 2));
        }
    }

    void appendCoverEventEnergySnapshot(QVariantList &values, double baseValue) const
    {
        for (int i = 0; i < 12; ++i)
        {
            values.append(roundToDecimals(baseValue + i * 4.25, 2));
        }
    }

    QDateTime now;
    QStringList phaseNames;
    QStringList demandLimitNames;
    QString countFormat;
    QString timeToSecondsFormat;
    QString timeToMinutesFormat;
    QString operatorCodeFormat;
    QString diCodeFormat;
    QString energyFormat;
    QString demandFormat;
    QString powerFormat;
    QString currentFormat;
    QString voltageFormat;
    QString powerFactorFormat;
    QString percentFormat;
    QString largePercentFormat;
    QString monthTimeFormat;
    QString scheduleTableFormat;
    QString yearZoneEntryFormat;
    int programmedDiCount;
    QString lossVoltageRecordFormat;
    QString demandLimitTotalsFormat;
    QString demandLimitRecordFormat;
    QString programRecordFormat;
    QString meterClearRecordFormat;
    QString demandClearRecordFormat;
    QString eventClearRecordFormat;
    QString timeCalRecordFormat;
    QString periodTableProgramRecordFormat;
    QString yearZoneProgramRecordFormat;
    QString coverEventRecordFormat;
    QString weekdayProgramRecordFormat;
    QString holidayProgramRecordFormat;
    QString singleByteProgramRecordFormat;
    QString settlementDayProgramRecordFormat;
    QString fullLossRecordFormat;
    QString simpleStartEndRecordFormat;
    QString reverseSequenceRecordFormat;
    QString voltageUnbalanceRecordFormat;
    QString totalVoltageComplianceFormat;
    QString phaseVoltageComplianceFormat;
};

void initializePhaseEventData(const EventInitializationContext &context, QMap<QString, DataItem> &dataItems);
void initializeStatisticalEventData(const EventInitializationContext &context, QMap<QString, DataItem> &dataItems);
void initializeProgrammingEventData(const EventInitializationContext &context,
                                    QMap<QString, DataItem> &dataItems,
                                    const QString &configuredOperatorCode);
}

#endif