#ifndef DEVICETREEWIDGET_H
#define DEVICETREEWIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMenu>
#include "Models/MeterPhaseType.h"

/**
 * @brief 设备树节点类型枚举
 */
enum class ItemType
{
    Connection, // 连接节点 (Level 1)
    Meter,      // 电表节点 (Level 2)
    Category,   // 数据分类节点 (Level 3)
    Data        // 数据项节点 (Level 4)
};

/**
 * @brief 连接信息结构体
 */
struct ConnectionInfo
{
    QString alias;    // 连接别名
    QString type;     // 连接类型 (串口/TCP)
    QString port;     // 串口号或IP:端口
    bool isConnected; // 连接状态

    // 串口配置
    int baudRate;
    int dataBits;
    int stopBits;
    int parity;
    int flowControl;

    // TCP配置
    QString tcpAddress;
    int tcpPort;
};

/**
 * @brief 电表信息结构体
 */
struct MeterInfo
{
    QString alias;         // 电表别名
    QString address;       // 通讯地址
    QString operatorCode;  // 操作者代码
    QString password;      // 电能表密码
    bool includeLeadingFE; // 是否包含前导FE
    MeterPhaseType phaseType = MeterPhaseType::ThreePhase;
};

/**
 * @brief 设备树组件
 *
 * 显示电表设备及其数据项的层次结构
 * 层级结构：
 * Level 1: 连接节点（Connection）
 * └─ Level 2: 电表节点（Meter）
 *    │
 *    └─ Level 3: 数据分类节点（Data Category）
 *       │
 *       └─ Level 4: 数据项节点（Data Item）
 */
class DeviceTreeWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 设备树组件构造函数
     * @param parent 父组件
     */
    explicit DeviceTreeWidget(QWidget *parent = nullptr);
    ~DeviceTreeWidget();

    /**
     * @brief 刷新设备树
     */
    void refreshTree();

    /**
     * @brief 添加连接节点
     * @param connectionInfo 连接信息
     */
    void addConnection(const ConnectionInfo &connectionInfo);

    /**
     * @brief 添加电表节点
     * @param connectionAlias 连接别名
     * @param meterInfo 电表信息
     */
    bool addMeter(const QString &connectionAlias, const MeterInfo &meterInfo);

    /**
     * @brief 清空设备树
     */
    void clearTree();

    /**
     * @brief 删除连接节点
     * @param connectionAlias 连接别名
     */
    void deleteConnection(const QString &connectionAlias);

    /**
     * @brief 删除电表节点
     * @param connectionAlias 连接别名
     * @param meterAlias 电表别名
     */
    void deleteMeter(const QString &connectionAlias, const QString &meterAlias);

    /**
     * @brief 设置连接状态
     * @param connectionAlias 连接别名
     * @param connected 连接状态
     */
    void setConnectionStatus(const QString &connectionAlias, bool connected);

    /**
     * @brief 更新连接节点信息
     * @param oldConnectionAlias 旧连接别名
     * @param connectionInfo 新连接信息
     */
    void updateConnection(const QString &oldConnectionAlias, const ConnectionInfo &connectionInfo);

    /**
     * @brief 展开所有节点
     */
    void expandAll();

    /**
     * @brief 折叠所有节点
     */
    void collapseAll();

    /**
     * @brief 检查是否可以添加电表
     * @return 是否可以添加
     */
    bool canAddMeter() const;

    /**
     * @brief 获取当前选中的连接别名
     * @return 连接别名，如果未选中则返回空字符串
     */
    QString getSelectedConnectionAlias() const;

    /**
     * @brief 获取当前选中的电表信息
     * @return 电表信息，如果未选中则返回空
     */
    MeterInfo getSelectedMeterInfo() const;

    /**
     * @brief 检查指定端口是否已连接
     * @param port 端口名称（串口号或IP:端口）
     * @return 如果端口已连接返回true，否则返回false
     */
    bool isPortConnected(const QString &port) const;

    /**
     * @brief 检查指定通信地址是否已存在
     * @param address 通信地址
     * @return 如果地址已存在返回true，否则返回false
     */
    bool isMeterAddressExists(const QString &address) const;

    /**
     * @brief 获取树形控件指针
     * @return QTreeWidget指针
     */
    QTreeWidget *getTreeWidget() const;

