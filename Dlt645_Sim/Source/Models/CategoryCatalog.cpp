#include "Models/CategoryCatalog.h"

#include "Models/DataItem.h"
#include "Models/MeterModel.h"

#include <QRegularExpression>

namespace CategoryCatalog
{
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

    QList<QString> settlementDayCategories(const QString &rootCategory)
    {
        QList<QString> categories;
        categories.reserve(13);
        categories.append(QStringLiteral("%1/当前").arg(rootCategory));
        for (int settlementDay = 1; settlementDay <= 12; ++settlementDay)
        {
            categories.append(QStringLiteral("%1/上%2结算日").arg(rootCategory).arg(settlementDay));
        }

        return categories;
    }

    QStringList eventCategoryNames()
    {
        return {
            QStringLiteral("失压"),
            QStringLiteral("欠压"),
            QStringLiteral("过压"),
            QStringLiteral("断相"),
            QStringLiteral("全失压"),
            QStringLiteral("辅助电源失电"),
            QStringLiteral("电压逆相序"),
            QStringLiteral("电流逆相序"),
            QStringLiteral("电压不平衡"),
            QStringLiteral("电流不平衡"),
            QStringLiteral("失流"),
            QStringLiteral("过流"),
            QStringLiteral("断流"),
            QStringLiteral("潮流反向"),
            QStringLiteral("过载"),
            QStringLiteral("电压合格率"),
            QStringLiteral("掉电"),
            QStringLiteral("需量超限"),
            QStringLiteral("编程记录"),
            QStringLiteral("电表清零"),
            QStringLiteral("需量清零"),
            QStringLiteral("事件清零"),
            QStringLiteral("校时"),
            QStringLiteral("时段表编程"),
            QStringLiteral("时区表编程"),
            QStringLiteral("周休日编程"),
            QStringLiteral("节假日编程"),
            QStringLiteral("有功组合方式编程"),
            QStringLiteral("无功组合方式1编程"),
            QStringLiteral("无功组合方式2编程"),
            QStringLiteral("结算日编程"),
            QStringLiteral("开表盖"),
            QStringLiteral("开端钮盒"),
            QStringLiteral("购电量记录"),
            QStringLiteral("购电金额记录")};
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

    QStringList parameterCategoryNames()
    {
        return {
            QStringLiteral("基本参数"),
            QStringLiteral("第一套时区时段"),
            QStringLiteral("第二套时区时段"),
            QStringLiteral("公共假日"),
            QStringLiteral("显示参数"),
            QStringLiteral("费率电价"),
            QStringLiteral("阶梯电价"),
            QStringLiteral("事件参数")};
    }

    QList<ParameterCategoryDefinition> parameterCategoryDefinitions()
    {
        return {
            {QStringLiteral("基本参数"), {QStringLiteral("0x0400"), QStringLiteral("0x1C0104")}},
            {QStringLiteral("第一套时区时段"), {QStringLiteral("0x0401")}},
            {QStringLiteral("第二套时区时段"), {QStringLiteral("0x0402")}},
            {QStringLiteral("公共假日"), {QStringLiteral("0x0403")}},
            {QStringLiteral("显示参数"), {QStringLiteral("0x0404")}},
            {QStringLiteral("费率电价"), {QStringLiteral("0x040501"), QStringLiteral("0x040502")}},
            {QStringLiteral("阶梯电价"), {QStringLiteral("0x040600"), QStringLiteral("0x040601"), QStringLiteral("0x040602"), QStringLiteral("0x040603")}},
            {QStringLiteral("事件参数"), {QStringLiteral("0x0409")}}};
    }

    QList<QString> filterCodesByPrefixes(const QList<QString> &allCodes, const QStringList &prefixes)
    {
        if (prefixes.isEmpty())
        {
            return allCodes;
        }

        QList<QString> filteredCodes;
        filteredCodes.reserve(allCodes.size());
        for (const QString &diCode : allCodes)
        {
            for (const QString &prefix : prefixes)
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

    QList<QString> filterEventCodesByCategory(const MeterModel *meterModel, const QList<QString> &allCodes, const QString &categoryName)
    {
        const QString normalizedCategoryName = categoryName.trimmed();
        QStringList matchedPrefixes;
        for (const EventCategoryDefinition &definition : eventCategoryDefinitions())
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

        QList<QString> filteredCodes = filterCodesByPrefixes(allCodes, matchedPrefixes);
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

    QList<QString> filterParameterCodesByCategory(const QList<QString> &allCodes, const QString &categoryName)
    {
        const QString normalizedCategoryName = categoryName.trimmed();
        QStringList matchedPrefixes;
        for (const ParameterCategoryDefinition &definition : parameterCategoryDefinitions())
        {
            if (definition.name == normalizedCategoryName)
            {
                matchedPrefixes = definition.prefixes;
                break;
            }
        }

        return filterCodesByPrefixes(allCodes, matchedPrefixes);
    }
}
