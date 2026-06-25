#ifndef DATAITEMANUALDIALOG_H
#define DATAITEMANUALDIALOG_H

#include <QDialog>
#include <QList>

class QTreeWidget;
class QTableWidget;
class QLineEdit;
class QPushButton;
class QLabel;
class QTreeWidgetItem;
class QTableWidgetItem;
class MeterModel;
class TermExplanationWidget;
struct DataItem;
struct TermDefinition;

/**
 * @brief 数据项手册对话框
 *
 * 展示 DLT645-2007 协议所有数据项的详细说明，支持分类浏览和搜索过滤。
 * 采用运行时伪静态方案：创建临时 MeterModel 实例提取元数据。
 */
class DataItemManualDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DataItemManualDialog(QWidget *parent = nullptr);
    ~DataItemManualDialog();

    void applyDarkTheme();
    void applyLightTheme();

private slots:
    void onCategorySelected(QTreeWidgetItem *item, int column);
    void onSearchTextChanged(const QString &text);
    void onTableCellDoubleClicked(int row, int column);
    void onTableCurrentCellChanged(int currentRow, int currentColumn, int previousRow, int previousColumn);

private:
    void setupUI();
    void loadDataItems();
    void populateTree();
    void populateTable(const QList<DataItem> &items);
    void filterTable(const QString &keyword);
    QList<DataItem> getDataItemsByCategory(const QString &categoryName) const;
    bool isEnergyDataItem(const DataItem &item) const;
    bool isDemandDataItem(const DataItem &item) const;
    bool isVariableDataItem(const DataItem &item) const;
    bool isEventDataItem(const DataItem &item) const;
    bool isParameterDataItem(const DataItem &item) const;
    bool isFreezeDataItem(const DataItem &item) const;
    bool isLoadRecordDataItem(const DataItem &item) const;
    bool findDataItemByDiCode(const QString &diCode, DataItem *result) const;
    bool findDisplayedDataItem(int row, DataItem *result) const;
    TermDefinition buildEnergyDataItemTerm(const DataItem &item) const;
    TermDefinition buildDemandDataItemTerm(const DataItem &item) const;
    TermDefinition buildVariableDataItemTerm(const DataItem &item) const;
    TermDefinition buildEventDataItemTerm(const DataItem &item) const;
    TermDefinition buildParameterDataItemTerm(const DataItem &item) const;
    TermDefinition buildFreezeDataItemTerm(const DataItem &item) const;
    TermDefinition buildLoadRecordDataItemTerm(const DataItem &item) const;
    bool showEnergyDataItemTerm(const DataItem &item);
    bool showDemandDataItemTerm(const DataItem &item);
    bool showVariableDataItemTerm(const DataItem &item);
    bool showEventDataItemTerm(const DataItem &item);
    bool showParameterDataItemTerm(const DataItem &item);
    bool showFreezeDataItemTerm(const DataItem &item);
    bool showLoadRecordDataItemTerm(const DataItem &item);
    bool showSpecializedDataItemTerm(const DataItem &item);

    QTreeWidget *m_categoryTree;
    QTableWidget *m_dataTable;
    QLineEdit *m_searchBox;
    TermExplanationWidget *m_termExplanationWidget;

    MeterModel *m_catalogMeterModel;  // 临时电表实例
    QList<DataItem> m_allDataItems;   // 缓存所有数据项
    QString m_currentCategory;        // 当前选中分类
};

#endif // DATAITEMANUALDIALOG_H
