[根目录](../../../CLAUDE.md) > [Source](../) > **ViewModels**

# ViewModels - 视图模型层

## 模块职责

视图模型层作为视图层与模型层之间的桥梁，负责封装 UI 状态逻辑、数据转换和界面交互协调。该层将纯粹的数据模型转换为 UI 友好的展示格式，同时将 UI 事件转换为业务操作，实现视图与业务逻辑的解耦。

**核心职责**：
- 维护主窗口的 UI 状态（连接状态、选中项、工具栏状态）
- 提供 UI 友好的数据访问接口
- 协调多个模型之间的数据交互
- 处理 UI 事件到业务逻辑的映射
- 管理视图层的临时状态

## 入口与启动

ViewModel 实例由主窗口创建并持有：

```cpp
// 在 MainWindow 构造函数中
m_viewModel = new MainWindowViewModel(this);

// 绑定信号槽
connect(m_viewModel, &MainWindowViewModel::connectionStateChanged,
        this, &MainWindow::onConnectionStateChanged);
connect(m_viewModel, &MainWindowViewModel::currentMeterChanged,
        this, &MainWindow::onCurrentMeterChanged);
```

## 对外接口

### MainWindowViewModel（主窗口视图模型）

**状态管理接口**：
- `setConnectionState(bool connected)` - 设置连接状态
- `isConnected()` - 获取连接状态
- `setCurrentConnection(QString alias)` - 设置当前连接
- `getCurrentConnection()` - 获取当前连接
- `setCurrentMeter(MeterInfo)` - 设置当前电表
- `getCurrentMeter()` - 获取当前电表
- `setCurrentCategory(DataCategory)` - 设置当前数据分类
- `getCurrentCategory()` - 获取当前数据分类

**数据转换接口**：
- `getMeterDisplayName(MeterInfo)` - 获取电表显示名称（别名 + 地址）
- `getConnectionDisplayName(ConnectionInfo)` - 获取连接显示名称
- `getCategoryDisplayName(DataCategory)` - 获取分类显示名称
- `formatDataValue(DataItem)` - 格式化数据值为显示文本

**工具栏状态接口**：
- `isNewMeterEnabled()` - "新建电表"按钮是否可用
- `isOpenConnectionEnabled()` - "打开连接"按钮是否可用
- `isCloseConnectionEnabled()` - "关闭连接"按钮是否可用
- `isCommunicationEnabled()` - "通信交互"按钮是否可用

**信号**：
- `connectionStateChanged(bool connected)` - 连接状态变更
- `currentConnectionChanged(QString alias)` - 当前连接变更
- `currentMeterChanged(MeterInfo)` - 当前电表变更
- `currentCategoryChanged(DataCategory)` - 当前分类变更
- `toolBarStateChanged()` - 工具栏状态变更

## 关键依赖与配置

### 依赖关系

```
MainWindowViewModel
    ├─ uses → MeterModel (数据访问)
    ├─ uses → ConnectionManager (连接信息)
    ├─ uses → CategoryCatalog (分类信息)
    └─ emits signals → MainWindow (状态通知)
```

### 状态管理设计

```cpp
class MainWindowViewModel : public QObject {
private:
    // UI 状态
    bool m_connected = false;
    QString m_currentConnectionAlias;
    MeterInfo m_currentMeterInfo;
    DataCategory m_currentCategory = DataCategory::Energy;
    
    // 引用外部服务
    ConnectionManager *m_connectionManager;
    QMap<QString, MeterModel*> *m_meterModels;
};
```

### 工具栏状态逻辑

```cpp
bool MainWindowViewModel::isNewMeterEnabled() {
    // 必须有当前连接
    return !m_currentConnectionAlias.isEmpty();
}

bool MainWindowViewModel::isOpenConnectionEnabled() {
    // 有连接且未连接时可打开
    return !m_currentConnectionAlias.isEmpty() && !m_connected;
}

bool MainWindowViewModel::isCloseConnectionEnabled() {
    // 已连接时可关闭
    return m_connected;
}

bool MainWindowViewModel::isCommunicationEnabled() {
    // 已连接时可打开通信交互窗口
    return m_connected;
}
```

## 数据模型

### 视图状态转换

**连接状态转换**：
```
用户点击"打开连接"
    ↓
MainWindow::onOpenConnection()
    ↓
ConnectionManager::openConnection()
    ↓
SerialController::openPort() / TcpController::connectToHost()
    ↓
emit connectionStateChanged(true)
    ↓
MainWindowViewModel::setConnectionState(true)
    ↓
emit connectionStateChanged(true)
    ↓
MainWindow::updateToolBarButtons()
    ↓
UI 更新："打开连接"按钮隐藏，"关闭连接"按钮显示
```

**电表选择转换**：
```
用户在设备树选择电表
    ↓
DeviceTreeWidget::emit meterSelected(meterAlias)
    ↓
MainWindow::onMeterSelected(meterAlias)
    ↓
查找 MeterInfo
    ↓
MainWindowViewModel::setCurrentMeter(meterInfo)
    ↓
emit currentMeterChanged(meterInfo)
    ↓
MainWindow::applyCurrentMeter(meterInfo)
    ↓
DataTableWidget::loadData(meterModel, category)
    ↓
UI 更新：数据表格刷新
```

### 数据格式转换示例

**电表显示名称**：
```cpp
QString MainWindowViewModel::getMeterDisplayName(const MeterInfo &info) {
    return QString("%1 (%2)").arg(info.alias).arg(info.address);
    // 结果："电表A (123456789012)"
}
```

