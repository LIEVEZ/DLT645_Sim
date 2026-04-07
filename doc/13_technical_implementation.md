# 技术实现文档

## 1. 技术方案概述

项目采用 Qt5 Widgets 桌面架构，通过 `MainWindow` 装配界面，通过 `MeterModel` 统一维护电表数据，通过控制器和协议解析层处理通信，通过 `MeterDataStore` 将电表快照持久化到 SQLite。

## 2. 核心技术点

### 2.1 UI 实现

- 主界面使用 `QMainWindow + QSplitter`
- 左侧设备树使用 `DeviceTreeWidget`
- 中部数据区域使用 `DataTableWidget`
- 右侧日志区域使用 `OperationLogWidget`

### 2.2 数据模型实现

- `MeterModel` 存放全部 DI 数据
- 通过 `dataChanged`、`dataBatchChanged`、`addressChanged` 发出变更通知
- 随机化、重置、分类获取等逻辑集中在模型层

### 2.3 数据一致性实现

当前已在模型层加入聚合同步逻辑：

- 当前总电能 = 费率 1~4 电能之和
- 日冻结总电能 = 费率 1~4 日冻结电能之和
- 瞬时总有功/无功/视在功率 = A/B/C 相之和
- 正向/反向总最大需量 = 费率 1~4 最大需量之和

### 2.4 时间格式实现

- 最大需量发生时间按 `compactDateTimeToMinutes()` 生成
- 当前采用格式：`yyMMddHHmm`

### 2.5 持久化实现

- `MainWindow::createMeterModel()` 中执行“加载或初始化”
- `MainWindow::attachMeterPersistence()` 中挂接保存逻辑
- 任意数据项变化、批量变化或地址变化后，调用 `MeterDataStore::saveMeterData()`

### 2.6 日志实现

- 所有成功数据库写入通过 `LOG_INFO` 进入统一日志通道
- `MainWindow::onLoggerLogEntryAdded()` 将日志转发到右侧日志视图

## 3. 当前实现细节

### 3.1 随机化当前数据

- `DataTableWidget` 新增“随机化当前数据”按钮
- 通过 `currentTableDiCodes()` 收集当前分类 DI
- 调用 `MeterModel::randomizeData(diCodes)`

### 3.2 数据库存储模式

- 数据库文件：`meter_data.db`
- 每个表地址一张表：`meter_<address>`
- 旧共享表 `meter_data` 已废弃并在初始化阶段删除

## 4. 构建实现

工程文件：`Dlt645_Sim/Dlt645_Sim.pro`

当前关键 Qt 模块：

- `core`
- `gui`
- `widgets`
- `serialport`
- `network`
- `sql`

## 5. 已识别的技术债

- `MainWindow` 有一定“流程汇聚”倾向
- 自动化测试缺失
- 部分历史保留代码存在 `return` 后不可达分支

## 6. 后续实现建议

- 引入 Repository 或 SessionManager 做职责拆分
- 将协议语义对象再抽象一层
- 建立基于 Qt Test 的自动化测试基线

