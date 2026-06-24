#include "DataItemManualDialog.h"

#include "Models/DataItem.h"
#include "Models/MeterModel.h"
#include "Services/TermCatalogService.h"
#include "Views/Common/TermExplanationWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSizePolicy>
#include <QSplitter>
#include <QTableWidget>
#include <QTextStream>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QDebug>
DataItemManualDialog::DataItemManualDialog(QWidget *parent)
    : QDialog(parent),
      m_categoryTree(nullptr),
      m_dataTable(nullptr),
      m_searchBox(nullptr),
      m_termExplanationWidget(nullptr),
      m_catalogMeterModel(nullptr),
      m_currentCategory(QString::fromUtf8("全部"))
{
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);
    setupUI();
    loadDataItems();
    populateTree();
    populateTable(m_allDataItems);
    applyDarkTheme();

    setWindowTitle(QString::fromUtf8("DLT645 数据项手册"));
    resize(1000, 700);
}

DataItemManualDialog::~DataItemManualDialog()
{
}

void DataItemManualDialog::setupUI()
{
    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 顶部搜索栏
    QHBoxLayout *searchLayout = new QHBoxLayout();
    QLabel *searchLabel = new QLabel(QString::fromUtf8("搜索:"), this);
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(QString::fromUtf8("输入 DI 码或描述关键词..."));
    m_searchBox->setClearButtonEnabled(true);
    connect(m_searchBox, &QLineEdit::textChanged, this, &DataItemManualDialog::onSearchTextChanged);

    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(m_searchBox);
    mainLayout->addLayout(searchLayout);

    // 内容区上下分割：上方数据表、下方术语解释
    QSplitter *mainSplitter = new QSplitter(Qt::Vertical, this);

    // 上方分割器（左侧树 + 右侧表格）
    QSplitter *splitter = new QSplitter(Qt::Horizontal, mainSplitter);

    // 左侧分类树
    m_categoryTree = new QTreeWidget(this);
    m_categoryTree->setHeaderLabel(QString::fromUtf8("数据分类"));
    m_categoryTree->setMaximumWidth(250);
    connect(m_categoryTree, &QTreeWidget::itemClicked, this, &DataItemManualDialog::onCategorySelected);
    splitter->addWidget(m_categoryTree);

    // 右侧数据表格
    m_dataTable = new QTableWidget(this);
    m_dataTable->setColumnCount(5);
    m_dataTable->setHorizontalHeaderLabels({
        QString::fromUtf8("DI 码"),
        QString::fromUtf8("描述"),
        QString::fromUtf8("格式"),
        QString::fromUtf8("单位"),
        QString::fromUtf8("可写")
    });
    m_dataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dataTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_dataTable->setAlternatingRowColors(false);  // 禁用交替行色
    m_dataTable->setShowGrid(true);  // 显示网格线
    m_dataTable->verticalHeader()->setVisible(false);  // 隐藏行号列
    m_dataTable->verticalHeader()->setDefaultSectionSize(30);  // 行高 30px
    m_dataTable->horizontalHeader()->setStretchLastSection(false);
    m_dataTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_dataTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_dataTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_dataTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_dataTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_dataTable->setColumnWidth(0, 140);  // DI 码（增加到 140px）
    m_dataTable->setColumnWidth(2, 150);  // 格式
    m_dataTable->setColumnWidth(3, 80);   // 单位
    m_dataTable->setColumnWidth(4, 60);   // 可写
    connect(m_dataTable, &QTableWidget::cellDoubleClicked, this, &DataItemManualDialog::onTableCellDoubleClicked);
    splitter->addWidget(m_dataTable);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(QList<int>() << 220 << 750);  // 左侧 220px，右侧 750px
    splitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainSplitter->addWidget(splitter);

    // 底部术语解释面板
    m_termExplanationWidget = new TermExplanationWidget(mainSplitter);
    m_termExplanationWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainSplitter->addWidget(m_termExplanationWidget);

    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setChildrenCollapsible(false);
    mainSplitter->setSizes(QList<int>() << 320 << 320);
    mainLayout->addWidget(mainSplitter, 1);
}

