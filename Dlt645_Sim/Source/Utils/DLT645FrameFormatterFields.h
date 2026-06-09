#ifndef DLT645FRAMEFORMATTERFIELDS_H
#define DLT645FRAMEFORMATTERFIELDS_H

#include <QString>
#include <QStringList>

namespace DLT645FrameFormatterFields
{
inline QStringList coverEventFieldNames(const QString &eventName)
{
    const QStringList energyTypes = {
        QStringLiteral("正向有功总电能"),
        QStringLiteral("反向有功总电能"),
        QStringLiteral("第一象限无功总电能"),
        QStringLiteral("第二象限无功总电能"),
        QStringLiteral("第三象限无功总电能"),
        QStringLiteral("第四象限无功总电能")};

    QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("结束时刻")};
    for (const QString &energyType : energyTypes)
    {
        names.append(QStringLiteral("%1前%2").arg(eventName, energyType));
    }
    for (const QString &energyType : energyTypes)
    {
        names.append(QStringLiteral("%1后%2").arg(eventName, energyType));
    }
    return names;
}

inline QString phaseEventName(const QString &diCode)
{
    if (diCode.startsWith(QStringLiteral("0x0301")))
    {
        return QStringLiteral("失压");
    }
    if (diCode.startsWith(QStringLiteral("0x0302")))
    {
        return QStringLiteral("欠压");
    }
    if (diCode.startsWith(QStringLiteral("0x0303")))
    {
        return QStringLiteral("过压");
    }
    if (diCode.startsWith(QStringLiteral("0x0304")))
    {
        return QStringLiteral("断相");
    }

    return QStringLiteral("事件");
}

inline QStringList phaseVoltageEventFieldNames(const QString &eventName)
{
    const QString prefix = eventName + QStringLiteral("期间");
    const QStringList energyTypes = {
        QStringLiteral("正向有功"),
        QStringLiteral("反向有功"),
        QStringLiteral("组合无功1"),
        QStringLiteral("组合无功2")};
    const QStringList phases = {QStringLiteral("A相"), QStringLiteral("B相"), QStringLiteral("C相")};

    QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("结束时刻")};
    for (const QString &energyType : energyTypes)
    {
        names.append(QStringLiteral("%1%2总电能增量").arg(prefix, energyType));
    }

    for (const QString &phase : phases)
    {
        for (const QString &energyType : energyTypes)
        {
            names.append(QStringLiteral("%1%2%3电能增量").arg(prefix, phase, energyType));
        }
        names.append(QStringLiteral("%1时刻%2电压").arg(eventName, phase));
        names.append(QStringLiteral("%1时刻%2电流").arg(eventName, phase));
        names.append(QStringLiteral("%1时刻%2有功功率").arg(eventName, phase));
        names.append(QStringLiteral("%1时刻%2无功功率").arg(eventName, phase));
        names.append(QStringLiteral("%1时刻%2功率因数").arg(eventName, phase));
    }

    names.append(QStringLiteral("%1总安时数").arg(prefix));
    for (const QString &phase : phases)
    {
        names.append(QStringLiteral("%1%2安时数").arg(prefix, phase));
    }

    return names;
}

inline QStringList phaseElectricalEventFieldNames(const QString &eventName)
{
    const QString prefix = eventName + QStringLiteral("期间");
    const QStringList energyTypes = {
        QStringLiteral("正向有功"),
        QStringLiteral("反向有功"),
        QStringLiteral("组合无功1"),
        QStringLiteral("组合无功2")};
    const QStringList phases = {QStringLiteral("A相"), QStringLiteral("B相"), QStringLiteral("C相")};

    QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("结束时刻")};
    for (const QString &energyType : energyTypes)
    {
        names.append(QStringLiteral("%1%2总电能增量").arg(prefix, energyType));
    }

    for (const QString &phase : phases)
    {
        for (const QString &energyType : energyTypes)
        {
            names.append(QStringLiteral("%1%2%3电能增量").arg(prefix, phase, energyType));
        }
        names.append(QStringLiteral("%1时刻%2电压").arg(eventName, phase));
        names.append(QStringLiteral("%1时刻%2电流").arg(eventName, phase));
        names.append(QStringLiteral("%1时刻%2有功功率").arg(eventName, phase));
        names.append(QStringLiteral("%1时刻%2无功功率").arg(eventName, phase));
        names.append(QStringLiteral("%1时刻%2功率因数").arg(eventName, phase));
    }

    return names;
}

