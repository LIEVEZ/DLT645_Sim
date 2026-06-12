#include "DeviceTreeWidget.h"
#include "Models/MeterModel.h"
#include "Utils/Logger.h"
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QStackedLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QAction>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>

namespace
{
    constexpr int kCategoryRootNameRole = Qt::UserRole + 8;

    QString itemDisplayName(QTreeWidgetItem *item)
    {
        if (!item)
        {
            return QString();
        }

        const QString text = item->text(0);
        const int firstSpaceIndex = text.indexOf(QChar(' '));
        if (firstSpaceIndex < 0)
        {
            return text.trimmed();
        }

        return text.mid(firstSpaceIndex + 1).trimmed();
    }

    bool itemHasType(QTreeWidgetItem *item, ItemType type)
    {
        return item && static_cast<ItemType>(item->data(0, Qt::UserRole + 1).toInt()) == type;
    }

    MeterInfo meterInfoFromItem(QTreeWidgetItem *item)
    {
        MeterInfo info;
        if (!itemHasType(item, ItemType::Meter))
        {
            return info;
        }

        info.alias = item->data(0, Qt::UserRole).toString();
        info.address = item->data(0, Qt::UserRole + 2).toString();
        info.operatorCode = item->data(0, Qt::UserRole + 3).toString();
        info.password = item->data(0, Qt::UserRole + 4).toString();
        info.includeLeadingFE = item->data(0, Qt::UserRole + 5).toBool();
        info.phaseType = static_cast<MeterPhaseType>(item->data(0, Qt::UserRole + 6).toInt());
        const QVariant eventRecordPresentData = item->data(0, Qt::UserRole + 7);
        info.eventRecordPresent = eventRecordPresentData.isValid() ? eventRecordPresentData.toBool() : true;
        return info;
    }
}

DeviceTreeWidget::DeviceTreeWidget(QWidget *parent)
    : QWidget(parent), m_treeWidget(new QTreeWidget(this)),
      m_filterEdit(new QLineEdit(this)), m_refreshButton(new QPushButton(QStringLiteral("刷新"), this)), m_expandAllButton(new QPushButton(QStringLiteral("展开"), this)), m_collapseAllButton(new QPushButton(QStringLiteral("折叠"), this)),
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
    m_treeWidget->setColumnCount(2);
    m_treeWidget->setColumnWidth(1, 48);
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeWidget->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_treeWidget->header()->setStretchLastSection(false);
    m_treeWidget->setAllColumnsShowFocus(true);

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

bool DeviceTreeWidget::addMeter(const QString &connectionAlias, const MeterInfo &meterInfo)
{
    QTreeWidgetItem *connectionItem = findConnectionItem(connectionAlias);
    if (!connectionItem)
    {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("未找到连接节点"));
        return false;
    }

    if (meterInfo.address.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("电表地址不能为空"));
        return false;
    }

    if (isMeterAddressExists(meterInfo.address))
    {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("通信地址 %1 已存在，不能创建重复电表。").arg(meterInfo.address));
        return false;
    }

    QTreeWidgetItem *meterItem = createMeterNode(meterInfo);
    connectionItem->addChild(meterItem);

    connectionItem->setExpanded(true);

    createDataCategoryNodes(meterItem);
    updateConnectionMeterCount(connectionItem);

    meterItem->setExpanded(true);
    return true;
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
                updateConnectionMeterCount(connectionItem);
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
        info = meterInfoFromItem(selectedItem);
    }

    return info;
}

QStringList DeviceTreeWidget::connectionAliases() const
{
    QStringList aliases;
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = m_treeWidget->topLevelItem(i);
        if (!itemHasType(item, ItemType::Connection))
        {
            continue;
        }

        const QString alias = item->data(0, Qt::UserRole).toString();
        if (!alias.isEmpty())
        {
            aliases.append(alias);
        }
    }
    aliases.removeDuplicates();
    return aliases;
}

QStringList DeviceTreeWidget::connectedPortsExcept(const QString &connectionAlias) const
{
    QStringList ports;
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = m_treeWidget->topLevelItem(i);
        if (!itemHasType(item, ItemType::Connection))
        {
            continue;
        }

        const QString alias = item->data(0, Qt::UserRole).toString();
        const bool isConnected = item->data(0, Qt::UserRole + 2).toBool();
        const QString port = item->data(0, Qt::UserRole + 3).toString();
        if (isConnected && alias != connectionAlias && !port.isEmpty())
        {
            ports.append(port);
        }
    }
    ports.removeDuplicates();
    return ports;
}