void DataItemManualDialog::loadDataItems()
{
    // 创建临时电表实例
    m_catalogMeterModel = new MeterModel(this);
    m_catalogMeterModel->initializeDefaultDataItems();

    // 提取所有数据项
    m_allDataItems = m_catalogMeterModel->getAllDataItems();
}

void DataItemManualDialog::populateTree()
{
    m_categoryTree->clear();

    // 统计各分类数量
    int totalCount = m_allDataItems.size();
    int energyCount = 0, demandCount = 0, variableCount = 0, eventCount = 0;
    int parameterCount = 0, freezeCount = 0, loadRecordCount = 0;

    for (const DataItem &item : m_allDataItems) {
        QString diCode = item.diCode;
        if (diCode.startsWith("0x00")) energyCount++;
        else if (diCode.startsWith("0x01")) demandCount++;
        else if (diCode.startsWith("0x02")) variableCount++;
        else if (diCode.startsWith("0x03")) eventCount++;
        else if (diCode.startsWith("0x04")) parameterCount++;
        else if (diCode.startsWith("0x05")) freezeCount++;
        else if (diCode.startsWith("0x06")) loadRecordCount++;
    }

    // 添加分类节点
    QTreeWidgetItem *allItem = new QTreeWidgetItem(m_categoryTree);
    allItem->setText(0, QString::fromUtf8("📁 全部 (%1)").arg(totalCount));
    allItem->setData(0, Qt::UserRole, QString::fromUtf8("全部"));

    QTreeWidgetItem *energyItem = new QTreeWidgetItem(m_categoryTree);
    energyItem->setText(0, QString::fromUtf8("📁 电能量 (%1)").arg(energyCount));
    energyItem->setData(0, Qt::UserRole, QString::fromUtf8("电能量"));

    QTreeWidgetItem *demandItem = new QTreeWidgetItem(m_categoryTree);
    demandItem->setText(0, QString::fromUtf8("📁 最大需量 (%1)").arg(demandCount));
    demandItem->setData(0, Qt::UserRole, QString::fromUtf8("最大需量"));

    QTreeWidgetItem *variableItem = new QTreeWidgetItem(m_categoryTree);
    variableItem->setText(0, QString::fromUtf8("📁 变量 (%1)").arg(variableCount));
    variableItem->setData(0, Qt::UserRole, QString::fromUtf8("变量"));

    QTreeWidgetItem *eventItem = new QTreeWidgetItem(m_categoryTree);
    eventItem->setText(0, QString::fromUtf8("📁 事件记录 (%1)").arg(eventCount));
    eventItem->setData(0, Qt::UserRole, QString::fromUtf8("事件记录"));

    QTreeWidgetItem *parameterItem = new QTreeWidgetItem(m_categoryTree);
    parameterItem->setText(0, QString::fromUtf8("📁 参变量 (%1)").arg(parameterCount));
    parameterItem->setData(0, Qt::UserRole, QString::fromUtf8("参变量"));

    QTreeWidgetItem *freezeItem = new QTreeWidgetItem(m_categoryTree);
    freezeItem->setText(0, QString::fromUtf8("📁 冻结 (%1)").arg(freezeCount));
    freezeItem->setData(0, Qt::UserRole, QString::fromUtf8("冻结"));

    QTreeWidgetItem *loadRecordItem = new QTreeWidgetItem(m_categoryTree);
    loadRecordItem->setText(0, QString::fromUtf8("📁 负荷记录 (%1)").arg(loadRecordCount));
    loadRecordItem->setData(0, Qt::UserRole, QString::fromUtf8("负荷记录"));

    // 默认选中"全部"
    m_categoryTree->setCurrentItem(allItem);
}