inline QStringList totalAndPhaseEnergyNames(const QString &prefix, const QStringList &energyTypes, const QString &suffix)
{
    QStringList names;
    for (const QString &energyType : energyTypes)
    {
        names.append(QStringLiteral("%1%2总电能%3").arg(prefix, energyType, suffix));
    }

    const QStringList phases = {QStringLiteral("A相"), QStringLiteral("B相"), QStringLiteral("C相")};
    for (const QString &phase : phases)
    {
        for (const QString &energyType : energyTypes)
        {
            names.append(QStringLiteral("%1%2%3电能%4").arg(prefix, phase, energyType, suffix));
        }
    }

    return names;
}

inline QStringList sequenceEventFieldNames(const QString &eventName)
{
    const QStringList energyTypes = {
        QStringLiteral("正向有功"),
        QStringLiteral("反向有功"),
        QStringLiteral("组合无功1"),
        QStringLiteral("组合无功2")};
    QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("结束时刻")};
    names.append(totalAndPhaseEnergyNames(eventName + QStringLiteral("期间"), energyTypes, QStringLiteral("增量")));
    return names;
}

inline QStringList unbalanceEventFieldNames(const QString &eventName)
{
    QStringList names = {QStringLiteral("发生时刻"),
                         QStringLiteral("结束时刻"),
                         QStringLiteral("最大不平衡率")};
    names.append(sequenceEventFieldNames(eventName).mid(2));
    return names;
}

inline QStringList meterClearFieldNames()
{
    const QStringList energyTypes = {
        QStringLiteral("正向有功"),
        QStringLiteral("反向有功"),
        QStringLiteral("第一象限无功"),
        QStringLiteral("第二象限无功"),
        QStringLiteral("第三象限无功"),
        QStringLiteral("第四象限无功")};
    QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("操作者代码")};
    names.append(totalAndPhaseEnergyNames(QStringLiteral("电表清零前"), energyTypes, QString()));
    return names;
}

inline QStringList demandClearFieldNames()
{
    const QStringList demandTypes = {
        QStringLiteral("正向有功"),
        QStringLiteral("反向有功"),
        QStringLiteral("第一象限无功"),
        QStringLiteral("第二象限无功"),
        QStringLiteral("第三象限无功"),
        QStringLiteral("第四象限无功")};
    const QStringList phases = {QStringLiteral("A相"), QStringLiteral("B相"), QStringLiteral("C相")};

    QStringList baseNames;
    for (const QString &demandType : demandTypes)
    {
        baseNames.append(QStringLiteral("需量清零前%1总最大需量").arg(demandType));
    }
    for (const QString &phase : phases)
    {
        for (const QString &demandType : demandTypes)
        {
            baseNames.append(QStringLiteral("需量清零前%1%2最大需量").arg(phase, demandType));
        }
    }

    QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("操作者代码")};
    for (const QString &baseName : baseNames)
    {
        names.append(baseName);
        names.append(baseName + QStringLiteral("发生时间"));
    }

    return names;
}

inline QStringList voltageComplianceFieldNames(const QString &phaseName)
{
    const QString prefix = phaseName.isEmpty() ? QStringLiteral("电压") : phaseName + QStringLiteral("电压");
    return {prefix + QStringLiteral("监测时间"),
            prefix + QStringLiteral("合格率"),
            prefix + QStringLiteral("超限率"),
            prefix + QStringLiteral("超上限时间"),
            prefix + QStringLiteral("超下限时间"),
            phaseName + QStringLiteral("最高电压"),
            phaseName + QStringLiteral("最高电压出现时间"),
            phaseName + QStringLiteral("最低电压"),
            phaseName + QStringLiteral("最低电压出现时间")};
}

