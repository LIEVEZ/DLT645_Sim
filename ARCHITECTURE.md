# DLT645-2007 电表模拟器架构文档

## 1. 文档目标

本文档用于说明项目当前代码结构、核心模块职责、主要数据流与扩展方向。内容基于现有工程文件 [`Dlt645_Sim/Dlt645_Sim.pro`](Dlt645_Sim/Dlt645_Sim.pro)、入口 [`main()`](Dlt645_Sim/Source/main.cpp:5)、主窗口声明 [`MainWindow`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:35) 以及模型定义 [`MeterModel`](Dlt645_Sim/Source/Models/MeterModel.h:28) 进行整理。

本文档关注“当前实现架构”，用于：
- 帮助新成员快速理解项目分层
- 为后续补全 DLT645 协议能力提供结构参考
- 为 UI、协议、数据模型的解耦演进提供依据

---

## 2. 项目概览

该项目是一个基于 `Qt Widgets` 的桌面应用，用于模拟支持 DLT645-2007 协议的电能表设备。系统同时具备：
- 连接配置能力
- 电表实例管理能力
- 数据项建模与编辑能力
- 协议报文解析与生成能力
- 通信日志与调试展示能力

从工程配置 [`Dlt645_Sim/Dlt645_Sim.pro`](Dlt645_Sim/Dlt645_Sim.pro) 可见，项目当前依赖：
- `Qt Core`
- `Qt Gui`
- `Qt Widgets`
- `Qt SerialPort`
- `C++17`

应用入口较为直接：[`main()`](Dlt645_Sim/Source/main.cpp:5) 创建 [`QApplication`](Dlt645_Sim/Source/main.cpp:7)，实例化 [`MainWindow`](Dlt645_Sim/Source/main.cpp:8) 并启动事件循环。

---

## 3. 总体架构

项目整体采用接近 `MVVM + 分层服务` 的组织方式，代码目录已按职责拆分。

### 3.1 分层视图

```text
+-------------------------------------------------------------------+
|                           Qt Widgets UI                           |
| MainWindow / DeviceTreeWidget / DataTableWidget / Dialogs / Log   |
+-------------------------------------------------------------------+
|                         ViewModel / UI协调层                       |
|                    MainWindowViewModel                            |
+-------------------------------------------------------------------+
|                      Controller / Service层                        |
|   SerialController / ProtocolController / ProtocolParser          |
|   DataGenerator / Logger                                          |
+-------------------------------------------------------------------+
|                           Domain Model层                           |
|         MeterModel / DataItem / ConnectionConfig / MeterInfo      |
+-------------------------------------------------------------------+
|                        协议与基础工具层                            |
|         BCDHelper / ChecksumHelper / DLT645DataHelper             |
+-------------------------------------------------------------------+
```

### 3.2 当前架构特征

1. UI 已形成明显模块化拆分，主窗口负责装配，不把所有控件堆在一个类中。
2. 电表数据通过 [`MeterModel`](Dlt645_Sim/Source/Models/MeterModel.h:28) 集中管理，避免视图直接维护业务数据。
3. 协议收发、解析、数据生成被拆分为多个控制器/服务类，便于后续替换通信介质或补协议细节。
4. 工具类承载 BCD、校验和、645 数据编解码等基础能力，使业务层不直接处理底层格式细节。

---

## 4. 目录结构与职责划分

### 4.1 顶层目录

- [`Dlt645_Sim/`](Dlt645_Sim/)：Qt 工程目录
- [`readme.md`](readme.md)：设计规格与目标说明
- [`CURRENT_IMPLEMENTATION.md`](CURRENT_IMPLEMENTATION.md)：当前实现状态说明
- [`ARCHITECTURE.md`](ARCHITECTURE.md)：本架构文档

### 4.2 源码目录结构

源码位于 [`Dlt645_Sim/Source/`](Dlt645_Sim/Source)。

#### 4.2.1 入口层
- [`main()`](Dlt645_Sim/Source/main.cpp:5)：应用入口，负责启动 Qt 应用并显示主窗口。

#### 4.2.2 Views 视图层
位于 [`Dlt645_Sim/Source/Views/`](Dlt645_Sim/Source/Views/)，负责界面展示与交互触发。

