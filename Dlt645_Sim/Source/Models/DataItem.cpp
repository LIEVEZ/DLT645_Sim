#include "DataItem.h"
#include "Utils/BCDHelper.h"
#include <QLocale>
#include <QRegularExpression>
#include <QDebug>
QString DataItem::getFormattedValue() const
{
    if (value.isNull())
    {
        return QString();
    }

    QString result;

    // 处理特殊格式
    if (!format.isEmpty())
    {
        // // 如果格式包含逗号，使用BCDHelper的格式化方法
        // if (format.contains(","))
        // {
        //     return BCDHelper::formatValueToString(value, format);
        // }
        qDebug() << "===DataItem::getFormattedValue()" << value;
        QVariantList valuesList;
        if (value.type() == QVariant::List)
        {
            valuesList = value.toList();
        }
        else
        {
            valuesList.append(value);
        }
        QStringList values;
        for (int i = 0; i < valuesList.size(); ++i)
        {
            const QVariant &value = valuesList[i];
            values.append(value.toString());
        }
        result = values.join(":");
    }
    return result;
}
