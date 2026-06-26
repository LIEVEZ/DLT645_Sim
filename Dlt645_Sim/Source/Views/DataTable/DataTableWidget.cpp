#include "DataTableWidget.h"
#include "DataTableExporter.h"
#include "Utils/Logger.h"
#include "../../Models/MeterModel.h"
#include "../../Models/DataItem.h"
#include "../../Models/MeterPhaseType.h"
#include "../../Models/MeterModelPrivateSupport.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QDoubleValidator>
#include <QRandomGenerator>
#include <QDateTime>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>

namespace
{
    struct CategorySelection
    {
        QString rootCategory;
        QString leafCategory;
    };

    struct NumericEditSpec
    {
        double minValue = -999999.99;
        double maxValue = 999999.99;
        int decimals = 2;
    };

    struct EventCategoryDefinition
    {
        QString name;
        QStringList prefixes;
    };

    struct ParameterCategoryDefinition
    {
        QString name;
        QStringList prefixes;
    };

    QString escapeCsvField(const QString &value)
    {
        QString escaped = value;
        escaped.replace("\"", "\"\"");

        if (escaped.contains(',') || escaped.contains('"') || escaped.contains('\n') || escaped.contains('\r'))
        {
            escaped = QString("\"%1\"").arg(escaped);
        }

        return escaped;
    }

    QString formatDisplayValue(const DataItem &dataItem)
    {
        DataItem displayItem = dataItem;
        return displayItem.getFormattedValue();
    }

    CategorySelection parseCategorySelection(const QString &category)
    {
        const int separatorIndex = category.indexOf('/');
        if (separatorIndex < 0)
        {
            return {category, QString()};
        }

        return {category.left(separatorIndex), category.mid(separatorIndex + 1)};
    }

    int settlementDayFromCategoryName(const QString &categoryName)
    {
        if (categoryName == QStringLiteral("当前"))
        {
            return 0;
        }

        static const QRegularExpression settlementDayPattern(QStringLiteral("^上(\\d+)结算日$"));
        const QRegularExpressionMatch match = settlementDayPattern.match(categoryName);
        if (match.hasMatch())
        {
            return match.captured(1).toInt();
        }

        return -1;
    }

    QString displayCategoryName(const QString &category)
    {
        const CategorySelection selection = parseCategorySelection(category);
        if (selection.leafCategory.isEmpty())
        {
            return selection.rootCategory;
        }

        return QStringLiteral("%1 / %2").arg(selection.rootCategory, selection.leafCategory);
    }

    QString rateName(int rateIndex)
    {
        switch (rateIndex)
        {
        case 1:
            return QStringLiteral("尖峰");
        case 2:
            return QStringLiteral("高峰");
        case 3:
            return QStringLiteral("平");
        case 4:
            return QStringLiteral("低谷");
        case 5:
            return QStringLiteral("深谷");
        default:
            return QStringLiteral("费率%1").arg(rateIndex);
        }
    }

    bool isYearZoneTableCode(const QString &diCode)
    {
        return diCode == QStringLiteral("0x04010000") || diCode == QStringLiteral("0x04020000");
    }

    bool isDayScheduleTableCode(const QString &diCode)
    {
        return diCode.startsWith(QStringLiteral("0x040100")) || diCode.startsWith(QStringLiteral("0x040200"));
    }

    bool parseYearZoneMonthDay(const QString &value, int *monthDay)
    {
        if (value.size() < 6)
        {
            return false;
        }

        bool monthOk = false;
        bool dayOk = false;
        const int month = value.mid(0, 2).toInt(&monthOk);
        const int day = value.mid(2, 2).toInt(&dayOk);
        if (!monthOk || !dayOk || month < 1 || month > 12 || day < 1 || day > 31)
        {
            return false;
        }

        *monthDay = month * 100 + day;
        return true;
    }

    bool parseDayScheduleTime(const QString &value, int *minutes)
    {
        if (value.size() < 6)
        {
            return false;
        }

        bool hourOk = false;
        bool minuteOk = false;
        const int hour = value.mid(0, 2).toInt(&hourOk);
        const int minute = value.mid(2, 2).toInt(&minuteOk);
        if (!hourOk || !minuteOk || hour < 0 || hour > 23 || minute < 0 || minute > 59)
        {
            return false;
        }

        *minutes = hour * 60 + minute;
        return true;
    }

    bool validateDetailValues(const QString &diCode, const QStringList &values, QString *message)
    {
        if (!isYearZoneTableCode(diCode) && !isDayScheduleTableCode(diCode))
        {
            return true;
        }

        int previousKey = -1;
        for (int i = 0; i < values.size(); ++i)
        {
            int currentKey = 0;
            const bool ok = isYearZoneTableCode(diCode)
                                ? parseYearZoneMonthDay(values.at(i), &currentKey)
                                : parseDayScheduleTime(values.at(i), &currentKey);
            if (!ok)
            {
                *message = isYearZoneTableCode(diCode)
                               ? QStringLiteral("第%1项格式不正确：时区表应为 MMDDTT，且月日必须合法。").arg(i + 1)
                               : QStringLiteral("第%1项格式不正确：时段表应为 HHMMR，小时范围 00-23，分钟范围 00-59。").arg(i + 1);
                return false;
            }

            if (previousKey > currentKey)
            {
                *message = isYearZoneTableCode(diCode)
                               ? QStringLiteral("第%1项月日不能小于前一项月日。").arg(i + 1)
                               : QStringLiteral("第%1项时分不能小于前一项时分，且小时不能填写 24。").arg(i + 1);
                return false;
            }

            previousKey = currentKey;
        }

        return true;
    }

    QVariant convertEditedValue(const QString &text, const QVariant &originalValue)
    {
        bool ok = false;
        switch (originalValue.type())
        {
        case QVariant::Int:
        {
            const int value = text.toInt(&ok);
            return ok ? QVariant(value) : QVariant(text);
        }
        case QVariant::UInt:
        {
            const uint value = text.toUInt(&ok);
            return ok ? QVariant(value) : QVariant(text);
        }
        case QVariant::LongLong:
        {
            const qlonglong value = text.toLongLong(&ok);
            return ok ? QVariant(value) : QVariant(text);
        }
        case QVariant::ULongLong:
        {
            const qulonglong value = text.toULongLong(&ok);
            return ok ? QVariant(value) : QVariant(text);
        }
        case QVariant::Double:
        {
            const double value = text.toDouble(&ok);
            return ok ? QVariant(value) : QVariant(text);
        }
        default:
            return text;
        }
    }

    QVariantList editedValuesToVariantList(const QStringList &editedValues, const QVariantList &originalValues)
    {
        QVariantList convertedValues;
        convertedValues.reserve(editedValues.size());
        for (int valueIndex = 0; valueIndex < editedValues.size(); ++valueIndex)
        {
            const QVariant originalValue = valueIndex < originalValues.size() ? originalValues.at(valueIndex) : QVariant();
            convertedValues.append(convertEditedValue(editedValues.at(valueIndex), originalValue));
        }

        return convertedValues;
    }

    QString phaseEventName(const QString &diCode)
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

    QStringList totalAndPhaseEnergyNames(const QString &prefix, const QStringList &energyTypes, const QString &suffix)
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

    QStringList phaseVoltageEventFieldNames(const QString &eventName)
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

    QStringList phaseElectricalEventFieldNames(const QString &eventName)
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

    QStringList sequenceEventFieldNames(const QString &eventName)
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

    QStringList unbalanceEventFieldNames(const QString &eventName)
    {
        QStringList names = {QStringLiteral("发生时刻"),
                             QStringLiteral("结束时刻"),
                             QStringLiteral("最大不平衡率")};
        names.append(sequenceEventFieldNames(eventName).mid(2));
        return names;
    }

    QStringList meterClearFieldNames()
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

    QStringList demandClearFieldNames()
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

    QStringList coverEventFieldNames(const QString &eventName)
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

    QStringList voltageComplianceFieldNames(const QString &phaseName)
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

    QStringList scheduleTableProgramFieldNames()
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

    QStringList yearZoneProgramFieldNames()
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