主要模块：
- [`MainWindow`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:35)：界面装配中心与主流程协调入口
- [`DeviceTreeWidget`](Dlt645_Sim/Source/Views/DeviceTree/DeviceTreeWidget.cpp)：左侧连接/电表/分类树
- [`DataTableWidget`](Dlt645_Sim/Source/Views/DataTable/DataTableWidget.cpp)：中部数据表展示、编辑、导出
- [`OperationLogWidget`](Dlt645_Sim/Source/Views/OperationLog/OperationLogWidget.cpp)：右侧操作日志与过滤导出
- [`CommunicationDialog`](Dlt645_Sim/Source/Views/Dialogs/CommunicationDialog.cpp)：通信交互窗口
- [`NewConnectionDialog`](Dlt645_Sim/Source/Views/Dialogs/NewConnectionDialog.cpp)：连接配置对话框
- [`NewMeterDialog`](Dlt645_Sim/Source/Views/Dialogs/NewMeterDialog.cpp)：电表配置对话框
- [`CommunicationBarWidget`](Dlt645_Sim/Source/Views/CommunicationBar/CommunicationBarWidget.cpp)：通信相关展示部件

#### 4.2.3 ViewModels 视图模型层
位于 [`Dlt645_Sim/Source/ViewModels/`](Dlt645_Sim/Source/ViewModels/)。

主要模块：
- [`MainWindowViewModel`](Dlt645_Sim/Source/ViewModels/MainWindowViewModel.h)：承载主窗口状态组织和界面交互数据桥接。

#### 4.2.4 Models 领域模型层
位于 [`Dlt645_Sim/Source/Models/`](Dlt645_Sim/Source/Models/)。

主要模块：
- [`MeterModel`](Dlt645_Sim/Source/Models/MeterModel.h:28)：电表核心数据模型
- [`DataItem`](Dlt645_Sim/Source/Models/DataItem.h)：单个 DI 数据项定义
- [`ConnectionConfig`](Dlt645_Sim/Source/Models/ConnectionConfig.h)：连接配置定义

#### 4.2.5 Controllers 控制层
位于 [`Dlt645_Sim/Source/Controllers/`](Dlt645_Sim/Source/Controllers/)。

主要模块：
- [`SerialController`](Dlt645_Sim/Source/Controllers/SerialController.h)：串口/TCP 连接及底层收发管理
- [`ProtocolController`](Dlt645_Sim/Source/Controllers/ProtocolController.h)：协议级收发协调、帧处理联动

#### 4.2.6 Services 服务层
位于 [`Dlt645_Sim/Source/Services/`](Dlt645_Sim/Source/Services/)。

主要模块：
- [`ProtocolParser`](Dlt645_Sim/Source/Services/ProtocolParser.h)：DLT645 报文解析与封装
- [`DataGenerator`](Dlt645_Sim/Source/Services/DataGenerator.h)：按数据项生成模拟值或应答数据

#### 4.2.7 Utils 工具层
位于 [`Dlt645_Sim/Source/Utils/`](Dlt645_Sim/Source/Utils/)。

主要模块：
- [`BCDHelper`](Dlt645_Sim/Source/Utils/BCDHelper.h)：BCD 编解码辅助
- [`ChecksumHelper`](Dlt645_Sim/Source/Utils/ChecksumHelper.h)：校验和计算
- [`DLT645DataHelper`](Dlt645_Sim/Source/Utils/DLT645DataHelper.h)：645 数据格式辅助
- [`Logger`](Dlt645_Sim/Source/Utils/Logger.h)：统一日志输出

---

## 5. 核心模块说明

## 5.1 主窗口协调中心

[`MainWindow`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:35) 是当前系统的 UI 装配与流程协调核心。

