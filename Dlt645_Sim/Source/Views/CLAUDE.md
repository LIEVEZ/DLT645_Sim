[根目录](../../../CLAUDE.md) > [Source](../) > **Views**

# Views - 视图层

## 模块职责

视图层负责所有 UI 界面的展示与用户交互，采用 Qt Widgets 构建桌面界面。该层通过信号槽机制与视图模型层和控制器层通信，不直接操作业务数据，所有数据访问通过模型层接口完成。

**核心职责**：
- 渲染用户界面组件
- 响应用户输入事件
- 通过信号通知业务层处理请求
- 接收模型变更通知并更新 UI
- 管理界面布局与主题样式

## 入口与启动

应用启动时创建主窗口：

```cpp
// main.cpp
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow mainWindow;
    mainWindow.show();
    return app.exec();
}
```

## 对外接口

### MainWindow（主窗口）

**核心职责**：
- 装配所有 UI 组件（设备树、数据表、日志面板、对话框）
- 协调各组件之间的交互
- 管理菜单栏、工具栏、状态栏
- 处理主题切换
- 管理连接和电表生命周期

**关键方法**：
- `setupUI()` - 初始化界面布局
- `setupConnections()` - 绑定信号槽
- `setupMenuBar()` - 初始化菜单
- `setupToolBar()` - 初始化工具栏
- `setupStatusBar()` - 初始化状态栏
- `applyDarkTheme()` - 应用深色主题
- `applyLightTheme()` - 应用浅色主题

**布局结构**：
```
MainWindow
├─ MenuBar（菜单栏）
├─ ToolBar（工具栏）
├─ RealTimeDataBar（实时数据摘要栏）
├─ QSplitter（主工作区）
│  ├─ DeviceTreeWidget（左侧设备树）
│  ├─ DataTableWidget（中部数据表）
│  └─ OperationLogWidget（右侧日志）
└─ StatusBar（状态栏）
```

### DeviceTreeWidget（设备树）

**核心职责**：
- 展示连接、电表、数据分类的树形结构
- 支持搜索、展开/折叠操作
- 响应节点选择事件
- 提供右键菜单

**信号**：
- `connectionSelected(QString alias)` - 连接被选中
- `meterSelected(QString alias)` - 电表被选中
- `categorySelected(DataCategory)` - 数据分类被选中
- `openConnectionRequested(QString alias)` - 请求打开连接
- `closeConnectionRequested(QString alias)` - 请求关闭连接
- `deleteConnectionRequested(QString alias)` - 请求删除连接
- `deleteMeterRequested(QString connectionAlias, QString meterAlias)` - 请求删除电表

**树形结构**：
```
🔗 COM3连接 🟢
  ├─ 📊 电表1 (123456789012)
  │  ├─ 📁 电能量
  │  │  ├─ 📄 当前
  │  │  ├─ 📄 上1结算日
  │  │  └─ ...
  │  ├─ 📁 最大需量及发生时间
  │  ├─ 📄 变量
  │  ├─ 📄 事件记录
  │  ├─ 📄 参变量
  │  ├─ 📁 冻结
  │  └─ 📄 负荷记录
  └─ 📊 电表2 (987654321098)
```

### DataTableWidget（数据表格）

**核心职责**：
- 展示当前选中分类的数据项列表
- 支持双击编辑数据值
- 提供搜索、过滤、排序功能
- 支持数据导出（CSV/JSON）

**表格列定义**：
| 列名 | 宽度 | 说明 |
|------|------|------|
| 数据标识 | 120px | DI 码（如 00000000） |
| 数据值 | 150px | 当前值（可编辑） |
| 单位 | 80px | 单位（kWh/V/A 等） |
| 数据格式 | 120px | 格式串（F9,AA,AA,.,AA） |
| 数据项描述 | 自适应 | 中文名称 |

**工具栏**：
- 搜索框：支持 DI 码、名称搜索
- 重置所有数据按钮
- 随机化所有数据按钮
- 导出所有数据按钮

**交互**：
- 双击数据值单元格进入编辑模式
- 右键菜单：读取、写入、复制、监视
- 支持键盘导航（方向键、Enter、Esc）

### OperationLogWidget（操作日志）

**核心职责**：
- 实时显示系统操作日志
- 支持日志过滤和搜索
- 自动滚动到最新日志
- 支持导出日志

**日志级别与颜色**：
- 🔵 **Debug**：调试信息（灰色）
- ℹ️ **Info**：一般信息（蓝色）
- ⚠️ **Warning**：警告（橙色）
- ❌ **Error**：错误（红色）
- 🔴 **Critical**：严重错误（深红色）

**工具栏**：
- 过滤输入框
- 自动滚动复选框（默认勾选）
- 清空按钮
- 导出按钮

### CommunicationBarWidget（通信栏）