QList<MeterInfo> DeviceTreeWidget::allMeterInfos() const
{
    QList<MeterInfo> meters;
    for (const QString &alias : connectionAliases())
    {
        meters.append(meterInfosForConnection(alias));
    }
    return meters;
}

QList<MeterInfo> DeviceTreeWidget::meterInfosForConnection(const QString &connectionAlias) const
{
    QList<MeterInfo> meters;
    QTreeWidgetItem *connectionItem = findConnectionItem(connectionAlias);
    if (!connectionItem)
    {
        return meters;
    }

    for (int i = 0; i < connectionItem->childCount(); ++i)
    {
        QTreeWidgetItem *meterItem = connectionItem->child(i);
        const MeterInfo info = meterInfoFromItem(meterItem);
        if (!info.address.isEmpty() || !info.alias.isEmpty())
        {
            meters.append(info);
        }
    }
    return meters;
}

MeterInfo DeviceTreeWidget::meterInfo(const QString &connectionAlias, const QString &meterAlias) const
{
    return meterInfoFromItem(findMeterItem(connectionAlias, meterAlias));
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

    createSettlementDayNodes(energyItem, "电能量", "当前", 0);
    for (int i = 1; i <= 12; ++i)
    {
        createSettlementDayNodes(energyItem, "电能量", QString("上%1结算日").arg(i), i);
    }

    // 最大需量及发生时间（与电能量保持一致的结算日分层）
    QTreeWidgetItem *demandItem = createCategoryItem("最大需量及发生时间", ItemType::Category, true);
    meterItem->addChild(demandItem);
    createSettlementDayNodes(demandItem, "最大需量及发生时间", "当前", 0);
    for (int i = 1; i <= 12; ++i)
    {
        createSettlementDayNodes(demandItem, "最大需量及发生时间", QString("上%1结算日").arg(i), i);
    }

    // 变量（叶子节点，不可展开）
    QTreeWidgetItem *variableItem = createCategoryItem("变量", ItemType::Category, false);
    meterItem->addChild(variableItem);

    // 事件记录（按标准 A.4 家族展开）
    QTreeWidgetItem *eventItem = createCategoryItem("事件记录", ItemType::Category, true);
    meterItem->addChild(eventItem);
    const QStringList eventSubCategories = {
        QStringLiteral("失压"),
        QStringLiteral("欠压"),
        QStringLiteral("过压"),
        QStringLiteral("断相"),
        QStringLiteral("全失压"),
        QStringLiteral("辅助电源失电"),
        QStringLiteral("电压逆相序"),
        QStringLiteral("电流逆相序"),
        QStringLiteral("电压不平衡"),
        QStringLiteral("电流不平衡"),
        QStringLiteral("失流"),
        QStringLiteral("过流"),
        QStringLiteral("断流"),
        QStringLiteral("潮流反向"),
        QStringLiteral("过载"),
        QStringLiteral("电压合格率"),
        QStringLiteral("掉电"),
        QStringLiteral("需量超限"),
        QStringLiteral("编程记录"),
        QStringLiteral("电表清零"),
        QStringLiteral("需量清零"),
        QStringLiteral("事件清零"),
        QStringLiteral("校时"),
        QStringLiteral("时段表编程"),
        QStringLiteral("时区表编程"),
        QStringLiteral("周休日编程"),
        QStringLiteral("节假日编程"),
        QStringLiteral("有功组合方式编程"),
        QStringLiteral("无功组合方式1编程"),
        QStringLiteral("无功组合方式2编程"),
        QStringLiteral("结算日编程"),
        QStringLiteral("开表盖"),
        QStringLiteral("开端钮盒"),
        QStringLiteral("购电量记录"),
        QStringLiteral("购电金额记录")};
    for (const QString &subCategory : eventSubCategories)
    {
        QTreeWidgetItem *subItem = createCategoryItem(subCategory, ItemType::Category, false);
        subItem->setData(0, kCategoryRootNameRole, QStringLiteral("事件记录"));
        eventItem->addChild(subItem);
    }

    // 参变量（按标准 A.5 分类展开）
    QTreeWidgetItem *paramItem = createCategoryItem("参变量", ItemType::Category, true);
    meterItem->addChild(paramItem);
    const QStringList parameterSubCategories = {
        QStringLiteral("基本参数"),
        QStringLiteral("第一套时区时段"),
        QStringLiteral("第二套时区时段"),
        QStringLiteral("公共假日"),
        QStringLiteral("显示参数"),
        QStringLiteral("费率电价"),
        QStringLiteral("阶梯电价"),
        QStringLiteral("事件参数")};
    for (const QString &subCategory : parameterSubCategories)
    {
        QTreeWidgetItem *subItem = createCategoryItem(subCategory, ItemType::Category, false);
        subItem->setData(0, kCategoryRootNameRole, QStringLiteral("参变量"));
        paramItem->addChild(subItem);
    }

    // 冻结（可展开的文件夹）
    QTreeWidgetItem *freezeItem = createCategoryItem("冻结", ItemType::Category, true);
    meterItem->addChild(freezeItem);

    QStringList freezeSubCategories;
    freezeSubCategories << "定时冻结"
                        << "瞬时冻结"
                        << "时区切换冻结"
                        << "日时段切换冻结"
                        << "整点冻结"
                        << "分时费率切换冻结"
                        << "日冻结"
                        << "阶梯切换冻结";

    for (const QString &subCat : freezeSubCategories)
    {
        QTreeWidgetItem *subItem = createCategoryItem(subCat, ItemType::Category, false);
        freezeItem->addChild(subItem);
    }

    // 负荷记录（叶子节点，不可展开）
    QTreeWidgetItem *loadItem = createCategoryItem("负荷记录", ItemType::Category, false);
    meterItem->addChild(loadItem);
}