从声明可见，它承担以下责任：
- 初始化界面结构，如 [`setupUI()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:73)
- 初始化信号槽连接，如 [`setupConnections()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:74)
- 初始化菜单、工具栏、状态栏，如 [`setupMenuBar()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:75)、[`setupToolBar()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:76)、[`setupStatusBar()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:77)
- 响应新建连接、新建电表、打开连接、关闭连接等入口，如 [`onNewConnection()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:44)、[`onNewMeterDialog()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:45)、[`onOpenConnection()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:46)、[`onCloseConnection()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:47)
- 协调协议帧与原始报文展示，如 [`onProtocolFrameReceived()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:51)、[`onRawDataReceived()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:53)
- 管理当前电表模型切换，如 [`createMeterModel()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:87)、[`applyCurrentMeter()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:88)、[`clearCurrentMeter()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:89)
- 管理实时数据摘要刷新，如 [`updateRealTimeDataDisplay()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:91)

从成员变量看，[`MainWindow`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:35) 直接持有：
- [`SerialController`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:95)
- [`ProtocolController`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:96)
- [`DataGenerator`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:97)
- [`MainWindowViewModel`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:98)
- [`OperationLogWidget`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:99)
- [`DataTableWidget`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:100)
- 以及若干状态栏、工具栏、定时器、通信对话框对象

这说明当前主窗口是“组装器 + 协调器”角色，而不是单纯的壳层。

---

## 5.2 电表领域模型

[`MeterModel`](Dlt645_Sim/Source/Models/MeterModel.h:28) 是项目业务核心。

### 5.2.1 职责

它负责：
- 维护电表地址、操作者代码、密码、前导 FE 配置
- 管理所有 DI 数据项集合
- 根据分类返回数据项列表
- 提供数据查询、修改、重置、随机化接口
- 在数据变更时通过信号通知视图刷新

### 5.2.2 分类设计

数据分类由 [`DataCategory`](Dlt645_Sim/Source/Models/MeterModel.h:12) 枚举定义：
- `Energy`
- `Demand`
- `Variable`
- `Event`
- `Parameter`
- `Freeze`
- `LoadRecord`

### 5.2.3 关键接口

关键公开接口包括：
- [`setMeterAddress()`](Dlt645_Sim/Source/Models/MeterModel.h:40)
- [`getMeterAddress()`](Dlt645_Sim/Source/Models/MeterModel.h:46)
- [`getDataItem()`](Dlt645_Sim/Source/Models/MeterModel.h:89)
- [`getDataValue()`](Dlt645_Sim/Source/Models/MeterModel.h:96)
- [`setDataValue()`](Dlt645_Sim/Source/Models/MeterModel.h:104)
- [`getDataItemsByCategory()`](Dlt645_Sim/Source/Models/MeterModel.h:111)
- [`resetAllData()`](Dlt645_Sim/Source/Models/MeterModel.h:190)
- [`randomizeData()`](Dlt645_Sim/Source/Models/MeterModel.h:195)
- [`initializeDefaultDataItems()`](Dlt645_Sim/Source/Models/MeterModel.h:200)

### 5.2.4 并发考虑

[`MeterModel`](Dlt645_Sim/Source/Models/MeterModel.h:225) 使用 [`QMutex`](Dlt645_Sim/Source/Models/MeterModel.h:225) 保护内部数据容器，说明设计上已经考虑到数据访问的线程安全问题，为后续异步通信、后台刷新留出了空间。

---

## 5.3 数据项对象

[`DataItem`](Dlt645_Sim/Source/Models/DataItem.h) 表示单个数据标识对象，是数据表显示与协议层访问的共同基础对象。

它通常应包含以下语义：
- DI 编码
- 当前值
- 单位
- 数据格式
- 中文描述
- 分类信息

在当前架构中，[`MeterModel`](Dlt645_Sim/Source/Models/MeterModel.h:230) 通过 `QMap<QString, DataItem>` 持有全部数据项，这意味着系统内部对 DI 的管理方式是“键值索引 + 对象封装”。

---

## 5.4 通信控制层

### 5.4.1 连接控制

[`SerialController`](Dlt645_Sim/Source/Controllers/SerialController.h) 负责底层连接与数据收发，按当前设计需要同时覆盖：
- 串口连接
- TCP 连接

它属于“物理链路适配层”，职责应尽量聚焦在：
- 打开/关闭连接
- 收到原始字节流
- 发送原始字节流
- 上报连接状态变化

### 5.4.2 协议控制

[`ProtocolController`](Dlt645_Sim/Source/Controllers/ProtocolController.h) 位于连接层与业务层之间，负责：
- 将原始报文交给解析器
- 将业务数据组织成协议帧
- 转发发送/接收事件给界面或日志模块
- 将某个电表模型的值映射为具体应答帧

这一层的存在，使得底层通信介质可以变化，而上层界面和数据模型不必感知串口/TCP 细节。

---

## 5.5 协议服务层

### 5.5.1 协议解析

[`ProtocolParser`](Dlt645_Sim/Source/Services/ProtocolParser.h) 负责 DLT645 相关报文的解析和封装，是协议知识最集中的位置之一。

该模块理论上应处理：
- 前导 FE
- 起始符、地址域、控制码、长度域
- 数据域偏移规则
- 校验和与结束符
- 读/写/广播等命令结构

### 5.5.2 数据生成

[`DataGenerator`](Dlt645_Sim/Source/Services/DataGenerator.h) 负责构造模拟数据，可能承担：
- 默认值生成
- 随机值生成
- 按 DI 规则格式化值
- 构建应答数据域

这一模块是“模拟器”区别于普通协议分析工具的关键，后续若要增强真实性，应把更多“电表行为规则”沉淀到这里。

---

## 5.6 工具层

### 5.6.1 BCD 与数据格式

[`BCDHelper`](Dlt645_Sim/Source/Utils/BCDHelper.h) 与 [`DLT645DataHelper`](Dlt645_Sim/Source/Utils/DLT645DataHelper.h) 负责处理：
- 十进制数字与 BCD 的互转
- 数据域偏移编码/还原
- 地址、密码、操作者代码等固定格式字段处理

### 5.6.2 校验处理

[`ChecksumHelper`](Dlt645_Sim/Source/Utils/ChecksumHelper.h) 负责帧校验和计算。

### 5.6.3 日志处理

[`Logger`](Dlt645_Sim/Source/Utils/Logger.h) 负责统一日志发布，主窗口中的 [`onLoggerLogEntryAdded()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:57) 表明日志系统已与 UI 建立联动。

