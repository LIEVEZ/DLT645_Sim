#include "DeviceTreeWidget.h"
#include "Models/MeterModel.h"
#include "Utils/Logger.h"
#include <QHeaderView>
#include <QAction>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
DeviceTreeWidget::DeviceTreeWidget(QWidget *parent)
    : QWidget(parent), m_treeWidget(new QTreeWidget(this)),
      m_filterEdit(new QLineEdit(this)), m_refreshButton(new QPushButton("刷新", this)), m_expandAllButton(new QPushButton("展开", this)), m_collapseAllButton(new QPushButton("折叠", this)),
      m_contextMenu(new QMenu(this)), m_emptyStateLabel(new QLabel(this)), m_isConnected(false)
{
    setupUI();
    setupTree();
    showEmptyState();
}

DeviceTreeWidget::~DeviceTreeWidget()
{
}

void DeviceTreeWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // 搜索框（置顶，独立控件，左对齐）
    m_filterEdit->setPlaceholderText("搜索设备...");
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setObjectName("searchBox");

    connect(m_filterEdit, &QLineEdit::textChanged, this, &DeviceTreeWidget::onFilterTextChanged);
    mainLayout->addWidget(m_filterEdit);

    // 创建堆叠布局来管理树形控件和空状态标签
    m_stackedLayout = new QStackedLayout();
    m_stackedLayout->setStackingMode(QStackedLayout::StackOne);

    // 添加树形控件
    m_stackedLayout->addWidget(m_treeWidget);

    // 添加空状态标签
    m_emptyStateLabel->setAlignment(Qt::AlignCenter);
    m_emptyStateLabel->setStyleSheet("QLabel { color: #808080; font-size: 12pt; }");
    m_emptyStateLabel->setText("点击\"新建连接\"\n开始添加设备");
    m_emptyStateLabel->setWordWrap(true);
    m_stackedLayout->addWidget(m_emptyStateLabel);

    // 默认显示空状态
    m_stackedLayout->setCurrentWidget(m_emptyStateLabel);

    mainLayout->addLayout(m_stackedLayout, 1);

    // 底部工具栏（固定在底部）
    QHBoxLayout *toolBarLayout = new QHBoxLayout();
    toolBarLayout->setSpacing(5);
    toolBarLayout->setContentsMargins(0, 0, 0, 0);

    m_refreshButton->setMaximumWidth(60);
    connect(m_refreshButton, &QPushButton::clicked, this, &DeviceTreeWidget::onRefreshClicked);
    toolBarLayout->addWidget(m_refreshButton);

    m_expandAllButton->setMaximumWidth(60);
    connect(m_expandAllButton, &QPushButton::clicked, this, &DeviceTreeWidget::onExpandAllClicked);
    toolBarLayout->addWidget(m_expandAllButton);

    m_collapseAllButton->setMaximumWidth(60);
    connect(m_collapseAllButton, &QPushButton::clicked, this, &DeviceTreeWidget::onCollapseAllClicked);
    toolBarLayout->addWidget(m_collapseAllButton);

    toolBarLayout->addStretch(); // 添加弹性空间，使按钮左对齐

    mainLayout->addLayout(toolBarLayout);
}

void DeviceTreeWidget::setupTree()
{
    m_treeWidget->setHeaderHidden(true);
    m_treeWidget->setRootIsDecorated(true);
    m_treeWidget->setIndentation(15);
    m_treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeWidget->setDragEnabled(false);
    m_treeWidget->setDropIndicatorShown(false);

    // 设置列宽自适应，确保文本完全显示
    m_treeWidget->setColumnCount(1);
    m_treeWidget->setColumnWidth(0, 300);
    m_treeWidget->header()->setStretchLastSection(true);

    // 连接信号
    connect(m_treeWidget, &QTreeWidget::itemClicked, this, &DeviceTreeWidget::onItemClicked);
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked, this, &DeviceTreeWidget::onItemDoubleClicked);
    connect(m_treeWidget, &QTreeWidget::itemExpanded, this, &DeviceTreeWidget::onItemExpanded);
    connect(m_treeWidget, &QTreeWidget::itemCollapsed, this, &DeviceTreeWidget::onItemCollapsed);

    // 设置上下文菜单
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested, this, &DeviceTreeWidget::onCustomContextMenuRequested);
}