void DataItemManualDialog::populateTable(const QList<DataItem> &items)
{
    m_dataTable->setRowCount(0);
    m_dataTable->setRowCount(items.size());

    for (int i = 0; i < items.size(); ++i) {
        const DataItem &item = items[i];

        // DI 码
        QTableWidgetItem *diItem = new QTableWidgetItem(item.diCode);
        m_dataTable->setItem(i, 0, diItem);

        // 描述
        QTableWidgetItem *descItem = new QTableWidgetItem(item.description);
        m_dataTable->setItem(i, 1, descItem);

        // 格式
        QTableWidgetItem *formatItem = new QTableWidgetItem(item.format);
        m_dataTable->setItem(i, 2, formatItem);

        // 单位
        QTableWidgetItem *unitItem = new QTableWidgetItem(item.unit);
        m_dataTable->setItem(i, 3, unitItem);

        // 可写
        QString writableText = item.isWritable ? QString::fromUtf8("✓") : QString::fromUtf8("✗");
        QTableWidgetItem *writableItem = new QTableWidgetItem(writableText);
        writableItem->setTextAlignment(Qt::AlignCenter);
        m_dataTable->setItem(i, 4, writableItem);
    }
}

void DataItemManualDialog::filterTable(const QString &keyword)
{
    QList<DataItem> filtered;
    QString lowerKeyword = keyword.toLower();

    for (const DataItem &item : m_allDataItems) {
        // 分类过滤
        bool categoryMatch = true;
        if (m_currentCategory != QString::fromUtf8("全部")) {
            categoryMatch = false;
            if (m_currentCategory == QString::fromUtf8("电能量") && item.diCode.startsWith("0x00")) categoryMatch = true;
            else if (m_currentCategory == QString::fromUtf8("最大需量") && item.diCode.startsWith("0x01")) categoryMatch = true;
            else if (m_currentCategory == QString::fromUtf8("变量") && item.diCode.startsWith("0x02")) categoryMatch = true;
            else if (m_currentCategory == QString::fromUtf8("事件记录") && item.diCode.startsWith("0x03")) categoryMatch = true;
            else if (m_currentCategory == QString::fromUtf8("参变量") && item.diCode.startsWith("0x04")) categoryMatch = true;
            else if (m_currentCategory == QString::fromUtf8("冻结") && item.diCode.startsWith("0x05")) categoryMatch = true;
            else if (m_currentCategory == QString::fromUtf8("负荷记录") && item.diCode.startsWith("0x06")) categoryMatch = true;
        }

        // 关键词过滤
        bool keywordMatch = keyword.isEmpty() ||
                            item.diCode.toLower().contains(lowerKeyword) ||
                            item.description.toLower().contains(lowerKeyword);

        if (categoryMatch && keywordMatch) {
            filtered.append(item);
        }
    }

    populateTable(filtered);
}

QList<DataItem> DataItemManualDialog::getDataItemsByCategory(const QString &categoryName) const
{
    QList<DataItem> result;
    for (const DataItem &item : m_allDataItems) {
        bool match = false;
        if (categoryName == QString::fromUtf8("全部")) {
            match = true;
        } else if (categoryName == QString::fromUtf8("电能量") && item.diCode.startsWith("0x00")) {
            match = true;
        } else if (categoryName == QString::fromUtf8("最大需量") && item.diCode.startsWith("0x01")) {
            match = true;
        } else if (categoryName == QString::fromUtf8("变量") && item.diCode.startsWith("0x02")) {
            match = true;
        } else if (categoryName == QString::fromUtf8("事件记录") && item.diCode.startsWith("0x03")) {
            match = true;
        } else if (categoryName == QString::fromUtf8("参变量") && item.diCode.startsWith("0x04")) {
            match = true;
        } else if (categoryName == QString::fromUtf8("冻结") && item.diCode.startsWith("0x05")) {
            match = true;
        } else if (categoryName == QString::fromUtf8("负荷记录") && item.diCode.startsWith("0x06")) {
            match = true;
        }

        if (match) {
            result.append(item);
        }
    }
    return result;
}

void DataItemManualDialog::onCategorySelected(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item) return;

    m_currentCategory = item->data(0, Qt::UserRole).toString();
    filterTable(m_searchBox->text());
}

void DataItemManualDialog::onSearchTextChanged(const QString &text)
{
    filterTable(text);

    // 搜索匹配的术语
    TermDefinition term = TermCatalogService::instance()->searchTerm(text);

    qDebug() << "[TermSearch] Keyword:" << text << "| Found:" << (!term.term.isEmpty()) << "| Term:" << term.term;

    if (!term.term.isEmpty()) {
        m_termExplanationWidget->showTerm(term);
    } else {
        m_termExplanationWidget->clear();
    }
}