signals:
    /**
     * @brief 数据项选中信号
     * @param diCode 数据标识
     */
    void dataItemSelected(const QString &diCode);

    /**
     * @brief 数据项双击信号
     * @param diCode 数据标识
     */
    void dataItemDoubleClicked(const QString &diCode);

    /**
     * @brief 刷新请求信号
     */
    void refreshRequested();

    /**
     * @brief 连接选中信号
     * @param connectionAlias 连接别名
     */
    void connectionSelected(const QString &connectionAlias);

    /**
     * @brief 电表选中信号
     * @param meterAlias 电表别名
     */
    void meterSelected(const QString &meterAlias);

    /**
     * @brief 电表信息选中信号
     * @param meterInfo 电表信息
     */
    void meterInfoSelected(const MeterInfo &meterInfo);

    /**
     * @brief 数据分类选中信号
     * @param categoryName 分类名称
     * @param meterAlias 电表别名
     */
    void categorySelected(const QString &categoryName, const QString &meterAlias);

    /**
     * @brief 打开连接信号
     * @param connectionAlias 连接别名
     */
    void openConnectionRequested(const QString &connectionAlias);

    /**
     * @brief 关闭连接信号
     * @param connectionAlias 连接别名
     */
    void closeConnectionRequested(const QString &connectionAlias);

    /**
     * @brief 删除连接信号
     * @param connectionAlias 连接别名
     */
    void deleteConnectionRequested(const QString &connectionAlias);
    void deleteAllMetersRequested(const QString &connectionAlias);

    /**
     * @brief 编辑连接信号
     * @param connectionAlias 连接别名
     */
    void editConnectionRequested(const QString &connectionAlias);

    /**
     * @brief 新建电表信号
     * @param connectionAlias 连接别名
     */
    void newMeterRequested(const QString &connectionAlias);

    void batchNewMeterRequested(const QString &connectionAlias);

    /**
     * @brief 读取所有数据信号
     * @param meterAlias 电表别名
     */
    void readAllDataRequested(const QString &meterAlias);

    /**
     * @brief 批量设置数据信号
     * @param meterAlias 电表别名
     */
    void batchSetDataRequested(const QString &meterAlias);

    /**
     * @brief 删除电表信号
     * @param connectionAlias 连接别名
     * @param meterAlias 电表别名
     */
    void deleteMeterRequested(const QString &connectionAlias, const QString &meterAlias);

    /**
     * @brief 读取数据项信号
     * @param diCode 数据标识
     */
    void readDataItemRequested(const QString &diCode);

    /**
     * @brief 写入数据项信号
     * @param diCode 数据标识
     */
    void writeDataItemRequested(const QString &diCode);

    /**
     * @brief 添加监视项信号
     * @param diCode 数据标识
     */
    void addWatchItemRequested(const QString &diCode);

    /**
     * @brief 导出数据项信号
     * @param diCode 数据标识
     */
    void exportDataItemRequested(const QString &diCode);

private slots:
    void onItemClicked(QTreeWidgetItem *item, int column);
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onFilterTextChanged(const QString &text);
    void onRefreshClicked();
    void onExpandAllClicked();
    void onCollapseAllClicked();
    void onItemExpanded(QTreeWidgetItem *item);
    void onItemCollapsed(QTreeWidgetItem *item);
    void onCustomContextMenuRequested(const QPoint &pos);

private:
    void setupUI();
    void setupTree();
    void showEmptyState();
    void hideEmptyState();
    void createDataCategoryNodes(QTreeWidgetItem *meterItem);
    void createSettlementDayNodes(QTreeWidgetItem *parentItem, const QString &rootCategoryName, const QString &name, int settlementDay);
    QTreeWidgetItem *createConnectionNode(const ConnectionInfo &info);
    QTreeWidgetItem *createMeterNode(const MeterInfo &info);
    QTreeWidgetItem *createCategoryItem(const QString &name, ItemType type, bool expandable = true);
    QTreeWidgetItem *createDataItem(const QString &diCode, const QString &name);
    void filterTree(const QString &filterText);
    bool itemMatchesFilter(QTreeWidgetItem *item, const QString &filterText);
    void updateConnectionStatusIcon(QTreeWidgetItem *connectionItem, bool connected);
    void updateConnectionMeterCount(QTreeWidgetItem *connectionItem);
    QTreeWidgetItem *findConnectionItem(const QString &connectionAlias);
    QTreeWidgetItem *findMeterItem(const QString &connectionAlias, const QString &meterAlias);

    QTreeWidget *m_treeWidget;
    QLineEdit *m_filterEdit;
    QPushButton *m_refreshButton;
    QPushButton *m_expandAllButton;
    QPushButton *m_collapseAllButton;
    QMenu *m_contextMenu;
    QLabel *m_emptyStateLabel;
    QStackedLayout *m_stackedLayout;
    bool m_isConnected;
};

#endif // DEVICETREEWIDGET_H
