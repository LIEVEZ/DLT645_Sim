#ifndef DATAITEM_H
#define DATAITEM_H

#include <QString>
#include <QVariant>
#include <QDateTime>

/**
 * @brief 数据项结构
 *
 * 表示DLT645-2007协议中的一个数据项
 */
struct DataItem
{
    QString diCode;      // 数据标识（如"0x00000100"）
    QString description; // 数据项描述
    QVariant value;      // 数据值
    QString unit;        // 单位
    QString format;      // 数据格式（如""）
    bool isWritable;     // 是否可写
    int decimalPlaces;   // 小数位数

    DataItem()
        : isWritable(false), decimalPlaces(0)
    {
    }

    DataItem(const QString &di, const QString &desc, const QVariant &val,
             const QString &u, const QString &fmt, bool writable = false, int decimals = 0)
        : diCode(di), description(desc), value(val), unit(u), format(fmt), isWritable(writable), decimalPlaces(decimals)
    {
    }

    /**
     * @brief 获取格式化后的值字符串
     * @return 格式化后的字符串
     */
    QString getFormattedValue() const;
};

#endif // DATAITEM_H