void DeviceTreeWidget::showEmptyState()
{
    m_stackedLayout->setCurrentWidget(m_emptyStateLabel);
}

void DeviceTreeWidget::hideEmptyState()
{
    m_stackedLayout->setCurrentWidget(m_treeWidget);
}

void DeviceTreeWidget::addConnection(const ConnectionInfo &connectionInfo)
{
    LOG_INFO(QString("addConnection: alias='%1', port='%2', type='%3'")
                 .arg(connectionInfo.alias)
                 .arg(connectionInfo.port)
                 .arg(connectionInfo.type));

    hideEmptyState();

    QTreeWidgetItem *connectionItem = createConnectionNode(connectionInfo);
    m_treeWidget->addTopLevelItem(connectionItem);

    // 自动展开新添加的连接
    connectionItem->setExpanded(true);

    // 自动选中新添加的连接节点
    m_treeWidget->setCurrentItem(connectionItem);

    // 发出连接选中信号，确保工具栏按钮状态正确更新
    emit connectionSelected(connectionInfo.alias);
}

void DeviceTreeWidget::addMeter(const QString &connectionAlias, const MeterInfo &meterInfo)
{
    QTreeWidgetItem *connectionItem = findConnectionItem(connectionAlias);
    if (!connectionItem)
    {
        QMessageBox::warning(this, "错误", "未找到连接节点");
        return;
    }

    // 创建电表节点
    QTreeWidgetItem *meterItem = createMeterNode(meterInfo);
    connectionItem->addChild(meterItem);

    // 自动展开连接节点
    connectionItem->setExpanded(true);

    // 创建数据分类节点
    createDataCategoryNodes(meterItem);

    // 自动展开电表节点
    meterItem->setExpanded(true);
}

void DeviceTreeWidget::clearTree()
{
    m_treeWidget->clear();
    showEmptyState();
}

void DeviceTreeWidget::setConnectionStatus(const QString &connectionAlias, bool connected)
{
    QTreeWidgetItem *connectionItem = findConnectionItem(connectionAlias);
    if (connectionItem)
    {
        updateConnectionStatusIcon(connectionItem, connected);
    }
}

void DeviceTreeWidget::updateConnection(const QString &oldConnectionAlias, const ConnectionInfo &connectionInfo)
{
    QTreeWidgetItem *connectionItem = findConnectionItem(oldConnectionAlias);
    if (!connectionItem)
    {
        return;
    }

    connectionItem->setData(0, Qt::UserRole, connectionInfo.alias);
    connectionItem->setData(0, Qt::UserRole + 2, connectionInfo.isConnected);
    connectionItem->setData(0, Qt::UserRole + 3, connectionInfo.port);
    updateConnectionStatusIcon(connectionItem, connectionInfo.isConnected);
}

void DeviceTreeWidget::deleteConnection(const QString &connectionAlias)
{
    QTreeWidgetItem *connectionItem = findConnectionItem(connectionAlias);
    if (connectionItem)
    {
        int index = m_treeWidget->indexOfTopLevelItem(connectionItem);
        if (index >= 0)
        {
            m_treeWidget->takeTopLevelItem(index);
            delete connectionItem;

            // 如果没有连接了，显示空状态
            if (m_treeWidget->topLevelItemCount() == 0)
            {
                showEmptyState();
            }
        }
    }
}

void DeviceTreeWidget::deleteMeter(const QString &connectionAlias, const QString &meterAlias)
{
    LOG_INFO(QString("deleteMeter: connectionAlias='%1', meterAlias='%2'")
                 .arg(connectionAlias)
                 .arg(meterAlias));

    QTreeWidgetItem *connectionItem = findConnectionItem(connectionAlias);
    if (connectionItem)
    {
        for (int i = 0; i < connectionItem->childCount(); ++i)
        {
            QTreeWidgetItem *meterItem = connectionItem->child(i);
            // 比较存储在 UserRole 中的别名，而不是文本
            if (meterItem && meterItem->data(0, Qt::UserRole).toString() == meterAlias)
            {
                LOG_INFO(QString("deleteMeter: Found meter item, deleting..."));
                connectionItem->removeChild(meterItem);
                delete meterItem;
                break;
            }
        }
    }
    else
    {
        LOG_INFO(QString("deleteMeter: Connection item not found for alias='%1'").arg(connectionAlias));
    }
}