    QStringList holidayProgramFieldNames()
    {
        QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("操作者代码")};
        for (int i = 1; i <= 254; ++i)
        {
            names.append(QStringLiteral("节假日编程前第%1公共假日日期及日时段表号").arg(i));
        }
        return names;
    }

    QString purchaseRecordFieldName(const QString &diCode)
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

    QString indexedName(const QStringList &names, int valueIndex)
    {
        return valueIndex >= 0 && valueIndex < names.size() ? names.at(valueIndex) : QString();
    }

    QString upperHexByte(int value)
    {
        return QStringLiteral("%1").arg(value, 2, 16, QChar('0')).toUpper();
    }

    bool isHistoryItem(const QString &diCode)
    {
        return diCode.size() >= 10 && diCode.mid(8, 2) != QStringLiteral("00");
    }

    QString eventDetailValueDescription(const QString &diCode, int valueIndex)
    {
        if (!diCode.startsWith(QStringLiteral("0x03")))
        {
            return QString();
        }

        const QString historyByte = diCode.size() >= 10 ? diCode.mid(8, 2) : QString();
        const QString thirdByte = diCode.size() >= 8 ? diCode.mid(6, 2) : QString();

        const bool isPhaseVoltageFamily = diCode.startsWith(QStringLiteral("0x0301")) ||
                                          diCode.startsWith(QStringLiteral("0x0302")) ||
                                          diCode.startsWith(QStringLiteral("0x0303")) ||
                                          diCode.startsWith(QStringLiteral("0x0304"));
        const bool isPhaseElectricalFamily = diCode.startsWith(QStringLiteral("0x030B")) ||
                                             diCode.startsWith(QStringLiteral("0x030C")) ||
                                             diCode.startsWith(QStringLiteral("0x030D"));
        const bool isPhaseSequenceFamily = diCode.startsWith(QStringLiteral("0x030E")) ||
                                           diCode.startsWith(QStringLiteral("0x030F"));

        if (isPhaseVoltageFamily &&
            thirdByte != QStringLiteral("00") && historyByte != QStringLiteral("00"))
        {
            return indexedName(phaseVoltageEventFieldNames(phaseEventName(diCode)), valueIndex);
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
            return indexedName({QStringLiteral("总次数"), QStringLiteral("总累计时间")}, valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x030500")) && isHistoryItem(diCode))
        {
            return indexedName({QStringLiteral("全失压开始时间"), QStringLiteral("全失压时电流"), QStringLiteral("全失压结束时间")}, valueIndex);
        }

        if ((diCode.startsWith(QStringLiteral("0x030600")) ||
             diCode.startsWith(QStringLiteral("0x031100"))) &&
            isHistoryItem(diCode))
        {
            return indexedName({QStringLiteral("事件开始时间"), QStringLiteral("事件结束时间")}, valueIndex);
        }

        if ((diCode.startsWith(QStringLiteral("0x030700")) ||
             diCode.startsWith(QStringLiteral("0x030800"))) &&
            isHistoryItem(diCode))
        {
            const QString eventName = diCode.startsWith(QStringLiteral("0x030700"))
                                          ? QStringLiteral("电压逆相序")
                                          : QStringLiteral("电流逆相序");
            return indexedName(sequenceEventFieldNames(eventName), valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x030900")) && isHistoryItem(diCode))
        {
            return indexedName(unbalanceEventFieldNames(QStringLiteral("电压不平衡")), valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x030A00")) && isHistoryItem(diCode))
        {
            return indexedName(unbalanceEventFieldNames(QStringLiteral("电流不平衡")), valueIndex);
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
            return indexedName(phaseElectricalEventFieldNames(eventName), valueIndex);
        }

        if (isPhaseSequenceFamily && thirdByte != QStringLiteral("00") && isHistoryItem(diCode))
        {
            const QString eventName = diCode.startsWith(QStringLiteral("0x030E"))
                                          ? QStringLiteral("潮流反向")
                                          : QStringLiteral("过载");
            return indexedName(sequenceEventFieldNames(eventName), valueIndex);
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
            return indexedName(voltageComplianceFieldNames(phaseName), valueIndex);
        }

        if (diCode == QStringLiteral("0x03120000"))
        {
            const QStringList demandLimitNames = {
                QStringLiteral("正向有功需量超限总次数"),
                QStringLiteral("反向有功需量超限总次数"),
                QStringLiteral("第一象限无功需量超限总次数"),
                QStringLiteral("第二象限无功需量超限总次数"),
                QStringLiteral("第三象限无功需量超限总次数"),
                QStringLiteral("第四象限无功需量超限总次数")};
            return indexedName(demandLimitNames, valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x0312")) && isHistoryItem(diCode))
        {
            return indexedName({QStringLiteral("发生时刻"),
                                QStringLiteral("结束时刻"),
                                QStringLiteral("需量超限期间最大需量"),
                                QStringLiteral("需量超限期间最大需量发生时间")},
                               valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x03300D")) && isHistoryItem(diCode))
        {
            return indexedName(coverEventFieldNames(QStringLiteral("开表盖")), valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x03300E")) && isHistoryItem(diCode))
        {
            return indexedName(coverEventFieldNames(QStringLiteral("开端钮盒")), valueIndex);
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
            return indexedName(names, valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x033001")) && isHistoryItem(diCode))
        {
            return indexedName(meterClearFieldNames(), valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x033002")) && isHistoryItem(diCode))
        {
            return indexedName(demandClearFieldNames(), valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x033003")) && isHistoryItem(diCode))
        {
            return indexedName({QStringLiteral("发生时刻"), QStringLiteral("操作者代码"), QStringLiteral("事件清零数据标识码")}, valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x033004")) && isHistoryItem(diCode))
        {
            return indexedName({QStringLiteral("操作者代码"), QStringLiteral("校时前时间"), QStringLiteral("校时后时间")}, valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x033005")) && isHistoryItem(diCode))
        {
            return indexedName(scheduleTableProgramFieldNames(), valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x033006")) && isHistoryItem(diCode))
        {
            return indexedName(yearZoneProgramFieldNames(), valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x033007")) && isHistoryItem(diCode))
        {
            return indexedName({QStringLiteral("发生时刻"),
                                QStringLiteral("操作者代码"),
                                QStringLiteral("周休日编程前采用的日时段表号")},
                               valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x033008")) && isHistoryItem(diCode))
        {
            return indexedName(holidayProgramFieldNames(), valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x033009")) && isHistoryItem(diCode))
        {
            return indexedName({QStringLiteral("发生时刻"), QStringLiteral("操作者代码"), QStringLiteral("有功组合方式编程前特征字")}, valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x03300A")) && isHistoryItem(diCode))
        {
            return indexedName({QStringLiteral("发生时刻"), QStringLiteral("操作者代码"), QStringLiteral("无功组合方式1编程前特征字")}, valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x03300B")) && isHistoryItem(diCode))
        {
            return indexedName({QStringLiteral("发生时刻"), QStringLiteral("操作者代码"), QStringLiteral("无功组合方式2编程前特征字")}, valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x03300C")) && isHistoryItem(diCode))
        {
            return indexedName({QStringLiteral("发生时刻"),
                                QStringLiteral("操作者代码"),
                                QStringLiteral("结算日编程前每月第1结算日"),
                                QStringLiteral("结算日编程前每月第2结算日"),
                                QStringLiteral("结算日编程前每月第3结算日")},
                               valueIndex);
        }

        if (diCode.startsWith(QStringLiteral("0x0332")) || diCode.startsWith(QStringLiteral("0x0333")))
        {
            return valueIndex == 0 ? purchaseRecordFieldName(diCode) : QString();
        }

        return QString();
    }

    QString energyBlockValueDescription(const MeterModel *meterModel, const QString &diCode, int valueIndex, int valueCount)
    {
        if (!meterModel ||
            !diCode.endsWith(QStringLiteral("FF00")) ||
            valueIndex < 0 ||
            valueIndex >= valueCount)
        {
            return QString();
        }

        const QString prefix = diCode.left(6);
        const DataItem totalItem = meterModel->getDataItem(prefix + QStringLiteral("0000"));
        if (totalItem.diCode.isEmpty() ||
            !totalItem.unit.contains(QChar('h'), Qt::CaseInsensitive) ||
            meterModel->getDataItem(prefix + QStringLiteral("0100")).diCode.isEmpty() ||
            !meterModel->getDataItem(prefix + QStringLiteral("0001")).diCode.isEmpty())
        {
            return QString();
        }

        const QString targetCode = valueIndex == 0
                                       ? prefix + QStringLiteral("0000")
                                       : prefix + upperHexByte(valueIndex) + QStringLiteral("00");
        const DataItem targetItem = meterModel->getDataItem(targetCode);
        return targetItem.diCode.isEmpty() ? QString() : targetItem.description;
    }

    QString demandBlockValueDescription(const MeterModel *meterModel, const QString &diCode, int valueIndex, int valueCount)
    {
        if (!meterModel ||
            !diCode.endsWith(QStringLiteral("FF00")) ||
            valueIndex < 0 ||
            valueIndex >= valueCount ||
            valueCount % 2 != 0)
        {
            return QString();
        }

        const QString prefix = diCode.left(6);
        if (meterModel->getDataItem(prefix + QStringLiteral("0000")).diCode.isEmpty() ||
            meterModel->getDataItem(prefix + QStringLiteral("0001")).diCode.isEmpty() ||
            meterModel->getDataItem(prefix + QStringLiteral("0100")).diCode.isEmpty() ||
            meterModel->getDataItem(prefix + QStringLiteral("0101")).diCode.isEmpty())
        {
            return QString();
        }

        const int rateCount = qMax(0, valueCount / 2 - 1);
        QString targetCode;
        if (valueIndex < 2)
        {
            const bool timeField = valueIndex == 1;
            targetCode = prefix + (timeField ? QStringLiteral("0001") : QStringLiteral("0000"));
        }
        else
        {
            const int rateIndex = (valueIndex - 2) / 2 + 1;
            if (rateIndex > rateCount)
            {
                return QString();
            }
            const bool timeField = (valueIndex % 2 == 1);
            targetCode = prefix + upperHexByte(rateIndex) + (timeField ? QStringLiteral("01") : QStringLiteral("00"));
        }

        const DataItem targetItem = meterModel->getDataItem(targetCode);
        return targetItem.diCode.isEmpty() ? QString() : targetItem.description;
    }

    QStringList variableBlockPayloadCodes(const QString &diCode)
    {
        if (diCode == QStringLiteral("0x0201FF00"))
        {
            return {QStringLiteral("0x02010100"), QStringLiteral("0x02010200"), QStringLiteral("0x02010300")};
        }
        if (diCode == QStringLiteral("0x0202FF00"))
        {
            return {QStringLiteral("0x02020100"), QStringLiteral("0x02020200"), QStringLiteral("0x02020300")};
        }
        if (diCode == QStringLiteral("0x0203FF00"))
        {
            return {QStringLiteral("0x02030000"), QStringLiteral("0x02030100"), QStringLiteral("0x02030200"), QStringLiteral("0x02030300")};
        }
        if (diCode == QStringLiteral("0x0204FF00"))
        {
            return {QStringLiteral("0x02040000"), QStringLiteral("0x02040100"), QStringLiteral("0x02040200"), QStringLiteral("0x02040300")};
        }
        if (diCode == QStringLiteral("0x0205FF00"))
        {
            return {QStringLiteral("0x02050000"), QStringLiteral("0x02050100"), QStringLiteral("0x02050200"), QStringLiteral("0x02050300")};
        }
        if (diCode == QStringLiteral("0x0206FF00"))
        {
            return {QStringLiteral("0x02060000"), QStringLiteral("0x02060100"), QStringLiteral("0x02060200"), QStringLiteral("0x02060300")};
        }
        if (diCode == QStringLiteral("0x0207FF00"))
        {
            return {QStringLiteral("0x02070100"), QStringLiteral("0x02070200"), QStringLiteral("0x02070300")};
        }
        if (diCode == QStringLiteral("0x0208FF00"))
        {
            return {QStringLiteral("0x02080100"), QStringLiteral("0x02080200"), QStringLiteral("0x02080300")};
        }
        if (diCode == QStringLiteral("0x0209FF00"))
        {
            return {QStringLiteral("0x02090100"), QStringLiteral("0x02090200"), QStringLiteral("0x02090300")};
        }
        if ((diCode.startsWith(QStringLiteral("0x020A")) || diCode.startsWith(QStringLiteral("0x020B"))) &&
            diCode.size() == 10 &&
            diCode.endsWith(QStringLiteral("FF")))
        {
            const QString prefix = diCode.left(8);
            QStringList codes;
            codes.reserve(21);
            for (int harmonicOrder = 1; harmonicOrder <= 21; ++harmonicOrder)
            {
                codes.append(prefix + upperHexByte(harmonicOrder));
            }
            return codes;
        }
        if (diCode == QStringLiteral("0x230100FF"))
        {
            return {QStringLiteral("0x23010001"),
                    QStringLiteral("0x23010002"),
                    QStringLiteral("0x23010003"),
                    QStringLiteral("0x23010004"),
                    QStringLiteral("0x23010005"),
                    QStringLiteral("0x23010006"),
                    QStringLiteral("0x23010007"),
                    QStringLiteral("0x23010008")};
        }

        return {};
    }

    QString variableBlockValueDescription(const MeterModel *meterModel, const QString &diCode, int valueIndex)
    {
        if (!meterModel || valueIndex < 0)
        {
            return QString();
        }

        const QStringList codes = variableBlockPayloadCodes(diCode);
        if (valueIndex >= codes.size())
        {
            return QString();
        }

        const DataItem targetItem = meterModel->getDataItem(codes.at(valueIndex));
        return targetItem.diCode.isEmpty() ? QString() : targetItem.description;
    }

    QString detailValueDescription(const MeterModel *meterModel, const QString &diCode, const QString &value, int valueIndex, int valueCount)
    {
        const QString energyDescription = energyBlockValueDescription(meterModel, diCode, valueIndex, valueCount);
        if (!energyDescription.isEmpty())
        {
            return energyDescription;
        }

        const QString demandDescription = demandBlockValueDescription(meterModel, diCode, valueIndex, valueCount);
        if (!demandDescription.isEmpty())
        {
            return demandDescription;
        }

        const QString variableDescription = variableBlockValueDescription(meterModel, diCode, valueIndex);
        if (!variableDescription.isEmpty())
        {
            return variableDescription;
        }

        const QString eventDescription = eventDetailValueDescription(diCode, valueIndex);
        if (!eventDescription.isEmpty())
        {
            return eventDescription;
        }

        bool monthOk = false;
        bool dayOk = false;
        bool indexOk = false;

        if (isYearZoneTableCode(diCode) && value.size() >= 6)
        {
            const int month = value.mid(0, 2).toInt(&monthOk);
            const int day = value.mid(2, 2).toInt(&dayOk);
            const int tableIndex = value.mid(4, 2).toInt(&indexOk);
            if (monthOk && dayOk && indexOk)
            {
                return QStringLiteral("%1月%2日起使用第%3日时段表").arg(month).arg(day).arg(tableIndex);
            }
        }

        if (isDayScheduleTableCode(diCode) && value.size() >= 6)
        {
            const int hour = value.mid(0, 2).toInt(&monthOk);
            const int minute = value.mid(2, 2).toInt(&dayOk);
            const int rateIndex = value.mid(4, 2).toInt(&indexOk);
            if (monthOk && dayOk && indexOk)
            {
                return QStringLiteral("%1:%2起执行%3").arg(hour, 2, 10, QChar('0')).arg(minute, 2, 10, QChar('0')).arg(rateName(rateIndex));
            }
        }

        return QString();
    }

    QList<EventCategoryDefinition> eventCategoryDefinitions()
    {
        return {
            {QStringLiteral("失压"), {QStringLiteral("0x0301")}},
            {QStringLiteral("欠压"), {QStringLiteral("0x0302")}},
            {QStringLiteral("过压"), {QStringLiteral("0x0303")}},
            {QStringLiteral("断相"), {QStringLiteral("0x0304")}},
            {QStringLiteral("全失压"), {QStringLiteral("0x0305")}},
            {QStringLiteral("辅助电源失电"), {QStringLiteral("0x0306")}},
            {QStringLiteral("电压逆相序"), {QStringLiteral("0x0307")}},
            {QStringLiteral("电流逆相序"), {QStringLiteral("0x0308")}},
            {QStringLiteral("电压不平衡"), {QStringLiteral("0x0309")}},
            {QStringLiteral("电流不平衡"), {QStringLiteral("0x030A")}},
            {QStringLiteral("失流"), {QStringLiteral("0x030B")}},
            {QStringLiteral("过流"), {QStringLiteral("0x030C")}},
            {QStringLiteral("断流"), {QStringLiteral("0x030D")}},
            {QStringLiteral("潮流反向"), {QStringLiteral("0x030E")}},
            {QStringLiteral("过载"), {QStringLiteral("0x030F")}},
            {QStringLiteral("电压合格率"), {QStringLiteral("0x0310")}},
            {QStringLiteral("掉电"), {QStringLiteral("0x0311")}},
            {QStringLiteral("需量超限"), {QStringLiteral("0x0312")}},
            {QStringLiteral("编程记录"), {QStringLiteral("0x033000")}},
            {QStringLiteral("电表清零"), {QStringLiteral("0x033001")}},
            {QStringLiteral("需量清零"), {QStringLiteral("0x033002")}},
            {QStringLiteral("事件清零"), {QStringLiteral("0x033003")}},
            {QStringLiteral("校时"), {QStringLiteral("0x033004")}},
            {QStringLiteral("时段表编程"), {QStringLiteral("0x033005")}},
            {QStringLiteral("时区表编程"), {QStringLiteral("0x033006")}},
            {QStringLiteral("周休日编程"), {QStringLiteral("0x033007")}},
            {QStringLiteral("节假日编程"), {QStringLiteral("0x033008")}},
            {QStringLiteral("有功组合方式编程"), {QStringLiteral("0x033009")}},
            {QStringLiteral("无功组合方式1编程"), {QStringLiteral("0x03300A")}},
            {QStringLiteral("无功组合方式2编程"), {QStringLiteral("0x03300B")}},
            {QStringLiteral("结算日编程"), {QStringLiteral("0x03300C")}},
            {QStringLiteral("开表盖"), {QStringLiteral("0x03300D")}},
            {QStringLiteral("开端钮盒"), {QStringLiteral("0x03300E")}},
            {QStringLiteral("购电量记录"), {QStringLiteral("0x0332")}},
            {QStringLiteral("购电金额记录"), {QStringLiteral("0x0333")}}};
    }

    QList<QString> filterEventCodesByCategory(const MeterModel *meterModel, const QList<QString> &allCodes, const QString &categoryName)
    {
        const QString normalizedCategoryName = categoryName.trimmed();
        QStringList matchedPrefixes;
        const QList<EventCategoryDefinition> definitions = eventCategoryDefinitions();
        for (const EventCategoryDefinition &definition : definitions)
        {
            if (definition.name == normalizedCategoryName)
            {
                matchedPrefixes = definition.prefixes;
                break;
            }
        }

        if (matchedPrefixes.isEmpty())
        {
            return allCodes;
        }

        QList<QString> filteredCodes;
        filteredCodes.reserve(allCodes.size());
        for (const QString &diCode : allCodes)
        {
            for (const QString &prefix : matchedPrefixes)
            {
                if (diCode.startsWith(prefix))
                {
                    filteredCodes.append(diCode);
                    break;
                }
            }
        }

        if (!filteredCodes.isEmpty())
        {
            return filteredCodes;
        }

        // 前缀映射未命中时，退回到按描述关键字过滤，避免树节点文案差异导致空结果。
        if (!meterModel)
        {
            return {};
        }

        for (const QString &diCode : allCodes)
        {
            const DataItem dataItem = meterModel->getDataItem(diCode);
            if (dataItem.description.contains(normalizedCategoryName, Qt::CaseInsensitive))
            {
                filteredCodes.append(diCode);
            }
        }

        return filteredCodes;
    }

    QList<ParameterCategoryDefinition> parameterCategoryDefinitions()
    {
        return {
            {QStringLiteral("基本参数"), {QStringLiteral("0x0400")}},
            {QStringLiteral("第一套时区时段"), {QStringLiteral("0x0401")}},
            {QStringLiteral("第二套时区时段"), {QStringLiteral("0x0402")}},
            {QStringLiteral("公共假日"), {QStringLiteral("0x0403")}},
            {QStringLiteral("显示参数"), {QStringLiteral("0x0404")}},
            {QStringLiteral("费率电价"), {QStringLiteral("0x040501"), QStringLiteral("0x040502")}},
            {QStringLiteral("阶梯电价"), {QStringLiteral("0x040600"), QStringLiteral("0x040601"), QStringLiteral("0x040602"), QStringLiteral("0x040603")}},
            {QStringLiteral("事件参数"), {QStringLiteral("0x0409")}}};
    }

    QList<QString> filterParameterCodesByCategory(const QList<QString> &allCodes, const QString &categoryName)
    {
        const QString normalizedCategoryName = categoryName.trimmed();
        QStringList matchedPrefixes;
        const QList<ParameterCategoryDefinition> definitions = parameterCategoryDefinitions();
        for (const ParameterCategoryDefinition &definition : definitions)
        {
            if (definition.name == normalizedCategoryName)
            {
                matchedPrefixes = definition.prefixes;
                break;
            }
        }

        if (matchedPrefixes.isEmpty())
        {
            return allCodes;
        }

        QList<QString> filteredCodes;
        filteredCodes.reserve(allCodes.size());
        for (const QString &diCode : allCodes)
        {
            for (const QString &prefix : matchedPrefixes)
            {
                if (diCode.startsWith(prefix))
                {
                    filteredCodes.append(diCode);
                    break;
                }
            }
        }

        return filteredCodes;
    }

    NumericEditSpec editSpecForFormat(const QString &format)
    {
        NumericEditSpec spec;
        const QStringList tokens = format.split(',', Qt::SkipEmptyParts);
        if (tokens.isEmpty())
        {
            return spec;
        }

        int decimalIndex = -1;
        for (int i = 0; i < tokens.size(); ++i)
        {
            if (tokens[i].trimmed() == QStringLiteral("."))
            {
                decimalIndex = i;
                break;
            }
        }

        if (decimalIndex >= 0)
        {
            int decimalDigits = 0;
            for (int i = decimalIndex + 1; i < tokens.size(); ++i)
            {
                if (tokens[i].trimmed() != QStringLiteral("."))
                {
                    decimalDigits += 2;
                }
            }
            spec.decimals = qMax(0, decimalDigits);
        }
        else
        {
            const int xxIndex = tokens.indexOf(QStringLiteral("xx"));
            if (xxIndex >= 0)
            {
                int digitsAfterXx = 0;
                for (int i = xxIndex + 1; i < tokens.size(); ++i)
                {
                    if (tokens[i].trimmed() != QStringLiteral("."))
                    {
                        digitsAfterXx += 2;
                    }
                }
                spec.decimals = digitsAfterXx + 1;
            }
        }

        return spec;
    }
}
// ==================== NumericDelegate ====================

NumericDelegate::NumericDelegate(QObject *parent)
    : QStyledItemDelegate(parent), m_minValue(-999999.99), m_maxValue(999999.99), m_decimals(2)
{
}

QWidget *NumericDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                       const QModelIndex &index) const
{
    Q_UNUSED(option);
    QLineEdit *editor = new QLineEdit(parent);
    NumericEditSpec spec;
    const QModelIndex formatIndex = index.sibling(index.row(), 3);
    if (formatIndex.isValid())
    {
        spec = editSpecForFormat(formatIndex.data(Qt::DisplayRole).toString());
    }

    QDoubleValidator *validator = new QDoubleValidator(spec.minValue, spec.maxValue, spec.decimals, editor);
    validator->setNotation(QDoubleValidator::StandardNotation);
    editor->setValidator(validator);
    return editor;
}

void NumericDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QString value = index.model()->data(index, Qt::EditRole).toString();
    QLineEdit *lineEdit = static_cast<QLineEdit *>(editor);
    lineEdit->setText(value);
}

void NumericDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
    QLineEdit *lineEdit = static_cast<QLineEdit *>(editor);
    QString value = lineEdit->text();

    if (!lineEdit->hasAcceptableInput())
    {
        return;
    }

    model->setData(index, value, Qt::EditRole);
}

void NumericDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                           const QModelIndex &index) const
{
    Q_UNUSED(index);
    editor->setGeometry(option.rect);
}

void NumericDelegate::setValidatorRange(double min, double max, int decimals)
{
    m_minValue = min;
    m_maxValue = max;
    m_decimals = decimals;
}

bool NumericDelegate::validateNumericValue(const QString &value) const
{
    bool ok;
    double num = value.toDouble(&ok);
    return ok && num >= m_minValue && num <= m_maxValue;
}

// ==================== DataTableWidget ====================

DataTableWidget::DataTableWidget(QWidget *parent)
    : QWidget(parent), m_meterModel(nullptr), m_exporter(nullptr), m_randomizeCurrentButton(nullptr), m_currentCategory(""), m_meterAlias(""), m_meterAddress("")
{
    setupUI();

    // 初始化导出服务
    m_exporter = new DataTableExporter(m_tableWidget);

    // 设置数值编辑代理
    m_tableWidget->setItemDelegateForColumn(1, m_numericDelegate);
}

DataTableWidget::~DataTableWidget()
{
    delete m_exporter;
}

void DataTableWidget::setMeterModel(MeterModel *meterModel)
{
    if (m_meterModel)
    {
        disconnect(m_meterModel, &MeterModel::dataChanged,
                   this, &DataTableWidget::onMeterDataChanged);
        disconnect(m_meterModel, &MeterModel::dataBatchChanged,
                   this, &DataTableWidget::onMeterBatchChanged);
    }

    m_meterModel = meterModel;

    // 连接数据变化信号，当数据改变时重新刷新表格
    if (m_meterModel)
    {
        connect(m_meterModel, &MeterModel::dataChanged,
                this, &DataTableWidget::onMeterDataChanged, Qt::UniqueConnection);
        connect(m_meterModel, &MeterModel::dataBatchChanged,
                this, &DataTableWidget::onMeterBatchChanged, Qt::UniqueConnection);
    }

    updateExportButtonState();
    refreshTable();
}

void DataTableWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // 创建电表信息标签
    m_meterInfoLabel = new QLabel(this);
    m_meterInfoLabel->setText("Meter Info");
    mainLayout->addWidget(m_meterInfoLabel);

    // 添加间距
    mainLayout->addSpacing(10);

    // 顶部工具栏
    QHBoxLayout *toolBarLayout = new QHBoxLayout();
    toolBarLayout->setSpacing(5);

    // 创建搜索框
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("搜索数据...");
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &DataTableWidget::onSearchTextChanged);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &DataTableWidget::onSearchReturnPressed);
    toolBarLayout->addWidget(m_searchEdit);

    toolBarLayout->addStretch();

    // 创建重置按钮
    m_resetButton = new QPushButton("重置所有数据", this);
    m_resetButton->setMinimumWidth(120);
    connect(m_resetButton, &QPushButton::clicked, this, &DataTableWidget::onResetClicked);
    toolBarLayout->addWidget(m_resetButton);

    // 创建随机化按钮
    m_randomizeButton = new QPushButton("随机化所有数据", this);
    m_randomizeButton->setMinimumWidth(120);
    connect(m_randomizeButton, &QPushButton::clicked, this, &DataTableWidget::onRandomizeClicked);
    toolBarLayout->addWidget(m_randomizeButton);
    m_randomizeCurrentButton = new QPushButton("随机化当前数据", this);
    m_randomizeCurrentButton->setMinimumWidth(120);
    connect(m_randomizeCurrentButton, &QPushButton::clicked, this, &DataTableWidget::onRandomizeCurrentClicked);
    toolBarLayout->addWidget(m_randomizeCurrentButton);

    // 创建导出按钮
    m_exportButton = new QPushButton("导出", this);
    m_exportButton->setMinimumWidth(120);
    connect(m_exportButton, &QPushButton::clicked, this, &DataTableWidget::onExportClicked);
    toolBarLayout->addWidget(m_exportButton);

    mainLayout->addLayout(toolBarLayout);

    // 创建格式类型提示标签
    m_formatHintLabel = new QLabel(this);
    m_formatHintLabel->setWordWrap(true);
    m_formatHintLabel->setText(
        "格式类型提示：\n"
        "• AA=直接转换为字符显示\n"
        "• AS=转 ASCII 字符显示\n"
        "• xx=除以 10 再转换为字符显示，即在两数之间加小数点\n"
        "• F9=大于等于 80 是负数，90 是 -10，F9 是 -79，用于电能的正负表示\n"
        "• bin=后续是表名，键值与帧中数据是一样的不用转换，当成键值去查就行了，bin 格式，只取一个结果\n"
        "• bit=直接转字符，再转 2 进制，再从控制表中查，列出 bit 中所有类型");
    mainLayout->addWidget(m_formatHintLabel);

    // 创建数据项个数统计标签
    m_dataCountLabel = new QLabel(this);
    m_dataCountLabel->setText("数据项个数: 0");
    m_dataCountLabel->setAlignment(Qt::AlignRight);
    mainLayout->addWidget(m_dataCountLabel);

    // 创建表格控件
    m_tableWidget = new QTableWidget(this);
    setupTable();
    mainLayout->addWidget(m_tableWidget);

    // 创建数值编辑代理
    m_numericDelegate = new NumericDelegate(this);
    updateExportButtonState();
}

void DataTableWidget::setupTable()
{
    // 设置表格列（6列）
    m_tableWidget->setColumnCount(5);
    m_tableWidget->setHorizontalHeaderLabels(QStringList()
                                             << "数据标识" << "数据值" << "单位" << "数据格式" << "数据项描述");

    // 设置表格属性
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setEditTriggers(QAbstractItemView::EditKeyPressed);
    m_tableWidget->setAlternatingRowColors(false);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);

    // 设置列宽
    m_tableWidget->setColumnWidth(0, 120); // 数据标识
    m_tableWidget->setColumnWidth(1, 150); // 数据值
    m_tableWidget->setColumnWidth(2, 80);  // 单位
    m_tableWidget->setColumnWidth(3, 120); // 数据格式

    // 连接信号
    connect(m_tableWidget, &QTableWidget::itemClicked, this, &DataTableWidget::onItemClicked);
    connect(m_tableWidget, &QTableWidget::itemDoubleClicked, this, &DataTableWidget::onItemDoubleClicked);
    connect(m_tableWidget, &QTableWidget::itemChanged, this, &DataTableWidget::onItemChanged);
    connect(m_tableWidget, &QTableWidget::customContextMenuRequested,
            this, &DataTableWidget::onCustomContextMenuRequested);
    m_tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
}

void DataTableWidget::populateCategory(const QString &category)
{
    QSignalBlocker blocker(m_tableWidget);
    m_tableWidget->setRowCount(0);

    if (!m_meterModel)
    {
        updateDataCount();
        return;
    }

    const QString filterText = m_searchEdit ? m_searchEdit->text().trimmed() : QString();
    const bool hasSearchFilter = !filterText.isEmpty();
    const QList<QString> codes = dataCodesForCategory(category);

    for (const QString &diCode : codes)
    {
        // 跳过当前配置下不支持的数据项（含费率数过滤、单相表过滤、事件记录开关等）
        if (!m_meterModel->isDataItemSupported(diCode))
        {
            continue;
        }

        // 从MeterModel中获取完整的DataItem信息
        DataItem dataItem = m_meterModel->getDataItem(diCode);
        QString name = dataItem.description;
        dataItem.value = m_meterModel->getDataValue(diCode);
        QString unit = dataItem.unit;
        QString dataFormat = dataItem.format;
        QString formattedValue = formatDisplayValue(dataItem);

        if (hasSearchFilter)
        {
            const QString haystack = QStringLiteral("%1 %2 %3 %4 %5")
                                         .arg(diCode, name, formattedValue, unit, dataFormat);
            if (!haystack.contains(filterText, Qt::CaseInsensitive))
            {
                continue;
            }
        }

        int row = m_tableWidget->rowCount();
        m_tableWidget->insertRow(row);

        // 数据标识
        QTableWidgetItem *diItem = new QTableWidgetItem(diCode);
        diItem->setFlags(diItem->flags() & ~Qt::ItemIsEditable);
        diItem->setData(Qt::UserRole, diCode);
        m_tableWidget->setItem(row, 0, diItem);

        QTableWidgetItem *valueItem = new QTableWidgetItem(formattedValue);
        valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueItem->setData(Qt::UserRole, dataItem.value);
        m_tableWidget->setItem(row, 1, valueItem);

        // 单位
        QTableWidgetItem *unitItem = new QTableWidgetItem(unit);
        unitItem->setFlags(unitItem->flags() & ~Qt::ItemIsEditable);
        unitItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(row, 2, unitItem);

        // 数据格式
        QTableWidgetItem *formatItem = new QTableWidgetItem(dataFormat);
        formatItem->setFlags(formatItem->flags() & ~Qt::ItemIsEditable);
        formatItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(row, 3, formatItem);

        // 数据项描述
        QTableWidgetItem *descItem = new QTableWidgetItem(name);
        descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
        m_tableWidget->setItem(row, 4, descItem);
    }
}

QList<QString> DataTableWidget::dataCodesForCategory(const QString &category) const
{
    if (!m_meterModel)
    {
        return {};
    }

    QList<QString> codes;
    const CategorySelection selection = parseCategorySelection(category);
    const QString rootCategory = selection.rootCategory;
    const QString leafCategory = selection.leafCategory;
    const bool hasRootCategory = !leafCategory.isEmpty();

    if ((hasRootCategory && rootCategory == "电能量") || category == "当前")
    {
        const int settlementDay = hasRootCategory ? settlementDayFromCategoryName(leafCategory)
                                                  : settlementDayFromCategoryName(category);
        if (settlementDay == 0)
        {
            codes.append(m_meterModel->getEnergyDataCodes());
        }
        else if (settlementDay > 0)
        {
            codes.append(m_meterModel->getSettlementDayEnergyDataCodes(settlementDay));
        }
    }
    else
    {
        const int settlementDay = hasRootCategory ? settlementDayFromCategoryName(leafCategory)
                                                  : settlementDayFromCategoryName(category);
        if (!hasRootCategory && settlementDay > 0)
        {
            codes.append(m_meterModel->getSettlementDayEnergyDataCodes(settlementDay));
            return codes;
        }
    }

    if ((hasRootCategory && rootCategory == "最大需量及发生时间") || category == "最大需量及发生时间")
    {
        if (hasRootCategory)
        {
            const int settlementDay = settlementDayFromCategoryName(leafCategory);
            if (settlementDay >= 0)
            {
                codes.append(m_meterModel->getSettlementDayDemandDataCodes(settlementDay));
            }
        }
        else
        {
            codes.append(m_meterModel->getDemandDataCodes());
            codes.append(m_meterModel->getMaxDemandDataCodes());
        }
    }
    else if (category == "变量")
    {
        codes.append(m_meterModel->getVariableDataCodes());
    }
    else if ((hasRootCategory && rootCategory == "事件记录") || category == "事件记录")
    {
        const QList<QString> eventCodes = m_meterModel->getEventDataCodes();
        if (hasRootCategory)
        {
            codes.append(filterEventCodesByCategory(m_meterModel, eventCodes, leafCategory));
        }
        else
        {
            codes.append(eventCodes);
        }
    }
    else if ((hasRootCategory && rootCategory == "参变量") || category == "参变量")
    {
        const QList<QString> parameterCodes = m_meterModel->getParameterDataCodes();
        if (hasRootCategory)
        {
            codes.append(filterParameterCodesByCategory(parameterCodes, leafCategory));
        }
        else
        {
            codes.append(parameterCodes);
        }
    }
    else if (category == "日冻结")
    {
        codes.append(m_meterModel->getFreezeDataCodes(category));
    }
    else if (category == "定时冻结" ||
             category == "瞬时冻结" ||
             category == "时区切换冻结" ||
             category == "日时段切换冻结" ||
             category == "整点冻结" ||
             category == "分时费率切换冻结" ||
             category == "阶梯切换冻结")
    {
        codes.append(m_meterModel->getFreezeDataCodes(category));
    }
    else if (category == "负荷记录")
    {
        codes.append(m_meterModel->getLoadRecordDataCodes());
    }

    QList<QString> uniqueCodes;
    QSet<QString> seenCodes;
    uniqueCodes.reserve(codes.size());
    for (const QString &diCode : codes)
    {
        if (seenCodes.contains(diCode))
        {
            continue;
        }

        seenCodes.insert(diCode);
        uniqueCodes.append(diCode);
    }

    return uniqueCodes;
}

void DataTableWidget::refreshTable()
{
    populateCategory(m_currentCategory);
    updateDataCount();
    updateExportButtonState();
}

void DataTableWidget::setCategory(const QString &category)
{
    m_currentCategory = category;
    populateCategory(category);
    updateDataCount();
    updateExportButtonState();
}

void DataTableWidget::onCategorySelected(const QString &categoryName, const QString &meterAlias)
{
    Q_UNUSED(meterAlias);
    setCategory(categoryName);
}

QString DataTableWidget::getCategory() const
{
    return m_currentCategory;
}

void DataTableWidget::setMeterInfo(const QString &alias, const QString &address, MeterPhaseType phaseType)
{
    m_meterAlias = alias;
    m_meterAddress = address;
    m_meterPhaseType = phaseType;

    // 更新导出器的电表信息
    if (m_exporter)
    {
        m_exporter->setMeterInfo(m_meterAlias, m_meterAddress, m_currentCategory);
    }

    updateExportButtonState();
    if (alias.isEmpty() && address.isEmpty())
    {
        m_meterInfoLabel->setText("电表信息");
    }
    else
    {
        QString phaseTypeText = meterPhaseTypeToDisplayName(phaseType);
        m_meterInfoLabel->setText(QString("电表信息：[%1] [%2] [%3]")
                                      .arg(alias, address, phaseTypeText));
    }
}

bool DataTableWidget::updateDataItem(const QString &diCode, const QVariant &value)
{
    for (int i = 0; i < m_tableWidget->rowCount(); ++i)
    {
        QTableWidgetItem *diItem = m_tableWidget->item(i, 0);
        if (diItem && diItem->data(Qt::UserRole).toString() == diCode)
        {
            QTableWidgetItem *valueItem = m_tableWidget->item(i, 1);
            if (valueItem)
            {
                QSignalBlocker blocker(m_tableWidget);
                QString formattedValue = value.toString();
                if (m_meterModel)
                {
                    DataItem dataItem = m_meterModel->getDataItem(diCode);
                    if (!dataItem.diCode.isEmpty())
                    {
                        dataItem.value = value;
                        formattedValue = formatDisplayValue(dataItem);
                    }
                }

                valueItem->setText(formattedValue);
                valueItem->setData(Qt::UserRole, value);
            }
            return true;
        }
    }

    return false;
}

void DataTableWidget::clearTable()
{
    m_tableWidget->setRowCount(0);
    updateDataCount();
}

void DataTableWidget::updateDataCount()
{
    int count = m_tableWidget->rowCount();
    m_dataCountLabel->setText(QString("数据项个数: %1").arg(count));
}

void DataTableWidget::addDataItem(const QString &diCode, const QString &name, const QVariant &value, const QString &unit, const QString &format)
{
    int row = m_tableWidget->rowCount();
    m_tableWidget->insertRow(row);

    // 数据标识
    QTableWidgetItem *diItem = new QTableWidgetItem(diCode);
    diItem->setFlags(diItem->flags() & ~Qt::ItemIsEditable);
    diItem->setData(Qt::UserRole, diCode);
    m_tableWidget->setItem(row, 0, diItem);

    // 创建临时DataItem来格式化值
    QString dataFormat = format.isEmpty() ? "error" : format;

    // 将QVariant值转换为QString，多个值用逗号隔开
    QString formattedValue;
    if (value.type() == QVariant::List)
    {
        QVariantList values = value.toList();
        QStringList valueStrings;
        for (const QVariant &val : values)
        {
            valueStrings.append(val.toString());
        }
        formattedValue = valueStrings.join(", ");
    }
    else
    {
        formattedValue = value.toString();
    }

    // 数据值

    QTableWidgetItem *valueItem = new QTableWidgetItem(formattedValue);
    valueItem->setTextAlignment(Qt::AlignCenter);
    valueItem->setData(Qt::UserRole, value);
    m_tableWidget->setItem(row, 1, valueItem);

    // 单位
    QTableWidgetItem *unitItem = new QTableWidgetItem(unit);
    unitItem->setFlags(unitItem->flags() & ~Qt::ItemIsEditable);
    unitItem->setTextAlignment(Qt::AlignCenter);
    m_tableWidget->setItem(row, 2, unitItem);

    // 数据格式 - 显示格式字符串
    QTableWidgetItem *formatItem = new QTableWidgetItem(format);
    formatItem->setFlags(formatItem->flags() & ~Qt::ItemIsEditable);
    formatItem->setTextAlignment(Qt::AlignCenter);
    m_tableWidget->setItem(row, 3, formatItem);

    // 数据项描述
    QTableWidgetItem *descItem = new QTableWidgetItem(name);
    descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
    m_tableWidget->setItem(row, 4, descItem);

    // 更新数据项个数统计
    updateDataCount();
}

void DataTableWidget::onItemClicked(QTableWidgetItem *item)
{
    if (!item)
        return;

    int row = item->row();
    QTableWidgetItem *diItem = m_tableWidget->item(row, 0);
    if (diItem)
    {
        QString diCode = diItem->data(Qt::UserRole).toString();
        emit dataItemSelected(diCode);
    }
}

bool DataTableWidget::showValueDetailDialog(QTableWidgetItem *item)
{
    if (!item || item->column() != 1)
    {
        return false;
    }

    const int row = item->row();
    const QString valueText = item->text();
    const QStringList values = valueText.split(QRegularExpression(QStringLiteral("\\s*[:：]\\s*")), Qt::KeepEmptyParts);
    const QVariantList originalValues = item->data(Qt::UserRole).toList();
    const QString separator = valueText.contains(QStringLiteral("：")) && !valueText.contains(QStringLiteral(":"))
                                  ? QStringLiteral("：")
                                  : QStringLiteral(":");

    if (values.size() <= 1)
    {
        return false;
    }

    QTableWidgetItem *diItem = m_tableWidget->item(row, 0);
    QTableWidgetItem *descItem = m_tableWidget->item(row, 4);
    const QString diCode = diItem ? diItem->data(Qt::UserRole).toString() : QString();
    const QString description = descItem ? descItem->text() : QString();

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("数据值明细"));
    dialog.resize(460, qMin(560, 190 + values.size() * 30));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *titleLabel = new QLabel(QStringLiteral("数据标识：%1\n数据项：%2").arg(diCode, description), &dialog);
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    QLabel *hintLabel = new QLabel(QStringLiteral("提示：双击下方“数据值”单元格可直接修改，确定后写回原数据值。"), &dialog);
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    QTableWidget *detailTable = new QTableWidget(values.size(), 3, &dialog);
    detailTable->setHorizontalHeaderLabels(QStringList() << QStringLiteral("序号") << QStringLiteral("数据值") << QStringLiteral("说明"));
    detailTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    detailTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    detailTable->verticalHeader()->setVisible(false);
    detailTable->horizontalHeader()->setStretchLastSection(true);
    detailTable->setColumnWidth(0, 70);
    detailTable->setColumnWidth(1, 140);

    for (int i = 0; i < values.size(); ++i)
    {
        QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(i + 1));
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        indexItem->setTextAlignment(Qt::AlignCenter);
        detailTable->setItem(i, 0, indexItem);

        QTableWidgetItem *valueItem = new QTableWidgetItem(values.at(i));
        valueItem->setTextAlignment(Qt::AlignCenter);
        detailTable->setItem(i, 1, valueItem);

        QTableWidgetItem *descriptionItem = new QTableWidgetItem(detailValueDescription(m_meterModel, diCode, values.at(i), i, values.size()));
        descriptionItem->setFlags(descriptionItem->flags() & ~Qt::ItemIsEditable);
        detailTable->setItem(i, 2, descriptionItem);
    }

    connect(detailTable, &QTableWidget::itemChanged, detailTable, [detailTable, meterModel = m_meterModel, diCode](QTableWidgetItem *changedItem) {
        if (!changedItem || changedItem->column() != 1)
        {
            return;
        }

        QTableWidgetItem *descriptionItem = detailTable->item(changedItem->row(), 2);
        if (descriptionItem)
        {
            descriptionItem->setText(detailValueDescription(meterModel, diCode, changedItem->text(), changedItem->row(), detailTable->rowCount()));
        }
    });

    layout->addWidget(detailTable);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    QVariantList acceptedValue;
    bool hasAcceptedValue = false;

    while (dialog.exec() == QDialog::Accepted)
    {
        QStringList editedValues;
        editedValues.reserve(detailTable->rowCount());
        for (int i = 0; i < detailTable->rowCount(); ++i)
        {
            QTableWidgetItem *valueItem = detailTable->item(i, 1);
            editedValues.append(valueItem ? valueItem->text() : QString());
        }

        QString validationMessage;
        if (!validateDetailValues(diCode, editedValues, &validationMessage))
        {
            QMessageBox::warning(&dialog, QStringLiteral("数据值格式不正确"), validationMessage);
            continue;
        }

        acceptedValue = editedValuesToVariantList(editedValues, originalValues);
        hasAcceptedValue = true;
        break;
    }

    if (hasAcceptedValue)
    {
        if (m_meterModel)
        {
            QTimer::singleShot(0, this, [this, diCode, acceptedValue]() {
                if (!m_meterModel)
                {
                    return;
                }

                m_meterModel->setDataValue(diCode, acceptedValue);
                emit dataItemEdited(diCode, acceptedValue);
            });
        }
        else
        {
            QSignalBlocker blocker(m_tableWidget);
            item->setData(Qt::UserRole, acceptedValue);
            QStringList displayValues;
            displayValues.reserve(acceptedValue.size());
            for (const QVariant &value : acceptedValue)
            {
                displayValues.append(value.toString());
            }
            item->setText(displayValues.join(separator));
        }
    }

    return true;
}

void DataTableWidget::onItemDoubleClicked(QTableWidgetItem *item)
{
    if (!item)
        return;

    if (item->column() == 1)
    {
        if (showValueDetailDialog(item))
        {
            return;
        }

        m_tableWidget->editItem(item);
        return;
    }

    int row = item->row();
    QTableWidgetItem *diItem = m_tableWidget->item(row, 0);
    if (diItem)
    {
        QString diCode = diItem->data(Qt::UserRole).toString();
        emit dataItemSelected(diCode);
    }
}

void DataTableWidget::onItemChanged(QTableWidgetItem *item)
{
    if (!item || item->column() != 1 || !m_meterModel)
        return;

    int row = item->row();
    QTableWidgetItem *diItem = m_tableWidget->item(row, 0);
    if (diItem)
    {
        QString diCode = diItem->data(Qt::UserRole).toString();
        const DataItem currentItem = m_meterModel->getDataItem(diCode);
        QVariant newValue = item->text();
        bool ok = false;
        const double numericValue = item->text().toDouble(&ok);
        if (ok)
        {
            if (currentItem.value.type() == QVariant::Int)
            {
                newValue = static_cast<int>(numericValue);
            }
            else if (currentItem.value.type() == QVariant::Double)
            {
                newValue = numericValue;
            }
        }

        m_meterModel->setDataValue(diCode, newValue);
        emit dataItemEdited(diCode, newValue);
    }
}

void DataTableWidget::onSearchTextChanged(const QString &text)
{
    Q_UNUSED(text);
    refreshTable();
}

void DataTableWidget::onSearchReturnPressed()
{
    // 回车键触发搜索（与textChanged相同）
    refreshTable();
}

void DataTableWidget::onRefreshClicked()
{
    refreshTable();
}

void DataTableWidget::onResetClicked()
{
    if (!m_meterModel)
    {
        QMessageBox::information(this, "Select Meter", "Please select a meter first.");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "确认重置",
        "确定要将所有数据重置为默认值吗？",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        m_meterModel->resetAllData();
        LOG_INFO("所有数据已重置为默认值");
    }
}

void DataTableWidget::onRandomizeClicked()
{
    if (!m_meterModel)
    {
        QMessageBox::information(this, "Select Meter", "Please select a meter first.");
        return;
    }

    m_meterModel->randomizeData();
    LOG_INFO("所有数据已随机化");
}

void DataTableWidget::onRandomizeCurrentClicked()
{
    if (!m_meterModel)
    {
        QMessageBox::information(this, "Select Meter", "Please select a meter first.");
        return;
    }

    const QList<QString> diCodes = currentTableDiCodes();
    if (diCodes.isEmpty())
    {
        QMessageBox::information(this, "提示", "当前没有可随机化的数据项。");
        return;
    }

    m_meterModel->randomizeData(diCodes);
    LOG_INFO(QString("当前数据列表已随机化，共 %1 项").arg(diCodes.size()));
}

void DataTableWidget::onExportClicked()
{
    if (!m_meterModel)
    {
        QMessageBox::information(this, "Select Meter", "Please select a meter first.");
        return;
    }

    // 更新导出器信息（确保最新）
    if (m_exporter)
    {
        m_exporter->setMeterInfo(m_meterAlias, m_meterAddress, m_currentCategory);
    }

    const QString meterAddress = selectedMeterAddress();
    const QString defaultFileName = meterAddress.isEmpty()
                                        ? QStringLiteral("meter-data.csv")
                                        : QStringLiteral("meter-data-%1.csv").arg(meterAddress);

    QString fileName = QFileDialog::getSaveFileName(this, "导出数据",
                                                    QDir::homePath() + "/" + defaultFileName,
                                                    "CSV文件 (*.csv);;Excel文件 (*.xlsx);;JSON文件 (*.json)");

    if (fileName.isEmpty())
    {
        return;
    }

    QFileInfo fileInfo(fileName);
    QString suffix = fileInfo.suffix().toLower();

    bool success = false;
    if (suffix == "csv")
    {
        success = m_exporter->exportToCSV(fileName);
    }
    else if (suffix == "xlsx")
    {
        success = m_exporter->exportToExcel(fileName);
    }
    else if (suffix == "json")
    {
        success = m_exporter->exportToJSON(fileName);
    }
    else
    {
        // 默认CSV
        success = m_exporter->exportToCSV(fileName);
    }

    if (success)
    {
        LOG_INFO("数据导出成功");
    }
    else
    {
        LOG_ERROR("数据导出失败");
    }
}

void DataTableWidget::onCustomContextMenuRequested(const QPoint &pos)
{
    QModelIndex index = m_tableWidget->indexAt(pos);
    if (!index.isValid())
        return;

    QMenu menu(this);
    menu.addAction("读取数据", [=]()
                   { readDataItem(index.row()); });
    menu.addAction("写入数据", [=]()
                   { writeDataItem(index.row()); });
    menu.addSeparator();
    menu.addAction("复制", [=]()
                   { copyRowToClipboard(index.row()); });
    menu.addAction("复制DI码", [=]()
                   { copyDICode(index.row()); });
    menu.addSeparator();
    menu.addAction("添加监视", [=]()
                   { addWatchItem(index.row()); });

    menu.exec(m_tableWidget->viewport()->mapToGlobal(pos));
}

void DataTableWidget::onValidationFailed(const QString &message)
{
    QMessageBox::warning(this, "输入错误", message);
}

void DataTableWidget::onMeterDataChanged(const QString &diCode, const QVariant &newValue)
{
    const bool hasSearchFilter = m_searchEdit && !m_searchEdit->text().trimmed().isEmpty();
    if (hasSearchFilter)
    {
        refreshTable();
        updateExportButtonState();
        return;
    }

    updateDataItem(diCode, newValue);
    updateExportButtonState();
}

void DataTableWidget::onMeterBatchChanged()
{
    refreshTable();
    updateExportButtonState();
}

void DataTableWidget::filterTable(const QString &filterText)
{
    for (int i = 0; i < m_tableWidget->rowCount(); ++i)
    {
        bool match = false;

        for (int j = 0; j < m_tableWidget->columnCount(); ++j)
        {
            QTableWidgetItem *item = m_tableWidget->item(i, j);
            if (item && item->text().contains(filterText, Qt::CaseInsensitive))
            {
                match = true;
                break;
            }
        }

        m_tableWidget->setRowHidden(i, !match);
    }
}

void DataTableWidget::updateExportButtonState()
{
    if (!m_exportButton)
    {
        return;
    }

    m_exportButton->setEnabled(m_meterModel != nullptr);
    if (m_randomizeCurrentButton)
    {
        m_randomizeCurrentButton->setEnabled(m_meterModel != nullptr && !m_currentCategory.isEmpty());
    }
}

QString DataTableWidget::selectedMeterAddress() const
{
    if (!m_meterAddress.isEmpty())
    {
        return m_meterAddress;
    }

    if (m_meterModel)
    {
        return m_meterModel->getMeterAddress();
    }

    return QString();
}

QList<QString> DataTableWidget::currentTableDiCodes() const
{
    QList<QString> diCodes;
    diCodes.reserve(m_tableWidget->rowCount());

    for (int row = 0; row < m_tableWidget->rowCount(); ++row)
    {
        QTableWidgetItem *diItem = m_tableWidget->item(row, 0);
        if (!diItem)
        {
            continue;
        }

        const QString diCode = diItem->data(Qt::UserRole).toString();
        if (!diCode.isEmpty())
        {
            diCodes.append(diCode);
        }
    }

    return diCodes;
}

void DataTableWidget::updateItemValue(const QString &diCode, const QVariant &value)
{
    for (int i = 0; i < m_tableWidget->rowCount(); ++i)
    {
        QTableWidgetItem *diItem = m_tableWidget->item(i, 0);
        if (diItem && diItem->data(Qt::UserRole).toString() == diCode)
        {
            QTableWidgetItem *valueItem = m_tableWidget->item(i, 1);
            if (valueItem)
            {
                QSignalBlocker blocker(m_tableWidget);
                valueItem->setText(value.toString());
                valueItem->setData(Qt::UserRole, value);
            }
            break;
        }
    }
}

bool DataTableWidget::isVariableData(const QString &diCode) const
{
    // 变量数据DI码范围：0x02000000 - 0x02FFFFFF
    bool ok;
    quint32 code = diCode.toUInt(&ok, 16);
    return ok && (code >= 0x02000000 && code <= 0x02FFFFFF);
}

void DataTableWidget::readDataItem(int row)
{
    QTableWidgetItem *diItem = m_tableWidget->item(row, 0);
    if (diItem)
    {
        QString diCode = diItem->data(Qt::UserRole).toString();
        LOG_INFO(QString("读取数据: %1").arg(diCode));
    }
}

void DataTableWidget::writeDataItem(int row)
{
    QTableWidgetItem *diItem = m_tableWidget->item(row, 0);
    if (diItem)
    {
        QString diCode = diItem->data(Qt::UserRole).toString();
        LOG_INFO(QString("写入数据: %1").arg(diCode));
    }
}

void DataTableWidget::copyRowToClipboard(int row)
{
    QString rowText;
    for (int col = 0; col < m_tableWidget->columnCount(); ++col)
    {
        QTableWidgetItem *item = m_tableWidget->item(row, col);
        if (item)
        {
            rowText += item->text();
            if (col < m_tableWidget->columnCount() - 1)
                rowText += "\t";
        }
    }

    QApplication::clipboard()->setText(rowText);
    LOG_INFO("已复制到剪贴板");
}

void DataTableWidget::copyDICode(int row)
{
    QTableWidgetItem *diItem = m_tableWidget->item(row, 0);
    if (diItem)
    {
        QString diCode = diItem->data(Qt::UserRole).toString();
        QApplication::clipboard()->setText(diCode);
        LOG_INFO(QString("已复制DI码: %1").arg(diCode));
    }
}

void DataTableWidget::addWatchItem(int row)
{
    QTableWidgetItem *diItem = m_tableWidget->item(row, 0);
    if (diItem)
    {
        QString diCode = diItem->data(Qt::UserRole).toString();
        LOG_INFO(QString("添加监视: %1").arg(diCode));
    }
}
