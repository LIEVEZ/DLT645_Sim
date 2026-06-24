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

private:
    void setupUI();
    void loadDataItems();
    void populateTree();
    void populateTable(const QList<DataItem> &items);
    void filterTable(const QString &keyword);
    QList<DataItem> getDataItemsByCategory(const QString &categoryName) const;

    QTreeWidget *m_categoryTree;
    QTableWidget *m_dataTable;
    QLineEdit *m_searchBox;
    TermExplanationWidget *m_termExplanationWidget;

    MeterModel *m_catalogMeterModel;  // 临时电表实例
    QList<DataItem> m_allDataItems;   // 缓存所有数据项
    QString m_currentCategory;        // 当前选中分类
};

#endif // DATAITEMANUALDIALOG_H