void DeviceTreeWidget::createSettlementDayNodes(QTreeWidgetItem *parentItem, const QString &rootCategoryName, const QString &name, int settlementDay)
{
    // 只创建Level 3的子分类节点，不创建Level 4的数据项节点
    QTreeWidgetItem *subItem = createCategoryItem(name, ItemType::Category, false);
    subItem->setData(0, Qt::UserRole + 7, settlementDay);
    subItem->setData(0, kCategoryRootNameRole, rootCategoryName);
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
    updateConnectionMeterCount(item);

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
    item->setData(0, Qt::UserRole + 6, static_cast<int>(info.phaseType));
    item->setData(0, Qt::UserRole + 7, info.eventRecordPresent); // 存储事件记录是否存在

    // 设置电表图标
    item->setText(0, QString("📊 %1 [%2] (%3)")
                         .arg(info.alias, info.address, meterPhaseTypeToDisplayName(info.phaseType)));

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
        meterInfo.phaseType = static_cast<MeterPhaseType>(item->data(0, Qt::UserRole + 6).toInt());
        emit meterInfoSelected(meterInfo);
        return;
    }

    // 检查是否点击了数据分类节点（Level 3）
    if (itemType == ItemType::Category)
    {
        QString categoryName = itemDisplayName(item);
        const QString rootCategoryName = item->data(0, kCategoryRootNameRole).toString();
        if (!rootCategoryName.isEmpty())
        {
            categoryName = QString("%1/%2").arg(rootCategoryName, categoryName);
        }
        QString meterAlias;

        // 鑾峰彇鐖剁骇鐢佃〃鑺傜偣
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
            meterInfo.phaseType = static_cast<MeterPhaseType>(parentItem->data(0, Qt::UserRole + 6).toInt());
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
            m_contextMenu->addAction(QIcon(":/icons/add.png"), QStringLiteral("批量新建电表"), [this, connectionAlias]()
                                     { emit batchNewMeterRequested(connectionAlias); });
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
            m_contextMenu->addAction(QIcon(":/icons/delete.png"), QStringLiteral("删除所有电表"), [this, connectionAlias]()
                                     { emit deleteAllMetersRequested(connectionAlias); });
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
            m_contextMenu->addAction(QIcon(":/icons/settings.png"), "事件记录设置", [this, meterAlias]()
                                     {
                                        QString connectionAlias = getSelectedConnectionAlias();
                                        emit eventRecordSettingsRequested(connectionAlias, meterAlias); });
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

void DeviceTreeWidget::updateConnectionMeterCount(QTreeWidgetItem *connectionItem)
{
    if (!connectionItem)
    {
        return;
    }

    int meterCount = 0;
    for (int i = 0; i < connectionItem->childCount(); ++i)
    {
        QTreeWidgetItem *child = connectionItem->child(i);
        if (!child)
        {
            continue;
        }

        ItemType childType = static_cast<ItemType>(child->data(0, Qt::UserRole + 1).toInt());
        if (childType == ItemType::Meter)
        {
            ++meterCount;
        }
    }

    connectionItem->setText(1, QString("%1台").arg(meterCount));
    connectionItem->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
}

QTreeWidgetItem *DeviceTreeWidget::findConnectionItem(const QString &connectionAlias) const
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

QTreeWidgetItem *DeviceTreeWidget::findMeterItem(const QString &connectionAlias, const QString &meterAlias) const
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