void DeviceTreeWidget::refreshTree()
{
    // 刷新当前树内容
    // TODO: 保存当前展开状态并恢复
    emit refreshRequested();
}

void DeviceTreeWidget::expandAll()
{
    m_treeWidget->expandAll();
}

void DeviceTreeWidget::collapseAll()
{
    m_treeWidget->collapseAll();
}

bool DeviceTreeWidget::canAddMeter() const
{
    QTreeWidgetItem *selectedItem = m_treeWidget->currentItem();
    if (!selectedItem)
    {
        LOG_INFO("canAddMeter: No item selected");
        return false;
    }

    int itemTypeInt = selectedItem->data(0, Qt::UserRole + 1).toInt();
    ItemType itemType = static_cast<ItemType>(itemTypeInt);
    LOG_INFO(QString("canAddMeter: itemTypeInt=%1, Connection=%2, Meter=%3")
                 .arg(itemTypeInt)
                 .arg(static_cast<int>(ItemType::Connection))
                 .arg(static_cast<int>(ItemType::Meter)));

    // 允许在连接节点或电表节点下添加电表
    if (itemType == ItemType::Connection)
    {
        LOG_INFO("canAddMeter: Connection node selected, returning true");
        return true;
    }
    else if (itemType == ItemType::Meter)
    {
        // 如果选中了电表节点，检查其父节点是否是连接节点
        QTreeWidgetItem *parentItem = selectedItem->parent();
        if (parentItem)
        {
            ItemType parentType = static_cast<ItemType>(parentItem->data(0, Qt::UserRole + 1).toInt());
            bool result = parentType == ItemType::Connection;
            LOG_INFO(QString("canAddMeter: Meter node selected, parentType=%1, returning %2")
                         .arg(static_cast<int>(parentType))
                         .arg(result));
            return result;
        }
    }

    LOG_INFO("canAddMeter: No valid node selected, returning false");
    return false;
}

QString DeviceTreeWidget::getSelectedConnectionAlias() const
{
    QTreeWidgetItem *selectedItem = m_treeWidget->currentItem();
    if (!selectedItem)
    {
        LOG_INFO("getSelectedConnectionAlias: No item selected");
        return QString();
    }

    int itemTypeInt = selectedItem->data(0, Qt::UserRole + 1).toInt();
    ItemType itemType = static_cast<ItemType>(itemTypeInt);
    LOG_INFO(QString("getSelectedConnectionAlias: itemTypeInt=%1, Connection=%2, Meter=%3")
                 .arg(itemTypeInt)
                 .arg(static_cast<int>(ItemType::Connection))
                 .arg(static_cast<int>(ItemType::Meter)));

    if (itemType == ItemType::Connection)
    {
        QString alias = selectedItem->data(0, Qt::UserRole).toString();
        LOG_INFO(QString("getSelectedConnectionAlias: Connection node, alias='%1'").arg(alias));
        return alias;
    }
    else if (itemType == ItemType::Meter)
    {
        QTreeWidgetItem *parentItem = selectedItem->parent();
        if (parentItem)
        {
            QString alias = parentItem->data(0, Qt::UserRole).toString();
            LOG_INFO(QString("getSelectedConnectionAlias: Meter node, parent alias='%1'").arg(alias));
            return alias;
        }
    }

    LOG_INFO("getSelectedConnectionAlias: No valid node, returning empty string");
    return QString();
}

MeterInfo DeviceTreeWidget::getSelectedMeterInfo() const
{
    MeterInfo info;
    QTreeWidgetItem *selectedItem = m_treeWidget->currentItem();
    if (!selectedItem)
    {
        return info;
    }

    ItemType itemType = static_cast<ItemType>(selectedItem->data(0, Qt::UserRole + 1).toInt());
    if (itemType == ItemType::Meter)
    {
        info.alias = selectedItem->data(0, Qt::UserRole).toString();
        info.address = selectedItem->data(0, Qt::UserRole + 2).toString();
        info.operatorCode = selectedItem->data(0, Qt::UserRole + 3).toString();
        info.password = selectedItem->data(0, Qt::UserRole + 4).toString();
        info.includeLeadingFE = selectedItem->data(0, Qt::UserRole + 5).toBool();
    }

    return info;
}

