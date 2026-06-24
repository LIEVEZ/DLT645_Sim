#ifndef DATATABLEWIDGET_H
#define DATATABLEWIDGET_H

#include <QList>
#include <QPoint>
#include <QString>
#include <QStyledItemDelegate>
#include <QVariant>
#include <QWidget>

#include "Models/MeterPhaseType.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;
class MeterModel;
class DataTableExporter;

/**
 * @brief 数值编辑代理
 *
 * 用于验证和限制数据值列的输入
 */
class NumericDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit NumericDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;

    void setValidatorRange(double min, double max, int decimals);
    bool validateNumericValue(const QString &value) const;

private:
    double m_minValue;
    double m_maxValue;
    int m_decimals;
};

/**
 * @brief 数据表格组件
 *
 * 显示电表数据项的表格视图
 */
class DataTableWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 数据表格组件构造函数
     * @param parent 父组件
     */
    explicit DataTableWidget(QWidget *parent = nullptr);
    ~DataTableWidget();
    void setMeterModel(MeterModel *meterModel);

    /**
     * @brief 刷新数据表格
     */
    void refreshTable();

    /**
     * @brief 设置显示的数据类别
     * @param category 数据类别
     */
    void setCategory(const QString &category);

    /**
     * @brief 处理类别选中信号
     * @param categoryName 数据类别名称
     * @param meterAlias 电表别名
     */
    void onCategorySelected(const QString &categoryName, const QString &meterAlias);

    /**
     * @brief 获取当前显示的数据类别
     * @return 数据类别
     */
    QString getCategory() const;

    /**
     * @brief 设置电表信息
     * @param alias 电表别名
     * @param address 电表地址
     * @param phaseType 电表类型（单相/三相）
     */
    void setMeterInfo(const QString &alias, const QString &address, MeterPhaseType phaseType = MeterPhaseType::ThreePhase);

    /**
     * @brief 更新单个数据项的值
     * @param diCode 数据标识
     * @param value 新值
     */
    bool updateDataItem(const QString &diCode, const QVariant &value);

    /**
     * @brief 清空数据表格
     */
    void clearTable();

    /**
     * @brief 添加单个数据项到表格
     * @param diCode 数据标识
     * @param name 数据名称
     * @param value 数据值
     * @param unit 数据单位
     * @param format 数据格式
     */
    void addDataItem(const QString &diCode, const QString &name, const QVariant &value, const QString &unit, const QString &format = QString());

signals:
    /**
     * @brief 数据项选中信号
     * @param diCode 数据标识
     */
    void dataItemSelected(const QString &diCode);

    /**
     * @brief 数据项编辑信号
     * @param diCode 数据标识
     * @param value 新值
     */
    void dataItemEdited(const QString &diCode, const QVariant &value);

    /**
     * @brief 通知消息信号
     * @param message 消息内容
     * @param type 消息类型
     */
    void notification(const QString &message, int type);

private slots:
    void onItemClicked(QTableWidgetItem *item);
    void onItemDoubleClicked(QTableWidgetItem *item);
    void onItemChanged(QTableWidgetItem *item);
    void onSearchTextChanged(const QString &text);
    void onSearchReturnPressed();
    void onRefreshClicked();
    void onResetClicked();
    void onRandomizeClicked();
    void onRandomizeCurrentClicked();
    void onExportClicked();
    void onCustomContextMenuRequested(const QPoint &pos);
    void onValidationFailed(const QString &message);
    void onMeterDataChanged(const QString &diCode, const QVariant &newValue);
    void onMeterBatchChanged();

private:
    void setupUI();
    void setupTable();
    void populateCategory(const QString &category);
    QList<QString> dataCodesForCategory(const QString &category) const;
    void filterTable(const QString &filterText);
    void updateExportButtonState();
    QString selectedMeterAddress() const;
    void updateItemValue(const QString &diCode, const QVariant &value);
    bool showValueDetailDialog(QTableWidgetItem *item);
    void updateDataCount();
    QList<QString> currentTableDiCodes() const;
    bool isVariableData(const QString &diCode) const;
    void readDataItem(int row);
    void writeDataItem(int row);
    void copyRowToClipboard(int row);
    void copyDICode(int row);
    void addWatchItem(int row);

    MeterModel *m_meterModel;
    DataTableExporter *m_exporter;
    QTableWidget *m_tableWidget;
    QLineEdit *m_searchEdit;
    QPushButton *m_refreshButton;
    QPushButton *m_resetButton;
    QPushButton *m_randomizeButton;
    QPushButton *m_randomizeCurrentButton;
    QPushButton *m_exportButton;
    QLabel *m_meterInfoLabel;
    QLabel *m_formatHintLabel;
    QLabel *m_dataCountLabel;
    NumericDelegate *m_numericDelegate;
    QString m_currentCategory;
    QString m_meterAlias;
    QString m_meterAddress;
    MeterPhaseType m_meterPhaseType;
};

#endif // DATATABLEWIDGET_H