inline QStringList scheduleTableProgramFieldNames()
{
    QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("操作者代码")};
    for (int setIndex = 1; setIndex <= 2; ++setIndex)
    {
        for (int tableIndex = 1; tableIndex <= 8; ++tableIndex)
        {
            names.append(QStringLiteral("时段表编程前第%1套第%2日时段表数据").arg(setIndex).arg(tableIndex));
        }
    }

    return names;
}

inline QStringList yearZoneProgramFieldNames()
{
    QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("操作者代码")};
    for (int setIndex = 1; setIndex <= 2; ++setIndex)
    {
        for (int zoneIndex = 1; zoneIndex <= 14; ++zoneIndex)
        {
            names.append(QStringLiteral("时区表编程前第%1套第%2时区表数据").arg(setIndex).arg(zoneIndex));
        }
    }

    return names;
}

inline QStringList holidayProgramFieldNames()
{
    QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("操作者代码")};
    for (int i = 1; i <= 254; ++i)
    {
        names.append(QStringLiteral("节假日编程前第%1公共假日日期及日时段表号").arg(i));
    }
    return names;
}

inline QStringList settlementDayProgramFieldNames()
{
    return {QStringLiteral("发生时刻"),
            QStringLiteral("操作者代码"),
            QStringLiteral("结算日编程前每月第1结算日"),
            QStringLiteral("结算日编程前每月第2结算日"),
            QStringLiteral("结算日编程前每月第3结算日")};
}

inline QString purchaseRecordFieldName(const QString &diCode)
{
    bool ok = false;
    const int itemIndex = diCode.mid(6, 2).toInt(&ok, 16);
    if (!ok)
    {
        return QString();
    }

    if (diCode.startsWith(QStringLiteral("0x0332")))
    {
        const QStringList names = {
            QStringLiteral("购电日期"),
            QStringLiteral("购电后总购电次数"),
            QStringLiteral("购电量"),
            QStringLiteral("购电前剩余电量"),
            QStringLiteral("购电后剩余电量"),
            QStringLiteral("购电后累计购电量")};
        return itemIndex >= 1 && itemIndex <= names.size() ? names.at(itemIndex - 1) : QString();
    }

    if (diCode.startsWith(QStringLiteral("0x0333")))
    {
        const QStringList names = {
            QStringLiteral("购电日期"),
            QStringLiteral("购电后总购电次数"),
            QStringLiteral("购电金额"),
            QStringLiteral("购电前剩余金额"),
            QStringLiteral("购电后剩余金额"),
            QStringLiteral("购电后累计购电金额")};
        return itemIndex >= 1 && itemIndex <= names.size() ? names.at(itemIndex - 1) : QString();
    }

    return QString();
}

inline bool isHistoryItem(const QString &diCode)
{
    return diCode.size() >= 10 && diCode.mid(8, 2) != QStringLiteral("00");
}

