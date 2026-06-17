#include "DataTableExporter.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>

namespace
{
    struct CategorySelection
    {
        QString rootCategory;
        QString leafCategory;
    };

    CategorySelection parseCategorySelection(const QString &category)
    {
        const int separatorIndex = category.indexOf('/');
        if (separatorIndex < 0)
        {
            return {category, QString()};
        }

        return {category.left(separatorIndex), category.mid(separatorIndex + 1)};
    }
}

DataTableExporter::DataTableExporter(QTableWidget *tableWidget)
    : m_tableWidget(tableWidget)
{
}

void DataTableExporter::setMeterInfo(const QString &alias, const QString &address, const QString &category)
{
    m_meterAlias = alias;
    m_meterAddress = address;
    m_currentCategory = category;
}

QString DataTableExporter::escapeCsvField(const QString &value)
{
    QString escaped = value;
    escaped.replace("\"", "\"\"");

    if (escaped.contains(',') || escaped.contains('"') || escaped.contains('\n') || escaped.contains('\r'))
    {
        escaped = QString("\"%1\"").arg(escaped);
    }

    return escaped;
}

QString DataTableExporter::displayCategoryName(const QString &category)
{
    const CategorySelection selection = parseCategorySelection(category);
    if (selection.leafCategory.isEmpty())
    {
        return selection.rootCategory;
    }

    return QStringLiteral("%1 / %2").arg(selection.rootCategory, selection.leafCategory);
}

bool DataTableExporter::exportToCSV(const QString &fileName)
{
    QFile exportFile(fileName);
    if (!exportFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return false;
    }

    QTextStream out(&exportFile);
    out.setCodec("UTF-8");
    out.setGenerateByteOrderMark(true);

    // 写入元信息
    out << QString::fromUtf8("电表别名,") << escapeCsvField(m_meterAlias) << "\n";
    out << QString::fromUtf8("电表地址,") << escapeCsvField(m_meterAddress) << "\n";
    out << QString::fromUtf8("当前分类,") << escapeCsvField(displayCategoryName(m_currentCategory)) << "\n";
    out << QString::fromUtf8("导出时间,") << escapeCsvField(QDateTime::currentDateTime().toString(Qt::ISODate)) << "\n";
    out << "\n";

    // 写入表头
    out << QString::fromUtf8("数据标识,数据值,单位,数据格式,数据项描述\n");

    // 写入数据行
    for (int i = 0; i < m_tableWidget->rowCount(); ++i)
    {
        const QString diCode = m_tableWidget->item(i, 0)->text();
        const QString value = (diCode == "0x04000401") ? m_meterAddress : m_tableWidget->item(i, 1)->text();
        const QString unit = m_tableWidget->item(i, 2)->text();
        const QString format = m_tableWidget->item(i, 3)->text();
        const QString desc = m_tableWidget->item(i, 4)->text();

        out << escapeCsvField(diCode) << ","
            << escapeCsvField(value) << ","
            << escapeCsvField(unit) << ","
            << escapeCsvField(format) << ","
            << escapeCsvField(desc) << "\n";
    }

    exportFile.close();
    return true;
}

bool DataTableExporter::exportToExcel(const QString &fileName)
{
    // 简化实现：导出为 CSV 格式（Excel 可直接打开）
    return exportToCSV(fileName);
}

bool DataTableExporter::exportToJSON(const QString &fileName)
{
    QJsonArray dataArray;

    // 构建数据数组
    for (int i = 0; i < m_tableWidget->rowCount(); ++i)
    {
        QJsonObject dataObj;
        const QString diCode = m_tableWidget->item(i, 0)->text();
        dataObj["diCode"] = diCode;
        dataObj["value"] = (diCode == "0x04000401") ? m_meterAddress : m_tableWidget->item(i, 1)->text();
        dataObj["unit"] = m_tableWidget->item(i, 2)->text();
        dataObj["format"] = m_tableWidget->item(i, 3)->text();
        dataObj["description"] = m_tableWidget->item(i, 4)->text();
        dataArray.append(dataObj);
    }

    // 构建根对象
    QJsonObject exportObj;
    exportObj["meterAlias"] = m_meterAlias;
    exportObj["meterAddress"] = m_meterAddress;
    exportObj["category"] = displayCategoryName(m_currentCategory);
    exportObj["exportedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    exportObj["data"] = dataArray;

    // 写入文件
    QJsonDocument doc(exportObj);
    QFile exportFile(fileName);
    if (!exportFile.open(QIODevice::WriteOnly))
    {
        return false;
    }

    exportFile.write(doc.toJson());
    exportFile.close();
    return true;
}