bool DeviceTreeWidget::isPortConnected(const QString &port) const
{
    // 遍历所有顶层连接节点
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = m_treeWidget->topLevelItem(i);
        if (item)
        {
            // 检查是否是连接节点
            ItemType itemType = static_cast<ItemType>(item->data(0, Qt::UserRole + 1).toInt());
            if (itemType == ItemType::Connection)
            {
                // 获取该连接的端口信息
                QString itemPort = item->data(0, Qt::UserRole + 3).toString();
                // 获取连接状态
                bool isConnected = item->data(0, Qt::UserRole + 2).toBool();

                // 如果端口匹配且已连接，返回true
                if (itemPort == port && isConnected)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

QTreeWidget *DeviceTreeWidget::getTreeWidget() const
{
    return m_treeWidget;
}

bool DeviceTreeWidget::isMeterAddressExists(const QString &address) const
{
    // 遍历所有连接节点
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *connectionItem = m_treeWidget->topLevelItem(i);
        if (connectionItem)
        {
            // 遍历该连接下的所有电表节点
            for (int j = 0; j < connectionItem->childCount(); ++j)
            {
                QTreeWidgetItem *meterItem = connectionItem->child(j);
                if (meterItem)
                {
                    ItemType itemType = static_cast<ItemType>(meterItem->data(0, Qt::UserRole + 1).toInt());
                    if (itemType == ItemType::Meter)
                    {
                        QString meterAddress = meterItem->data(0, Qt::UserRole + 2).toString();
                        if (meterAddress == address)
                        {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

void DeviceTreeWidget::createDataCategoryNodes(QTreeWidgetItem *meterItem)
{
    // 电能量（可展开的文件夹）
    QTreeWidgetItem *energyItem = createCategoryItem("电能量", ItemType::Category, true);
    meterItem->addChild(energyItem);

    // 添加子节点：当前、上1-12结算日（Level 3分类节点，不可展开）

    createSettlementDayNodes(energyItem, "当前", 0);
    // for (int i = 1; i <= 12; ++i)
    // {
    //     createSettlementDayNodes(energyItem, QString("上 %1 结算日").arg(i), i);
    // }

    // 最大需量及发生时间（叶子节点，不可展开）
    QTreeWidgetItem *demandItem = createCategoryItem("最大需量及发生时间", ItemType::Category, false);
    meterItem->addChild(demandItem);

    // 变量（叶子节点，不可展开）
    QTreeWidgetItem *variableItem = createCategoryItem("变量", ItemType::Category, false);
    meterItem->addChild(variableItem);

    // 事件记录（叶子节点，不可展开）
    QTreeWidgetItem *eventItem = createCategoryItem("事件记录", ItemType::Category, false);
    meterItem->addChild(eventItem);

    // 参变量（叶子节点，不可展开）
    QTreeWidgetItem *paramItem = createCategoryItem("参变量", ItemType::Category, false);
    meterItem->addChild(paramItem);

    // 冻结（可展开的文件夹）
    QTreeWidgetItem *freezeItem = createCategoryItem("冻结", ItemType::Category, true);
    meterItem->addChild(freezeItem);

    QStringList freezeSubCategories;
    freezeSubCategories << "日冻结";

    for (const QString &subCat : freezeSubCategories)
    {
        QTreeWidgetItem *subItem = createCategoryItem(subCat, ItemType::Category, false);
        freezeItem->addChild(subItem);
    }

    // 负荷记录（叶子节点，不可展开）
    QTreeWidgetItem *loadItem = createCategoryItem("负荷记录", ItemType::Category, false);
    meterItem->addChild(loadItem);
}

void DeviceTreeWidget::createSettlementDayNodes(QTreeWidgetItem *parentItem, const QString &name, int settlementDay)
{
    // 只创建Level 3的子分类节点，不创建Level 4的数据项节点
    QTreeWidgetItem *subItem = createCategoryItem(name, ItemType::Category, false);
    parentItem->addChild(subItem);
}

QTreeWidgetItem *DeviceTreeWidget::createConnectionNode(const ConnectionInfo &info)
{
    QTreeWidgetItem *item = new QTreeWidgetItem();
    // 存储连接别名
    item->setData(0, Qt::UserRole, info.alias);
    item->setData(0, Qt::UserRole + 1, static_cast<int>(ItemType::Connection));
    item->setData(0, Qt::UserRole + 2, info.isConnected); // 存储连接状态
    item->setData(0, Qt::UserRole + 3, info.port);        // 存储端口信息

    // 设置显示文本和图标（链接图标 + 状态圆点）
    updateConnectionStatusIcon(item, info.isConnected);

    return item;
}

QTreeWidgetItem *DeviceTreeWidget::createMeterNode(const MeterInfo &info)
{
    QTreeWidgetItem *item = new QTreeWidgetItem();
    // 显示电表别名和通信地址
    item->setText(0, QString("%1 [%2]").arg(info.alias, info.address));
    item->setData(0, Qt::UserRole, info.alias); // 存储电表别名
    item->setData(0, Qt::UserRole + 1, static_cast<int>(ItemType::Meter));
    item->setData(0, Qt::UserRole + 2, info.address);          // 存储通讯地址
    item->setData(0, Qt::UserRole + 3, info.operatorCode);     // 存储操作者代码
    item->setData(0, Qt::UserRole + 4, info.password);         // 存储密码
    item->setData(0, Qt::UserRole + 5, info.includeLeadingFE); // 存储是否包含前导FE

    // 设置电表图标
    item->setText(0, QString("📊 %1 [%2]").arg(info.alias, info.address));

    return item;
}

QTreeWidgetItem *DeviceTreeWidget::createCategoryItem(const QString &name, ItemType type, bool expandable)
{
    QTreeWidgetItem *item = new QTreeWidgetItem();
    // 根据是否可展开选择不同的图标
    QString icon = expandable ? "📁" : "📄";
    item->setText(0, QString("%1 %2").arg(icon, name));
    item->setData(0, Qt::UserRole + 1, static_cast<int>(type));

    return item;
}

QTreeWidgetItem *DeviceTreeWidget::createDataItem(const QString &diCode, const QString &name)
{
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setText(0, QString("📄 %1").arg(name));
    item->setData(0, Qt::UserRole, diCode); // 存储DI码
    item->setData(0, Qt::UserRole + 1, static_cast<int>(ItemType::Data));

    return item;
}

void DeviceTreeWidget::onItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    // 确保当前项被正确设置
    m_treeWidget->setCurrentItem(item);

    ItemType itemType = static_cast<ItemType>(item->data(0, Qt::UserRole + 1).toInt());

    // 检查是否点击了连接节点
    if (itemType == ItemType::Connection)
    {
        QString connectionAlias = item->data(0, Qt::UserRole).toString();
        emit connectionSelected(connectionAlias);
        return;
    }

    // 检查是否点击了电表节点
    if (itemType == ItemType::Meter)
    {
        // 先同步当前连接，再同步当前电表，避免主窗口在连接选中时把刚选中的电表清掉。
        QTreeWidgetItem *parentItem = item->parent();
        if (parentItem)
        {
            QString connectionAlias = parentItem->data(0, Qt::UserRole).toString();
            emit connectionSelected(connectionAlias);
        }

        QString meterAlias = item->data(0, Qt::UserRole).toString();
        emit meterSelected(meterAlias);

        // 发出电表信息选中信号
        MeterInfo meterInfo;
        meterInfo.alias = item->data(0, Qt::UserRole).toString();
        meterInfo.address = item->data(0, Qt::UserRole + 2).toString();
        meterInfo.operatorCode = item->data(0, Qt::UserRole + 3).toString();
        meterInfo.password = item->data(0, Qt::UserRole + 4).toString();
        meterInfo.includeLeadingFE = item->data(0, Qt::UserRole + 5).toBool();
        emit meterInfoSelected(meterInfo);
        return;
    }

    // 检查是否点击了数据分类节点（Level 3）
    if (itemType == ItemType::Category)
    {
        QString categoryName = item->text(0).mid(3); // 移除"📁 "前缀（包括空格）
        QString meterAlias;

        // 获取父级电表节点
        QTreeWidgetItem *parentItem = item->parent();
        while (parentItem)
        {
            ItemType parentType = static_cast<ItemType>(parentItem->data(0, Qt::UserRole + 1).toInt());
            if (parentType == ItemType::Meter)
            {
                meterAlias = parentItem->data(0, Qt::UserRole).toString();
                break;
            }
            parentItem = parentItem->parent();
        }

        if (!meterAlias.isEmpty())
        {
            emit categorySelected(categoryName, meterAlias);

            // 发出电表信息选中信号
            MeterInfo meterInfo;
            meterInfo.alias = parentItem->data(0, Qt::UserRole).toString();
            meterInfo.address = parentItem->data(0, Qt::UserRole + 2).toString();
            meterInfo.operatorCode = parentItem->data(0, Qt::UserRole + 3).toString();
            meterInfo.password = parentItem->data(0, Qt::UserRole + 4).toString();
            meterInfo.includeLeadingFE = parentItem->data(0, Qt::UserRole + 5).toBool();
            emit meterInfoSelected(meterInfo);
        }
        return;
    }

    // // 检查是否点击了数据项
    // if (itemType == ItemType::Data)
    // {
    //     QString diCode = item->data(0, Qt::UserRole).toString();
    //     emit dataItemSelected(diCode);

    //     // 获取父级电表节点并发出电表信息选中信号
    //     QTreeWidgetItem *parentItem = item->parent();
    //     while (parentItem)
    //     {
    //         ItemType parentType = static_cast<ItemType>(parentItem->data(0, Qt::UserRole + 1).toInt());
    //         if (parentType == ItemType::Meter)
    //         {
    //             MeterInfo meterInfo;
    //             meterInfo.alias = parentItem->data(0, Qt::UserRole).toString();
    //             meterInfo.address = parentItem->data(0, Qt::UserRole + 2).toString();
    //             meterInfo.operatorCode = parentItem->data(0, Qt::UserRole + 3).toString();
    //             meterInfo.password = parentItem->data(0, Qt::UserRole + 4).toString();
    //             meterInfo.includeLeadingFE = parentItem->data(0, Qt::UserRole + 5).toBool();
    //             emit meterInfoSelected(meterInfo);
    //             break;
    //         }
    //         parentItem = parentItem->parent();
    //     }
    // }
}

void DeviceTreeWidget::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    ItemType itemType = static_cast<ItemType>(item->data(0, Qt::UserRole + 1).toInt());

    // 双击连接节点：打开/关闭连接
    if (itemType == ItemType::Connection)
    {
        QString connectionAlias = item->data(0, Qt::UserRole).toString();
        bool isConnected = item->data(0, Qt::UserRole + 2).toBool();

        if (isConnected)
        {
            emit closeConnectionRequested(connectionAlias);
        }
        else
        {
            emit openConnectionRequested(connectionAlias);
        }
        return;
    }

    // 双击电表节点：读取该电表所有数据
    if (itemType == ItemType::Meter)
    {
        QString meterAlias = item->data(0, Qt::UserRole).toString();
        emit readAllDataRequested(meterAlias);
        return;
    }

    // 双击数据项：弹出编辑对话框
    if (itemType == ItemType::Data)
    {
        QString diCode = item->data(0, Qt::UserRole).toString();
        emit dataItemDoubleClicked(diCode);
    }
}

void DeviceTreeWidget::onFilterTextChanged(const QString &text)
{
    filterTree(text);
}

void DeviceTreeWidget::onRefreshClicked()
{
    refreshTree();
}

void DeviceTreeWidget::onExpandAllClicked()
{
    expandAll();
}

void DeviceTreeWidget::onCollapseAllClicked()
{
    collapseAll();
}

void DeviceTreeWidget::onItemExpanded(QTreeWidgetItem *item)
{
    // 更新展开/折叠图标
    if (item->childCount() > 0)
    {
        ItemType itemType = static_cast<ItemType>(item->data(0, Qt::UserRole + 1).toInt());
        if (itemType == ItemType::Category)
        {
            QString text = item->text(0);
            if (text.startsWith("▶"))
            {
                text.replace(0, 1, "▼");
                item->setText(0, text);
            }
        }
    }
}

void DeviceTreeWidget::onItemCollapsed(QTreeWidgetItem *item)
{
    // 更新展开/折叠图标
    if (item->childCount() > 0)
    {
        ItemType itemType = static_cast<ItemType>(item->data(0, Qt::UserRole + 1).toInt());
        if (itemType == ItemType::Category)
        {
            QString text = item->text(0);
            if (text.startsWith("▼"))
            {
                text.replace(0, 1, "▶");
                item->setText(0, text);
            }
        }
    }
}

void DeviceTreeWidget::onCustomContextMenuRequested(const QPoint &pos)
{
    QTreeWidgetItem *item = m_treeWidget->itemAt(pos);
    if (item)
    {
        ItemType itemType = static_cast<ItemType>(item->data(0, Qt::UserRole + 1).toInt());
        m_contextMenu->clear();

        switch (itemType)
        {
        case ItemType::Connection:
        {
            QString connectionAlias = item->data(0, Qt::UserRole).toString();
            bool isConnected = item->data(0, Qt::UserRole + 2).toBool();

            m_contextMenu->addAction(QIcon(":/icons/add.png"), "新建电表", [this, connectionAlias]()
                                     { emit newMeterRequested(connectionAlias); });
            m_contextMenu->addSeparator();

            if (isConnected)
            {
                m_contextMenu->addAction(QIcon(":/icons/disconnect.png"), "关闭连接", [this, connectionAlias]()
                                         { emit closeConnectionRequested(connectionAlias); });
            }
            else
            {
                m_contextMenu->addAction(QIcon(":/icons/connect.png"), "打开连接", [this, connectionAlias]()
                                         { emit openConnectionRequested(connectionAlias); });
            }

            m_contextMenu->addSeparator();
            m_contextMenu->addAction(QIcon(":/icons/delete.png"), "删除连接", [this, connectionAlias]()
                                     { emit deleteConnectionRequested(connectionAlias); });
            m_contextMenu->addAction(QIcon(":/icons/settings.png"), "编辑", [this, connectionAlias]()
                                     { emit editConnectionRequested(connectionAlias); });
            break;
        }

        case ItemType::Meter:
        {
            QString meterAlias = item->data(0, Qt::UserRole).toString();
            m_contextMenu->addAction(QIcon(":/icons/read.png"), "读取所有数据", [this, meterAlias]()
                                     { emit readAllDataRequested(meterAlias); });
            m_contextMenu->addAction(QIcon(":/icons/write.png"), "批量设置", [this, meterAlias]()
                                     { emit batchSetDataRequested(meterAlias); });
            m_contextMenu->addSeparator();
            m_contextMenu->addAction(QIcon(":/icons/export.png"), "导出", [this, meterAlias]() { /* TODO: 导出电表数据 */ });
            m_contextMenu->addSeparator();
            m_contextMenu->addAction(QIcon(":/icons/delete.png"), "删除电表", [this, meterAlias]()
                                     {
                                        QString connectionAlias = getSelectedConnectionAlias();
                                        emit deleteMeterRequested(connectionAlias, meterAlias); });
            break;
        }

        case ItemType::Category:
        {
            // 数据分类右键菜单
            m_contextMenu->addAction("读取该分类所有数据", [this]() { /* TODO: 读取分类数据 */ });
            m_contextMenu->addAction("导出", [this]() { /* TODO: 导出分类数据 */ });
            break;
        }

        case ItemType::Data:
        {
            QString diCode = item->data(0, Qt::UserRole).toString();
            m_contextMenu->addAction("读取", [this, diCode]()
                                     { emit readDataItemRequested(diCode); });
            m_contextMenu->addAction("写入", [this, diCode]()
                                     { emit writeDataItemRequested(diCode); });
            m_contextMenu->addAction("添加监视", [this, diCode]()
                                     { emit addWatchItemRequested(diCode); });
            m_contextMenu->addSeparator();
            m_contextMenu->addAction("复制DI码", [this, diCode]()
                                     { QApplication::clipboard()->setText(diCode); });
            m_contextMenu->addAction("导出", [this, diCode]()
                                     { emit exportDataItemRequested(diCode); });
            break;
        }

        default:
            break;
        }

        if (!m_contextMenu->isEmpty())
        {
            m_contextMenu->exec(m_treeWidget->mapToGlobal(pos));
        }
    }
}

void DeviceTreeWidget::filterTree(const QString &filterText)
{
    if (filterText.isEmpty())
    {
        // 清除过滤，显示所有项
        QTreeWidgetItemIterator it(m_treeWidget, QTreeWidgetItemIterator::All);
        while (*it)
        {
            (*it)->setHidden(false);
            ++it;
        }
        return;
    }

    // 应用过滤
    QTreeWidgetItemIterator it(m_treeWidget, QTreeWidgetItemIterator::All);
    while (*it)
    {
        QTreeWidgetItem *item = *it;
        ItemType itemType = static_cast<ItemType>(item->data(0, Qt::UserRole + 1).toInt());

        if (itemType == ItemType::Category)
        {
            // 检查子项是否匹配
            bool hasVisibleChild = false;
            for (int i = 0; i < item->childCount(); ++i)
            {
                QTreeWidgetItem *child = item->child(i);
                ItemType childType = static_cast<ItemType>(child->data(0, Qt::UserRole + 1).toInt());
                if (childType == ItemType::Data)
                {
                    QString text = child->text(0);
                    QString diCode = child->data(0, Qt::UserRole).toString();
                    if (text.contains(filterText, Qt::CaseInsensitive) ||
                        diCode.contains(filterText, Qt::CaseInsensitive))
                    {
                        child->setHidden(false);
                        hasVisibleChild = true;
                    }
                    else
                    {
                        child->setHidden(true);
                    }
                }
            }
            item->setHidden(!hasVisibleChild);
            if (hasVisibleChild)
            {
                item->setExpanded(true); // 自动展开匹配的父节点
            }
        }
        else if (itemType == ItemType::Data)
        {
            // 数据项
            QString text = item->text(0);
            QString diCode = item->data(0, Qt::UserRole).toString();
            bool matched = text.contains(filterText, Qt::CaseInsensitive) ||
                           diCode.contains(filterText, Qt::CaseInsensitive);
            item->setHidden(!matched);
        }
        else if (itemType == ItemType::Connection || itemType == ItemType::Meter)
        {
            // 连接和电表节点：检查名称是否匹配
            QString text = item->text(0);
            bool matched = text.contains(filterText, Qt::CaseInsensitive);
            item->setHidden(!matched);
        }

        ++it;
    }
}

bool DeviceTreeWidget::itemMatchesFilter(QTreeWidgetItem *item, const QString &filterText)
{
    QString text = item->text(0);
    return text.contains(filterText, Qt::CaseInsensitive);
}

void DeviceTreeWidget::updateConnectionStatusIcon(QTreeWidgetItem *connectionItem, bool connected)
{
    if (!connectionItem)
    {
        return;
    }

    // 更新连接状态
    connectionItem->setData(0, Qt::UserRole + 2, connected);

    // 获取连接别名和端口
    QString alias = connectionItem->data(0, Qt::UserRole).toString();
    QString port = connectionItem->data(0, Qt::UserRole + 3).toString();

    // 设置显示文本：链接图标 + 别名 + 端口 + 状态圆点
    QString statusDot = connected ? "🟢" : "🔴";
    connectionItem->setText(0, QString("🔗 %1 %2 %3").arg(alias, port, statusDot));

    // 设置链接图标
    connectionItem->setIcon(0, QIcon(":/icons/link.png"));
}

QTreeWidgetItem *DeviceTreeWidget::findConnectionItem(const QString &connectionAlias)
{
    QTreeWidgetItemIterator it(m_treeWidget, QTreeWidgetItemIterator::All);
    while (*it)
    {
        QTreeWidgetItem *item = *it;
        ItemType itemType = static_cast<ItemType>(item->data(0, Qt::UserRole + 1).toInt());

        if (itemType == ItemType::Connection)
        {
            QString alias = item->data(0, Qt::UserRole).toString();
            if (alias == connectionAlias)
            {
                return item;
            }
        }

        ++it;
    }

    return nullptr;
}

QTreeWidgetItem *DeviceTreeWidget::findMeterItem(const QString &connectionAlias, const QString &meterAlias)
{
    QTreeWidgetItem *connectionItem = findConnectionItem(connectionAlias);
    if (!connectionItem)
    {
        return nullptr;
    }

    for (int i = 0; i < connectionItem->childCount(); ++i)
    {
        QTreeWidgetItem *child = connectionItem->child(i);
        ItemType itemType = static_cast<ItemType>(child->data(0, Qt::UserRole + 1).toInt());

        if (itemType == ItemType::Meter)
        {
            QString alias = child->data(0, Qt::UserRole).toString();
            if (alias == meterAlias)
            {
                return child;
            }
        }
    }

    return nullptr;
}