**核心职责**：
- 显示实时通信统计
- 展示关键数据摘要
- 提供快速操作入口

### Dialogs（对话框集合）

#### NewConnectionDialog（新建连接对话框）

**配置项**：
- 连接模式：串口/TCP（下拉选择）
- 连接别名：自定义名称
- **串口配置**：串口号、波特率、数据位、校验位、停止位、流控
- **TCP 配置**：服务端 IP、服务端端口

**默认值**：
- 波特率：2400
- 数据位：8
- 校验位：偶校验
- 停止位：1
- 流控：无

#### NewMeterDialog（新建电表对话框）

**配置项**：
- 电表别名：自定义名称
- 通讯地址：12 位 BCD 码（如 123456789012）
- 操作者代码：8 位数字（如 00000000）
- 电能表密码：8 位数字（如 00000000）
- 包含前导 FE：复选框

**校验规则**：
- 地址必须 12 位数字
- 操作者代码必须 8 位数字
- 密码必须 8 位数字
- 地址不能重复

#### BatchNewMeterDialog（批量新建电表对话框）

**功能**：
- 起始地址：12 位 BCD 码
- 数量：1-100
- 地址递增规则：自动递增
- 别名前缀：批量命名

#### CommunicationDialog（通信交互窗口）

**核心职责**：
- 实时捕获所有收发报文
- 完整报文显示（HEX 格式）
- 双击报文查看详细解析
- 支持过滤和搜索
- 导出通信记录

**布局**：
```
┌─────────────────────────────────────────┐
│ 连接：COM3 | 波特率：2400 | 状态：已连接 │
├─────────────────────────────────────────┤
│ 过滤：[____________] □读 □写 □异常      │
├─────────────────────────────────────────┤
│ [时间]    [方向] [完整报文(HEX)]         │
│ 20:08:59  ← RX   68 12 34 56 78 90 12.. │
│ 20:09:00  → TX   68 12 34 56 78 90 12.. │
├─────────────────────────────────────────┤
│ 帧详情解析                               │
│ 起始符: 0x68                            │
│ 地址域: 12 34 56 78 90 12               │
│ 控制码: 0x11 (读数据)                   │
│ 数据域: 00 00 00 00                     │
├─────────────────────────────────────────┤
│ 统计：发送 120 帧 | 接收 118 帧 | 错误 2│
└─────────────────────────────────────────┘
```

#### MasterQueryDialog（主站查询窗口）

**功能**：
- 模拟主站发起查询（测试用）
- 手动构造读/写命令
- 查看实时应答

#### EventRecordDialog（事件记录对话框）

**功能**：
- 配置事件记录是否存在
- 模拟事件触发
- 查看事件历史

#### FrameParseDialog（帧解析对话框）

**功能**：
- 粘贴原始报文（HEX）
- 自动解析帧结构
- 显示各字段含义

#### QueryFrameDialog（查询帧生成对话框）

**功能**：
- 选择 DI 码
- 自动生成查询帧
- 复制到剪贴板

### Common（公共组件）

#### FrameParseResultWidget

展示帧解析结果的通用组件。

## 关键依赖与配置

### 依赖关系

```
MainWindow
    ├─ includes → DeviceTreeWidget
    ├─ includes → DataTableWidget
    ├─ includes → OperationLogWidget
    ├─ uses → MainWindowViewModel
    ├─ uses → ProtocolController
    ├─ uses → ConnectionManager
    └─ uses → DataGenerator

DeviceTreeWidget
    ├─ uses → ConnectionManager
    └─ uses → MeterDataStore

DataTableWidget
    ├─ uses → MeterModel
    └─ uses → DataTableExporter

OperationLogWidget
    ├─ uses → Logger
    └─ uses → QTextEdit
```

### 主题样式

**深色主题**（默认）：
```css
QMainWindow {
    background-color: #2B2B2B;
    color: #FFFFFF;
}

QTreeWidget {
    background-color: #3C3F41;
    color: #FFFFFF;
    border: 1px solid #555555;
}

QTableWidget {
    background-color: #3C3F41;
    color: #FFFFFF;
    gridline-color: #555555;
}

QPushButton {
    background-color: #0E639C;
    color: #FFFFFF;
    border: none;
    padding: 5px 15px;
}

QPushButton:hover {
    background-color: #1177BB;
}
```

**浅色主题**：
```css
QMainWindow {
    background-color: #FFFFFF;
    color: #000000;
}

QTreeWidget {
    background-color: #F5F5F5;
    color: #000000;
    border: 1px solid #CCCCCC;
}
```

## 数据模型

### 设备树节点类型

```cpp
enum class NodeType {
    Connection,    // 连接节点
    Meter,         // 电表节点
    Category,      // 数据分类节点
    DataItem       // 数据项节点（叶子节点）
};
```

### 表格模型