void DataItemManualDialog::onTableCellDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0 || row >= m_dataTable->rowCount()) return;

    // 获取 DI 码
    QTableWidgetItem *diItem = m_dataTable->item(row, 0);
    if (!diItem) return;

    QString diCode = diItem->text();
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(diCode);

    // 状态栏提示（临时）
    QMessageBox::information(this, QString::fromUtf8("已复制"),
                             QString::fromUtf8("DI 码 %1 已复制到剪贴板").arg(diCode));
}

void DataItemManualDialog::applyDarkTheme()
{
    setStyleSheet(R"(
        QDialog {
            background-color: #2B2B2B;
            color: #FFFFFF;
        }

        QLabel {
            color: #FFFFFF;
            background-color: transparent;
            font-family: "Microsoft YaHei";
        }

        QTreeWidget {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
        }

        QTreeWidget::item:selected {
            background-color: #094771;
        }

        QTreeWidget::item:hover {
            background-color: #4E5254;
        }

        QTableWidget {
            background-color: #2B2B2B;
            color: #FFFFFF;
            gridline-color: #555555;
            border: 1px solid #555555;
            border-radius: 3px;
        }

        QTableWidget::item {
            padding: 6px 8px;
            color: #FFFFFF;
            background-color: #2B2B2B;
        }

        QTableWidget::item:selected {
            background-color: #094771;
            color: #FFFFFF;
        }

        QTableWidget::item:hover {
            background-color: #3C3F41;
            color: #FFFFFF;
        }

        QHeaderView::section {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            padding: 5px;
            font-weight: bold;
        }

        QLineEdit {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 5px;
        }

        QLineEdit:focus {
            border: 1px solid #094771;
        }

        QPushButton {
            background-color: #0E639C;
            color: #FFFFFF;
            border: none;
            border-radius: 3px;
            padding: 6px 14px;
        }

        QPushButton:hover {
            background-color: #1177BB;
        }

        QPushButton:pressed {
            background-color: #094771;
        }
    )");

    // 同步主题到术语面板
    if (m_termExplanationWidget) {
        m_termExplanationWidget->applyDarkTheme();
    }
}

void DataItemManualDialog::applyLightTheme()
{
    setStyleSheet(R"(
        QDialog {
            background-color: #FFFFFF;
            color: #000000;
        }

        QLabel {
            color: #000000;
            background-color: transparent;
            font-family: "Microsoft YaHei";
        }

        QTreeWidget {
            background-color: #F5F5F5;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
        }

        QTreeWidget::item:selected {
            background-color: #0078D7;
            color: #FFFFFF;
        }

        QTreeWidget::item:hover {
            background-color: #E5F3FF;
        }

        QTableWidget {
            background-color: #FFFFFF;
            color: #000000;
            gridline-color: #CCCCCC;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
        }

        QTableWidget::item {
            padding: 6px 8px;
            color: #000000;
            background-color: #FFFFFF;
        }

        QTableWidget::item:selected {
            background-color: #0078D7;
            color: #FFFFFF;
        }

        QTableWidget::item:hover {
            background-color: #F5F5F5;
            color: #000000;
        }

        QHeaderView::section {
            background-color: #F0F0F0;
            color: #000000;
            border: 1px solid #CCCCCC;
            padding: 5px;
            font-weight: bold;
        }

        QLineEdit {
            background-color: #FFFFFF;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 5px;
        }

        QLineEdit:focus {
            border: 1px solid #0078D7;
        }

        QPushButton {
            background-color: #0078D7;
            color: #FFFFFF;
            border: none;
            border-radius: 3px;
            padding: 6px 14px;
        }

        QPushButton:hover {
            background-color: #006CC1;
        }

        QPushButton:pressed {
            background-color: #005A9E;
        }
    )");

    // 同步主题到术语面板
    if (m_termExplanationWidget) {
        m_termExplanationWidget->applyLightTheme();
    }
}