**连接显示名称**：
```cpp
QString MainWindowViewModel::getConnectionDisplayName(const ConnectionInfo &info) {
    if (info.type == ConnectionType::Serial) {
        return QString("串口: %1").arg(info.portName);
        // 结果："串口: COM3"
    } else {
        return QString("TCP: %1:%2").arg(info.host).arg(info.port);
        // 结果："TCP: 127.0.0.1:8080"
    }
}
```

**数据值格式化**：
```cpp
QString MainWindowViewModel::formatDataValue(const DataItem &item) {
    QString formatted = item.value.toString();
    if (!item.unit.isEmpty()) {
        formatted += " " + item.unit;
    }
    return formatted;
    // 结果："220.0 V" 或 "+123456.78 kWh"
}
```

## 测试与质量

### 状态一致性保证

- 所有状态变更都通过 ViewModel 集中管理
- 状态变更必须触发对应信号通知 UI
- UI 不直接修改 ViewModel 状态，只通过接口调用

### 单元测试建议

```cpp
void testConnectionStateChange() {
    MainWindowViewModel vm;
    QSignalSpy spy(&vm, &MainWindowViewModel::connectionStateChanged);
    
    vm.setConnectionState(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(vm.isConnected(), true);
    QCOMPARE(vm.isOpenConnectionEnabled(), false);
    QCOMPARE(vm.isCloseConnectionEnabled(), true);
}

void testToolBarState() {
    MainWindowViewModel vm;
    
    // 初始状态：无连接
    QCOMPARE(vm.isNewMeterEnabled(), false);
    QCOMPARE(vm.isOpenConnectionEnabled(), false);
    
    // 设置当前连接
    vm.setCurrentConnection("COM3");
    QCOMPARE(vm.isNewMeterEnabled(), true);
    QCOMPARE(vm.isOpenConnectionEnabled(), true);
    
    // 连接后
    vm.setConnectionState(true);
    QCOMPARE(vm.isOpenConnectionEnabled(), false);
    QCOMPARE(vm.isCloseConnectionEnabled(), true);
    QCOMPARE(vm.isCommunicationEnabled(), true);
}
```

### 性能考虑

- 避免频繁的信号发射（状态未变化时不发射）
- 数据格式化结果缓存（避免重复计算）
- 惰性加载：仅在需要时查询模型数据

## 设计模式应用

### Observer 模式

```
MeterModel (Subject)
    ↓
    dataChanged signal
    ↓
MainWindowViewModel (Observer)
    ↓
    转换为 UI 友好格式
    ↓
    emit 自己的信号
    ↓
MainWindow (View) 更新 UI
```

### Adapter 模式

ViewModel 充当模型层与视图层的适配器：

```
业务模型（MeterModel）
    ↓
MainWindowViewModel（适配器）
    ├─ 转换数据格式
    ├─ 过滤不必要信息
    └─ 合并多个模型数据
    ↓
视图层（UI 组件）
```

## 常见问题

### Q1: ViewModel 与 Controller 的区别？

A: 
- **ViewModel**：专注于 UI 状态管理和数据格式转换，服务于特定视图
- **Controller**：专注于业务流程协调和通信控制，独立于 UI

### Q2: 是否每个视图都需要 ViewModel？

A: 不一定。简单视图（如纯展示组件）可以直接绑定模型。复杂视图（如主窗口）推荐使用 ViewModel 解耦。

### Q3: ViewModel 应该持有哪些数据？

A: 
- ✅ UI 状态（当前选中项、工具栏状态）
- ✅ 临时展示数据（搜索关键词、过滤条件）
- ❌ 业务核心数据（由 Model 层持有）
- ❌ 通信状态（由 Controller 层持有）

### Q4: 如何避免 ViewModel 膨胀？

A: 
- 按视图拆分 ViewModel（如 MainWindowViewModel、DataTableViewModel）
- 提取可复用的数据转换逻辑到独立工具类
- 避免在 ViewModel 中编写复杂业务逻辑

## 扩展方向

### 计划中的 ViewModel

**DataTableViewModel**（数据表格视图模型）：
```cpp
class DataTableViewModel : public QObject {
public:
    // 搜索与过滤
    void setSearchKeyword(QString keyword);
    void setFilterCategory(DataCategory);
    QList<DataItem> getFilteredItems();
    
    // 排序
    void sortByColumn(int column, Qt::SortOrder order);
    
    // 批量操作
    void selectAll();
    void clearSelection();
    QList<DataItem> getSelectedItems();
};
```

**DeviceTreeViewModel**（设备树视图模型）：
```cpp
class DeviceTreeViewModel : public QObject {
public:
    // 树形结构
    QList<ConnectionNode> getConnectionNodes();
    QList<MeterNode> getMeterNodes(QString connectionAlias);
    
    // 展开/折叠状态
    void setExpanded(QString nodeId, bool expanded);
    bool isExpanded(QString nodeId);
    
    // 搜索
    void setSearchKeyword(QString keyword);
    QList<QString> getMatchedNodeIds();
};
```

## 相关文件清单

| 文件 | 说明 |
|------|------|
| `MainWindowViewModel.h/cpp` | 主窗口视图模型 |

## 变更记录

### [2026-06-17] - 初始文档
- 完成视图模型层文档结构
- 详细说明 MainWindowViewModel 接口
- 添加状态转换流程说明
- 添加设计模式应用说明

---

**维护者**：LIEVEZ  
**最后更新**：2026-06-17
