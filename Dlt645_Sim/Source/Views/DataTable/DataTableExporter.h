#ifndef DATATABLEEXPORTER_H
#define DATATABLEEXPORTER_H

#include <QString>

class QTableWidget;

/**
 * @brief 数据表格导出服务
 *
 * 负责将表格数据导出为 CSV/Excel/JSON 格式
 */
class DataTableExporter
{
public:
    /**
     * @brief 构造函数
     * @param tableWidget 要导出的表格组件
     */
    explicit DataTableExporter(QTableWidget *tableWidget);

    /**
     * @brief 设置电表信息
     * @param alias 电表别名
     * @param address 电表地址
     * @param category 当前分类
     */
    void setMeterInfo(const QString &alias, const QString &address, const QString &category);

    /**
     * @brief 导出为 CSV 格式
     * @param fileName 文件路径
     * @return 是否成功
     */
    bool exportToCSV(const QString &fileName);

    /**
     * @brief 导出为 Excel 格式（实际为 CSV，Excel 可直接打开）
     * @param fileName 文件路径
     * @return 是否成功
     */
    bool exportToExcel(const QString &fileName);

    /**
     * @brief 导出为 JSON 格式
     * @param fileName 文件路径
     * @return 是否成功
     */
    bool exportToJSON(const QString &fileName);

private:
    /**
     * @brief CSV 字段转义
     * @param value 原始值
     * @return 转义后的值
     */
    static QString escapeCsvField(const QString &value);

    /**
     * @brief 显示分类名称
     * @param category 分类标识
     * @return 显示名称
     */
    static QString displayCategoryName(const QString &category);

    QTableWidget *m_tableWidget;
    QString m_meterAlias;
    QString m_meterAddress;
    QString m_currentCategory;
};

#endif // DATATABLEEXPORTER_H