```cpp
class DataTableModel : public QAbstractTableModel {
public:
    enum Column {
        DiCode = 0,      // DI 码
        Value,           // 数值
        Unit,            // 单位
        Format,          // 格式串
        Description      // 描述
    };
};
```

## 测试与质量

### UI 测试建议

**功能测试**：
- 新建连接对话框各项配置正确保存
- 串口/TCP 切换正常
- 树形节点展开/折叠流畅
- 数据表格双击编辑功能正常
- 搜索框实时过滤准确
- 主题切换无 UI 错乱
- 窗口缩放自适应正常
- 操作日志自动滚动正常

**性能测试**：
- 1000 行数据表格滚动流畅（≥30fps）
- 窗口调整大小无卡顿
- 大量日志输出不影响 UI 响应

### 可访问性

- 所有按钮有 Tooltip 说明
- 支持键盘快捷键（Ctrl+N/M/W/I 等）
- 支持键盘导航（Tab/方向键）
- 高对比度主题支持

## 常见问题

### Q1: 如何添加新菜单项？

A: 在 `MainWindow::setupMenuBar()` 中添加：

```cpp
QMenu *fileMenu = menuBar()->addMenu("文件(&F)");
QAction *newAction = new QAction("新建(&N)", this);
newAction->setShortcut(QKeySequence::New);
connect(newAction, &QAction::triggered, this, &MainWindow::onNew);
fileMenu->addAction(newAction);
```

### Q2: 如何自定义表格单元格渲染？

A: 使用 `QStyledItemDelegate`：

```cpp
class CustomDelegate : public QStyledItemDelegate {
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        // 自定义绘制逻辑
    }
};

dataTable->setItemDelegateForColumn(1, new CustomDelegate(this));
```

### Q3: 如何处理对话框返回值？

A: 使用模态对话框并检查返回值：

```cpp
NewMeterDialog dialog(this);
if (dialog.exec() == QDialog::Accepted) {
    MeterInfo info = dialog.getMeterInfo();
    // 处理新建电表逻辑
}
```

### Q4: 如何实现自定义右键菜单？

A: 重写 `contextMenuEvent()`：

```cpp
void DeviceTreeWidget::contextMenuEvent(QContextMenuEvent *event) {
    QTreeWidgetItem *item = itemAt(event->pos());
    if (!item) return;
    
    QMenu menu(this);
    menu.addAction("打开连接", this, &DeviceTreeWidget::onOpenConnection);
    menu.addAction("关闭连接", this, &DeviceTreeWidget::onCloseConnection);
    menu.exec(event->globalPos());
}
```

## 相关文件清单

| 文件 | 说明 |
|------|------|
| **MainWindow/** |  |
| `mainwindow.h/cpp` | 主窗口核心逻辑 |
| `mainwindow.ui` | 主窗口界面布局 |
| `mainwindow_setup.cpp` | 界面初始化 |
| `mainwindow_theme.cpp` | 主题管理 |
| `mainwindow_connections.cpp` | 连接管理 |
| `mainwindow_meters.cpp` | 电表管理 |
| `mainwindow_protocol.cpp` | 协议处理 |
| `mainwindow_configuration.cpp` | 配置管理 |
| `mainwindow_dialogs.cpp` | 对话框管理 |
| `mainwindow_status.cpp` | 状态管理 |
| `mainwindow_helpers.cpp` | 辅助方法 |
| **DeviceTree/** |  |
| `DeviceTreeWidget.h/cpp` | 设备树组件 |
| **DataTable/** |  |
| `DataTableWidget.h/cpp` | 数据表格组件 |
| `DataTableExporter.h/cpp` | 数据导出器 |
| **OperationLog/** |  |
| `OperationLogWidget.h/cpp` | 操作日志组件 |
| **Dialogs/** |  |
| `NewConnectionDialog.h/cpp` | 新建连接对话框 |
| `NewMeterDialog.h/cpp` | 新建电表对话框 |
| `BatchNewMeterDialog.h/cpp` | 批量新建电表对话框 |
| `CommunicationDialog.h/cpp` | 通信交互窗口 |
| `MasterQueryDialog.h/cpp` | 主站查询窗口 |
| `EventRecordDialog.h/cpp` | 事件记录对话框 |
| `FrameParseDialog.h/cpp` | 帧解析对话框 |
| `QueryFrameDialog.h/cpp` | 查询帧生成对话框 |
| **CommunicationBar/** |  |
| `CommunicationBarWidget.h/cpp` | 通信栏组件 |
| **Common/** |  |
| `FrameParseResultWidget.h/cpp` | 帧解析结果组件 |

## 变更记录

### [2026-06-17] - 初始文档
- 完成视图层文档结构
- 详细说明所有 UI 组件职责
- 添加布局结构与主题样式
- 添加常见问题与测试建议

---

**维护者**：LIEVEZ  
**最后更新**：2026-06-17
