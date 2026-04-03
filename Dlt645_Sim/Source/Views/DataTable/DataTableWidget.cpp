#include "DataTableWidget.h"
#include "Utils/Logger.h"
#include "../../Models/MeterModel.h"
#include "../../Models/DataItem.h"
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
#include <QLineEdit>
#include <QDoubleValidator>
#include <QRandomGenerator>
#include <QDateTime>
#include <QDebug>
#include <QSignalBlocker>

namespace
{
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
    Q_UNUSED(index);
    QLineEdit *editor = new QLineEdit(parent);
    editor->setValidator(new QDoubleValidator(m_minValue, m_maxValue, m_decimals, editor));
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

    if (!validateNumericValue(value))
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
    : QWidget(parent), m_meterModel(nullptr), m_currentCategory(""), m_meterAlias(""), m_meterAddress("")
{
    setupUI();

    // 设置数值编辑代理
    m_tableWidget->setItemDelegateForColumn(1, m_numericDelegate);
}

DataTableWidget::~DataTableWidget()
{
}

void DataTableWidget::setMeterModel(MeterModel *meterModel)
{
    if (m_meterModel)
    {
        disconnect(m_meterModel, &MeterModel::dataChanged,
                   this, &DataTableWidget::onMeterDataChanged);
    }

    m_meterModel = meterModel;

    // 连接数据变化信号，当数据改变时重新刷新表格
    if (m_meterModel)
    {
        connect(m_meterModel, &MeterModel::dataChanged,
                this, &DataTableWidget::onMeterDataChanged, Qt::UniqueConnection);
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
    m_tableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
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

    QList<QString> codes;

    if (category == "当前")
    {
        codes.append(m_meterModel->getEnergyDataCodes());
    }
    else if (category == "最大需量及发生时间")
    {
        codes.append(m_meterModel->getDemandDataCodes());
        codes.append(m_meterModel->getMaxDemandDataCodes());
    }
    else if (category == "变量")
    {
        codes.append(m_meterModel->getVariableDataCodes());
    }
    else if (category == "事件记录")
    {
        codes.append(m_meterModel->getEventDataCodes());
    }
    else if (category == "参变量")
    {
        codes.append(m_meterModel->getParameterDataCodes());
    }
    else if (category == "日冻结")
    {
        codes.append(m_meterModel->getFreezeDataCodes());
    }
    else if (category == "负荷记录")
    {
        codes.append(m_meterModel->getLoadRecordDataCodes());
    }

    for (const QString &diCode : codes)
    {
        // 从MeterModel中获取完整的DataItem信息
        DataItem dataItem = m_meterModel->getDataItem(diCode);
        QString name = dataItem.description;
        dataItem.value = m_meterModel->getDataValue(diCode);
        QString unit = dataItem.unit;
        QString dataFormat = dataItem.format;

        int row = m_tableWidget->rowCount();
        m_tableWidget->insertRow(row);

        // 数据标识
        QTableWidgetItem *diItem = new QTableWidgetItem(diCode);
        diItem->setFlags(diItem->flags() & ~Qt::ItemIsEditable);
        diItem->setData(Qt::UserRole, diCode);
        m_tableWidget->setItem(row, 0, diItem);

        // 数据值 - 使用DataItem的格式化方法
        QString formattedValue = dataItem.getFormattedValue();

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

void DataTableWidget::refreshTable()
{
    qDebug() << "DataTableWidget::refreshTable()" << m_currentCategory;
    populateCategory(m_currentCategory);
    updateDataCount();
}

void DataTableWidget::setCategory(const QString &category)
{
    m_currentCategory = category;
    populateCategory(category);
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

void DataTableWidget::setMeterInfo(const QString &alias, const QString &address)
{
    m_meterAlias = alias;
    m_meterAddress = address;
    updateExportButtonState();
    if (alias.isEmpty() && address.isEmpty())
    {
        m_meterInfoLabel->setText("电表信息");
    }
    else
    {
        m_meterInfoLabel->setText(QString("电表信息：[%1] [%2]").arg(alias, address));
    }
}

void DataTableWidget::updateDataItem(const QString &diCode, const QVariant &value)
{
    for (int i = 0; i < m_tableWidget->rowCount(); ++i)
    {
        QTableWidgetItem *diItem = m_tableWidget->item(i, 0);
        if (diItem && diItem->data(Qt::UserRole).toString() == diCode)
        {
            QTableWidgetItem *valueItem = m_tableWidget->item(i, 1);
            if (valueItem)
            {
                valueItem->setText(value.toString());
                valueItem->setData(Qt::UserRole, value);
            }
            break;
        }
    }
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

void DataTableWidget::onItemDoubleClicked(QTableWidgetItem *item)
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
    filterTable(text);
}

void DataTableWidget::onSearchReturnPressed()
{
    // 回车键触发搜索（与textChanged相同）
    filterTable(m_searchEdit->text());
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
        m_meterModel->setDataValue("0x04000000", m_meterModel->getMeterAddress());
        refreshTable();
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
    m_meterModel->setDataValue("0x04000000", m_meterModel->getMeterAddress());
    refreshTable();
    LOG_INFO("所有数据已随机化");
}

void DataTableWidget::onExportClicked()
{
    if (!m_meterModel)
    {
        QMessageBox::information(this, "Select Meter", "Please select a meter first.");
        return;
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

    if (suffix == "csv")
    {
        exportToCSV(fileName);
    }
    else if (suffix == "xlsx")
    {
        exportToExcel(fileName);
    }
    else if (suffix == "json")
    {
        exportToJSON(fileName);
    }
    else
    {
        // 默认CSV
        exportToCSV(fileName);
    }

    LOG_INFO("数据导出成功");
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
    Q_UNUSED(diCode);
    Q_UNUSED(newValue);
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

void DataTableWidget::exportToCSV(const QString &fileName)
{
    {
        QFile exportFile(fileName);
        if (!exportFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            return;
        }

        QTextStream out(&exportFile);
        out.setCodec("UTF-8");
        out.setGenerateByteOrderMark(true);

        const QString meterAddress = selectedMeterAddress();
        out << QString::fromUtf8("电表别名") << escapeCsvField(m_meterAlias) << "\n";
        out << QString::fromUtf8("电表地址") << escapeCsvField(meterAddress) << "\n";
        out << QString::fromUtf8("当前分类") << escapeCsvField(m_currentCategory) << "\n";
        out << QString::fromUtf8("导出时间") << escapeCsvField(QDateTime::currentDateTime().toString(Qt::ISODate)) << "\n";
        out << "\n";
        out << QString::fromUtf8("数据标识,数据值,单位,数据格式,数据项描述\n");

        for (int i = 0; i < m_tableWidget->rowCount(); ++i)
        {
            const QString diCode = m_tableWidget->item(i, 0)->text();
            const QString value = (diCode == "0x04000000") ? meterAddress : m_tableWidget->item(i, 1)->text();
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
        return;
    }

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&file);
        out.setCodec("UTF-8");
        out.setGenerateByteOrderMark(true);
        const QString meterAddress = selectedMeterAddress();

        // 写入表头
        out << "数据标识,数据值,单位,数据格式,数据项描述\n";

        // 写入数据
        for (int i = 0; i < m_tableWidget->rowCount(); ++i)
        {
            QString diCode = m_tableWidget->item(i, 0)->text();
            QString value = m_tableWidget->item(i, 1)->text();
            QString unit = m_tableWidget->item(i, 2)->text();
            QString time = m_tableWidget->item(i, 3)->text();
            QString format = m_tableWidget->item(i, 4)->text();
            QString desc = m_tableWidget->item(i, 5)->text();

            out << diCode << "," << value << "," << unit << "," << time << "," << format << "," << desc << "\n";
        }

        file.close();
    }
}

void DataTableWidget::exportToExcel(const QString &fileName)
{
    // 简化实现：导出为CSV格式（Excel可直接打开）
    exportToCSV(fileName);
}

void DataTableWidget::exportToJSON(const QString &fileName)
{
    {
        const QString meterAddress = selectedMeterAddress();
        QJsonArray dataArray;

        for (int i = 0; i < m_tableWidget->rowCount(); ++i)
        {
            QJsonObject dataObj;
            const QString diCode = m_tableWidget->item(i, 0)->text();
            dataObj["diCode"] = diCode;
            dataObj["value"] = (diCode == "0x04000000") ? meterAddress : m_tableWidget->item(i, 1)->text();
            dataObj["unit"] = m_tableWidget->item(i, 2)->text();
            dataObj["format"] = m_tableWidget->item(i, 3)->text();
            dataObj["description"] = m_tableWidget->item(i, 4)->text();
            dataArray.append(dataObj);
        }

        QJsonObject exportObj;
        exportObj["meterAlias"] = m_meterAlias;
        exportObj["meterAddress"] = meterAddress;
        exportObj["category"] = m_currentCategory;
        exportObj["exportedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        exportObj["data"] = dataArray;

        QJsonDocument doc(exportObj);
        QFile exportFile(fileName);
        if (exportFile.open(QIODevice::WriteOnly))
        {
            exportFile.write(doc.toJson());
            exportFile.close();
        }
        return;
    }

    QJsonArray dataArray;

    for (int i = 0; i < m_tableWidget->rowCount(); ++i)
    {
        QJsonObject dataObj;
        dataObj["diCode"] = m_tableWidget->item(i, 0)->text();
        dataObj["value"] = m_tableWidget->item(i, 1)->text();
        dataObj["unit"] = m_tableWidget->item(i, 2)->text();
        dataObj["updateTime"] = m_tableWidget->item(i, 3)->text();
        dataObj["format"] = m_tableWidget->item(i, 4)->text();
        dataObj["description"] = m_tableWidget->item(i, 5)->text();
        dataArray.append(dataObj);
    }

    QJsonDocument doc(dataArray);

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(doc.toJson());
        file.close();
    }
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