---

## 6. 主要运行流程

## 6.1 应用启动流程

```text
main()
  -> 创建 QApplication
  -> 创建 MainWindow
  -> MainWindow.setupUI()
  -> MainWindow.setupConnections()
  -> 初始化控制器/服务/视图
  -> 显示主窗口
```

对应代码入口见 [`main()`](Dlt645_Sim/Source/main.cpp:5)。

---

## 6.2 新建连接流程

```text
用户点击“新建连接”
  -> MainWindow.onNewConnection()
  -> 打开 NewConnectionDialog
  -> 用户填写串口/TCP参数
  -> 生成连接配置
  -> 更新设备树中的连接节点
  -> 根据需要交由 SerialController 建立连接
```

涉及主要入口：[`onNewConnection()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:44)。

---

## 6.3 新建电表流程

```text
用户点击“新建电表”
  -> MainWindow.onNewMeterDialog()
  -> 打开 NewMeterDialog
  -> 输入别名、地址、密码、操作者代码
  -> MainWindow.createMeterModel()
  -> MeterModel.initializeDefaultDataItems()
  -> 电表加入当前连接上下文
  -> 左侧设备树刷新，表格可展示对应分类数据
```

涉及主要入口：[`onNewMeterDialog()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:45)、[`createMeterModel()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:87)、[`initializeDefaultDataItems()`](Dlt645_Sim/Source/Models/MeterModel.h:200)。

---

## 6.4 分类数据展示流程

```text
用户在 DeviceTreeWidget 中选择某电表分类
  -> MainWindow.onMeterInfoSelected() / onMeterSelected()
  -> MainWindow.applyCurrentMeter()
  -> DataTableWidget 读取 MeterModel 对应分类数据
  -> 表格展示 DI、值、单位、格式、描述
```

主要入口见：[`onMeterSelected()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:61)、[`onMeterInfoSelected()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:62)、[`applyCurrentMeter()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:88)。

---

## 6.5 数据修改流程

```text
用户在 DataTableWidget 修改某项值
  -> DataTableWidget 提交修改请求
  -> MeterModel.setDataValue()
  -> MeterModel 发出 dataChanged 信号
  -> MainWindow / 相关视图刷新
  -> 日志模块记录操作
```

关键接口：[`setDataValue()`](Dlt645_Sim/Source/Models/MeterModel.h:104)、[`dataChanged`](Dlt645_Sim/Source/Models/MeterModel.h:208)。

---

## 6.6 协议接收与响应流程

```text
串口/TCP收到原始字节流
  -> SerialController 上报原始数据
  -> ProtocolController 接收并协调处理
  -> ProtocolParser 解析为协议帧
  -> 根据地址与DI定位 MeterModel
  -> DataGenerator / MeterModel 提供应答数据
  -> ProtocolParser 组帧
  -> SerialController 发送字节流
  -> MainWindow / CommunicationDialog / 日志面板展示收发结果
```