inline QStringList fieldNamesForDecodedValue(const QString &diCode)
{
    const bool isPhaseVoltageFamily = diCode.startsWith(QStringLiteral("0x0301")) ||
                                      diCode.startsWith(QStringLiteral("0x0302")) ||
                                      diCode.startsWith(QStringLiteral("0x0303")) ||
                                      diCode.startsWith(QStringLiteral("0x0304"));
    const bool isPhaseElectricalFamily = diCode.startsWith(QStringLiteral("0x030B")) ||
                                         diCode.startsWith(QStringLiteral("0x030C")) ||
                                         diCode.startsWith(QStringLiteral("0x030D"));
    const bool isPhaseSequenceFamily = diCode.startsWith(QStringLiteral("0x030E")) ||
                                       diCode.startsWith(QStringLiteral("0x030F"));
    const QString thirdByte = diCode.size() >= 8 ? diCode.mid(6, 2) : QString();

    if (isPhaseVoltageFamily && thirdByte != QStringLiteral("00") && isHistoryItem(diCode))
    {
        return phaseVoltageEventFieldNames(phaseEventName(diCode));
    }

    if (((isPhaseVoltageFamily && thirdByte == QStringLiteral("00")) ||
         diCode == QStringLiteral("0x03050000") ||
         diCode == QStringLiteral("0x03060000") ||
         diCode == QStringLiteral("0x03070000") ||
         diCode == QStringLiteral("0x03080000") ||
         diCode == QStringLiteral("0x03090000") ||
         diCode == QStringLiteral("0x030A0000") ||
         ((isPhaseElectricalFamily || isPhaseSequenceFamily) && thirdByte == QStringLiteral("00"))) &&
        (!isHistoryItem(diCode) || isPhaseVoltageFamily || isPhaseElectricalFamily || isPhaseSequenceFamily))
    {
        return {QStringLiteral("总次数"), QStringLiteral("总累计时间")};
    }

    if (diCode.startsWith(QStringLiteral("0x030500")) && isHistoryItem(diCode))
    {
        return {QStringLiteral("全失压开始时间"),
                QStringLiteral("全失压时电流"),
                QStringLiteral("全失压结束时间")};
    }

    if ((diCode.startsWith(QStringLiteral("0x030600")) ||
         diCode.startsWith(QStringLiteral("0x031100"))) &&
        isHistoryItem(diCode))
    {
        return {QStringLiteral("事件开始时间"), QStringLiteral("事件结束时间")};
    }

    if ((diCode.startsWith(QStringLiteral("0x030700")) ||
         diCode.startsWith(QStringLiteral("0x030800"))) &&
        isHistoryItem(diCode))
    {
        const QString eventName = diCode.startsWith(QStringLiteral("0x030700"))
                                      ? QStringLiteral("电压逆相序")
                                      : QStringLiteral("电流逆相序");
        return sequenceEventFieldNames(eventName);
    }

    if (diCode.startsWith(QStringLiteral("0x030900")) && isHistoryItem(diCode))
    {
        return unbalanceEventFieldNames(QStringLiteral("电压不平衡"));
    }

    if (diCode.startsWith(QStringLiteral("0x030A00")) && isHistoryItem(diCode))
    {
        return unbalanceEventFieldNames(QStringLiteral("电流不平衡"));
    }

    if (isPhaseElectricalFamily && thirdByte != QStringLiteral("00") && isHistoryItem(diCode))
    {
        QString eventName = QStringLiteral("失流");
        if (diCode.startsWith(QStringLiteral("0x030C")))
        {
            eventName = QStringLiteral("过流");
        }
        else if (diCode.startsWith(QStringLiteral("0x030D")))
        {
            eventName = QStringLiteral("断流");
        }
        return phaseElectricalEventFieldNames(eventName);
    }

    if (isPhaseSequenceFamily && thirdByte != QStringLiteral("00") && isHistoryItem(diCode))
    {
        const QString eventName = diCode.startsWith(QStringLiteral("0x030E"))
                                      ? QStringLiteral("潮流反向")
                                      : QStringLiteral("过载");
        return sequenceEventFieldNames(eventName);
    }

    if (diCode.startsWith(QStringLiteral("0x0310")))
    {
        QString phaseName;
        if (diCode.startsWith(QStringLiteral("0x031001")))
        {
            phaseName = QStringLiteral("A相");
        }
        else if (diCode.startsWith(QStringLiteral("0x031002")))
        {
            phaseName = QStringLiteral("B相");
        }
        else if (diCode.startsWith(QStringLiteral("0x031003")))
        {
            phaseName = QStringLiteral("C相");
        }
        return voltageComplianceFieldNames(phaseName);
    }

    if (diCode == QStringLiteral("0x03120000"))
    {
        return {QStringLiteral("正向有功需量超限总次数"),
                QStringLiteral("反向有功需量超限总次数"),
                QStringLiteral("第一象限无功需量超限总次数"),
                QStringLiteral("第二象限无功需量超限总次数"),
                QStringLiteral("第三象限无功需量超限总次数"),
                QStringLiteral("第四象限无功需量超限总次数")};
    }

    if (diCode.startsWith(QStringLiteral("0x0312")) && isHistoryItem(diCode))
    {
        return {QStringLiteral("发生时刻"),
                QStringLiteral("结束时刻"),
                QStringLiteral("需量超限期间最大需量"),
                QStringLiteral("需量超限期间最大需量发生时间")};
    }

    if (diCode.startsWith(QStringLiteral("0x03300D")) && isHistoryItem(diCode))
    {
        return coverEventFieldNames(QStringLiteral("开表盖"));
    }

    if (diCode.startsWith(QStringLiteral("0x03300E")) && isHistoryItem(diCode))
    {
        return coverEventFieldNames(QStringLiteral("开端钮盒"));
    }

    if (diCode.startsWith(QStringLiteral("0x033000")) && isHistoryItem(diCode))
    {
        QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("操作者代码")};
        for (int i = 1; i <= 10; ++i)
        {
            names.append(QStringLiteral("编程数据标识%1").arg(i));
        }
        names.append(QStringLiteral("扩展数据标识1"));
        names.append(QStringLiteral("扩展数据标识2"));
        return names;
    }

    if (diCode.startsWith(QStringLiteral("0x033001")) && isHistoryItem(diCode))
    {
        return meterClearFieldNames();
    }

    if (diCode.startsWith(QStringLiteral("0x033002")) && isHistoryItem(diCode))
    {
        return demandClearFieldNames();
    }

    if (diCode.startsWith(QStringLiteral("0x033003")) && isHistoryItem(diCode))
    {
        return {QStringLiteral("发生时刻"),
                QStringLiteral("操作者代码"),
                QStringLiteral("事件清零数据标识码")};
    }

    if (diCode.startsWith(QStringLiteral("0x033004")) && isHistoryItem(diCode))
    {
        return {QStringLiteral("操作者代码"),
                QStringLiteral("校时前时间"),
                QStringLiteral("校时后时间")};
    }

    if (diCode.startsWith(QStringLiteral("0x033005")) && isHistoryItem(diCode))
    {
        return scheduleTableProgramFieldNames();
    }

    if (diCode.startsWith(QStringLiteral("0x033006")) && isHistoryItem(diCode))
    {
        return yearZoneProgramFieldNames();
    }

    if (diCode.startsWith(QStringLiteral("0x033007")) && isHistoryItem(diCode))
    {
        return {QStringLiteral("发生时刻"),
                QStringLiteral("操作者代码"),
                QStringLiteral("周休日编程前采用的日时段表号")};
    }

    if (diCode.startsWith(QStringLiteral("0x033008")) && isHistoryItem(diCode))
    {
        return holidayProgramFieldNames();
    }

    if (diCode.startsWith(QStringLiteral("0x033009")) && isHistoryItem(diCode))
    {
        return {QStringLiteral("发生时刻"), QStringLiteral("操作者代码"), QStringLiteral("有功组合方式编程前特征字")};
    }

    if (diCode.startsWith(QStringLiteral("0x03300A")) && isHistoryItem(diCode))
    {
        return {QStringLiteral("发生时刻"), QStringLiteral("操作者代码"), QStringLiteral("无功组合方式1编程前特征字")};
    }

    if (diCode.startsWith(QStringLiteral("0x03300B")) && isHistoryItem(diCode))
    {
        return {QStringLiteral("发生时刻"), QStringLiteral("操作者代码"), QStringLiteral("无功组合方式2编程前特征字")};
    }

    if (diCode.startsWith(QStringLiteral("0x03300C")) && isHistoryItem(diCode))
    {
        return settlementDayProgramFieldNames();
    }

    if (diCode.startsWith(QStringLiteral("0x0332")) || diCode.startsWith(QStringLiteral("0x0333")))
    {
        const QString fieldName = purchaseRecordFieldName(diCode);
        return fieldName.isEmpty() ? QStringList{} : QStringList{fieldName};
    }

    return {};
}

inline QString decodedValueWithFieldDescriptions(const QString &diCode, const QString &decodedValue)
{
    const QStringList fieldNames = fieldNamesForDecodedValue(diCode);
    if (fieldNames.isEmpty() || !decodedValue.contains(QStringLiteral(" : ")))
    {
        return decodedValue;
    }

    const QStringList values = decodedValue.split(QStringLiteral(" : "));
    QStringList lines;
    lines.reserve(values.size());
    for (int i = 0; i < values.size(); ++i)
    {
        const QString fieldName = i < fieldNames.size()
                                      ? fieldNames.at(i)
                                      : QStringLiteral("字段%1").arg(i + 1);
        lines.append(QStringLiteral("%1：%2").arg(fieldName, values.at(i)));
    }

    return lines.join(QStringLiteral("\n"));
}
}

#endif