# 系统架构设计文档

## 1. 架构目标

- 提供清晰的 UI、通信、协议、数据模型和持久化分层
- 支持后续继续增加 DI、协议能力和持久化策略
- 保持桌面工具的可维护性和可扩展性

## 2. 总体分层

```text
UI 层
  MainWindow / DeviceTreeWidget / DataTableWidget / Dialogs / OperationLogWidget

视图模型层
  MainWindowViewModel

控制器层
  SerialController / TcpController / ProtocolController

服务层
  ProtocolParser / DataGenerator / MeterDataStore

领域模型层
  MeterModel / DataItem / ConnectionConfig / MeterInfo / ConnectionInfo

工具层
  Logger / BCDHelper / ChecksumHelper / DLT645DataHelper
```

## 3. 关键模块职责

| 模块 | 职责 |
| --- | --- |
| MainWindow | 主界面装配、流程协调、持久化挂接 |
| DeviceTreeWidget | 连接/电表/分类树导航 |
| DataTableWidget | 分类数据展示、编辑、随机化、导出 |
| MeterModel | 电表数据统一模型、数据同步、信号通知 |
| ProtocolController | 协议级协调与路由 |
| SerialController / TcpController | 通信通道打开、关闭、收发 |
| ProtocolParser | DLT645 报文解析与封装 |
| MeterDataStore | SQLite 本地持久化 |
| Logger | 日志统一发布 |

## 4. 启动架构流程

1. `main.cpp` 创建 `QApplication`
2. 创建 `MainWindow`
3. `MainWindow` 初始化 UI、菜单、工具栏、状态栏
4. 装配设备树、数据表、日志区
5. 创建控制器、ViewModel、数据存储
6. 绑定信号槽
7. 启动实时数据轮播与日志系统

## 5. 数据架构

- 以 `MeterModel` 为单块电表内存镜像
- 内部以 `QMap<QString, DataItem>` 管理 DI 数据
- UI 不直接持有业务数据，只通过 `MeterModel` 交互
- SQLite 以表地址分表保存

## 6. 当前架构特点

- 优点：
  - 分层基本清晰
  - 数据模型统一
  - UI 组件边界相对明确
  - 本地持久化已经接入主流程
- 风险：
  - `MainWindow` 仍承担较多编排责任
  - 测试隔离度还不够高
  - 部分遗留代码需要收敛

## 7. 推荐后续演进

- 将连接会话管理进一步从 `MainWindow` 拆出
- 为数据库增加专门的迁移版本管理
- 将协议构帧与业务映射进一步解耦
- 引入单元测试和集成测试自动化