从 [`MainWindow`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:51) 中的 [`onProtocolFrameReceived()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:51)、[`onProtocolFrameSent()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:52)、[`onRawDataReceived()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:53)、[`onRawDataSent()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:54) 可以确认，协议层与原始收发层都已经为界面调试预留了观察点。

---

## 7. 数据组织方式

## 7.1 核心对象关系

```text
Connection
  ├─ MeterInfo
  │    └─ MeterModel
  │         ├─ address / password / operatorCode / includePrefixFE
  │         └─ QMap<diCode, DataItem>
  └─ UI tree nodes
```

## 7.2 多电表管理方式

[`MainWindow`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:121) 中使用 `QMap<QString, MeterModel *> m_meterModelsByAddress` 管理电表模型，说明当前以“电表地址”为主键维护实例。

这种方式的优点：
- 地址天然符合协议寻址语义
- 便于收到报文后快速路由到目标电表

需要注意的点：
- 若未来允许同地址测试场景，需要额外区分连接上下文
- 若地址可动态修改，需要同步维护索引更新

---

## 8. UI 结构说明

## 8.1 主布局

根据现有文档和声明，主界面大体结构为：
- 菜单栏
- 工具栏
- 实时数据摘要栏
- 主工作区 `QSplitter`
  - 左：设备树
  - 中：数据表
  - 右：操作日志
- 底部状态栏

在 [`MainWindow`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:102) 中可见主布局依赖 [`QSplitter`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:102) 与 [`QStackedWidget`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:104) 管理区域布局和连接按钮状态。

## 8.2 主题机制

主窗口显式提供：
- [`applyDarkTheme()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:78)
- [`applyLightTheme()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:79)
- [`onToggleDarkTheme()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:69)
- [`onToggleLightTheme()`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:70)

这意味着主题切换已被视为一等能力，而不是散落样式表的临时实现。

## 8.3 实时数据摘要

[`MainWindow`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:107) 到 [`MainWindow`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:110) 中维护实时数据栏控件与定时器，说明关键测量值摘要采用定时轮播或定时刷新方式呈现。

---

## 9. 当前架构优势

### 9.1 模块边界相对清晰
- 视图、模型、控制器、服务、工具已经目录分离。
- 对于 Qt 桌面项目而言，这种结构已经明显优于单文件堆积式开发。

### 9.2 UI 骨架成熟
- 主窗口、设备树、数据表、日志区域均已独立组件化。
- 后续新增功能时，可在现有部件上扩展，不必重构整体框架。

### 9.3 协议能力具备扩展空间
- [`ProtocolController`](Dlt645_Sim/Source/Controllers/ProtocolController.h) + [`ProtocolParser`](Dlt645_Sim/Source/Services/ProtocolParser.h) 的拆分，为复杂报文场景留出了空间。

### 9.4 数据模型具备统一入口
- 所有数据项统一由 [`MeterModel`](Dlt645_Sim/Source/Models/MeterModel.h:28) 管理，便于后续做导入、导出、持久化、批量操作。

---

## 10. 当前架构风险与限制

### 10.1 主窗口职责偏重
[`MainWindow`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:35) 当前承担了：
- 界面装配
- 业务流程协调
- 模型缓存管理
- 日志联动
- 通信窗口联动
- 工具栏状态控制

随着功能继续增加，主窗口容易膨胀为“上帝类”。

### 10.2 连接上下文与电表索引可能耦合不足
当前从成员命名看，电表模型主要按地址索引。如果未来支持：
- 多连接并存
- 同地址不同连接
- 批量广播与上下文切换

则需要将“连接 + 地址”组合作为更稳定的模型键。

### 10.3 协议知识可能分散
DLT645 的规则若同时分布在：
- [`ProtocolParser`](Dlt645_Sim/Source/Services/ProtocolParser.h)
- [`DataGenerator`](Dlt645_Sim/Source/Services/DataGenerator.h)
- [`DLT645DataHelper`](Dlt645_Sim/Source/Utils/DLT645DataHelper.h)
- [`MeterModel`](Dlt645_Sim/Source/Models/MeterModel.h:28)

后续维护时容易出现“格式处理、业务语义、协议规则”边界模糊的问题。

### 10.4 持久化能力尚未形成明确层次
从当前目录结构看，尚未形成独立 Repository 或 Storage 层。若后续要支持：
- 项目保存/打开
- 电表模板管理
- 配置恢复
- 历史日志归档

建议增加独立持久化模块。

---

## 11. 后续推荐演进方向

## 11.1 将主窗口进一步瘦身
建议把以下能力逐步迁出 [`MainWindow`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:35)：
- 连接会话管理器
- 电表注册表
- 页面状态同步逻辑
- 工具栏状态策略

可增加：
- `ConnectionSessionManager`
- `MeterRegistry`
- `MainWindowCoordinator`

## 11.2 建立更明确的协议领域层
建议补一层专门的 645 领域对象，例如：
- `ProtocolFrame`
- `ProtocolCommand`
- `ResponseBuilder`
- `AddressRouter`

这样能把“原始字节解析”和“业务语义映射”分开。

## 11.3 引入持久化层
建议增加：
- `ProjectRepository`
- `MeterProfileRepository`
- `ConnectionConfigRepository`

用于保存连接、电表、数据项默认值和导出模板。

## 11.4 完善多连接多电表上下文模型
推荐将当前模型索引从“仅地址”提升为：
- `connectionAlias + meterAddress`
或使用唯一 `meterId`。

## 11.5 为协议仿真引入行为脚本
若后续要增强教学/测试价值，可引入：
- 负荷变化曲线
- 冻结数据生成策略
- 事件注入策略
- 异常报文模拟策略

这部分可在 [`DataGenerator`](Dlt645_Sim/Source/Services/DataGenerator.h) 基础上逐步演化。

---

## 12. 适合团队协作的理解方式

可以把当前项目理解为 4 个协作面：

### 12.1 UI 面
负责：
- 主界面布局
- 对话框交互
- 数据展示
- 日志与状态展示

对应目录：[`Dlt645_Sim/Source/Views/`](Dlt645_Sim/Source/Views/)

### 12.2 业务模型面
负责：
- 电表实例
- 数据项组织
- 分类映射
- 默认值、随机值、可编辑值管理

对应目录：[`Dlt645_Sim/Source/Models/`](Dlt645_Sim/Source/Models/)

### 12.3 协议与通信面
负责：
- 连接收发
- 报文解析
- 应答组装
- 协议联调

对应目录：[`Dlt645_Sim/Source/Controllers/`](Dlt645_Sim/Source/Controllers/)、[`Dlt645_Sim/Source/Services/`](Dlt645_Sim/Source/Services/)

### 12.4 基础工具面
负责：
- BCD 编码
- 校验和
- 数据格式处理
- 日志基础设施

对应目录：[`Dlt645_Sim/Source/Utils/`](Dlt645_Sim/Source/Utils/)

---

## 13. 总结

当前项目已经具备一个较清晰的桌面工业工具架构雏形：
- 用 [`MainWindow`](Dlt645_Sim/Source/Views/MainWindow/mainwindow.h:35) 作为装配入口
- 用 [`MeterModel`](Dlt645_Sim/Source/Models/MeterModel.h:28) 作为业务核心
- 用 [`SerialController`](Dlt645_Sim/Source/Controllers/SerialController.h) 和 [`ProtocolController`](Dlt645_Sim/Source/Controllers/ProtocolController.h) 承接通信与协议调度
- 用 [`ProtocolParser`](Dlt645_Sim/Source/Services/ProtocolParser.h) 和 [`DataGenerator`](Dlt645_Sim/Source/Services/DataGenerator.h) 承载协议与模拟逻辑
- 用 [`BCDHelper`](Dlt645_Sim/Source/Utils/BCDHelper.h)、[`ChecksumHelper`](Dlt645_Sim/Source/Utils/ChecksumHelper.h)、[`DLT645DataHelper`](Dlt645_Sim/Source/Utils/DLT645DataHelper.h) 等工具模块提供底层支撑

从软件工程角度看，这个项目最重要的优势不是“某个功能已经完全做完”，而是 **结构已经适合持续迭代**。后续只要继续沿着“模型统一、协议集中、主窗口瘦身、持久化补齐”的方向演进，就能逐步从“可用原型”提升到“完整模拟平台”。
