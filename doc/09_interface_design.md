# 接口设计文档

## 1. 文档范围

本项目没有对外 Web API，接口设计主要包括：

- UI 组件之间的信号槽接口
- 控制器与模型之间的内部接口
- 持久化服务接口
- 协议收发接口

## 2. UI 交互接口

### 2.1 DeviceTreeWidget -> MainWindow

| 信号 | 说明 |
| --- | --- |
| `connectionSelected` | 选中连接 |
| `meterSelected` | 选中电表 |
| `meterInfoSelected` | 选中电表详细信息 |
| `categorySelected` | 选中数据分类 |
| `openConnectionRequested` | 请求打开连接 |
| `closeConnectionRequested` | 请求关闭连接 |
| `deleteConnectionRequested` | 请求删除连接 |
| `newMeterRequested` | 请求新增电表 |
| `deleteMeterRequested` | 请求删除电表 |

### 2.2 DataTableWidget -> MeterModel

| 行为 | 说明 |
| --- | --- |
| `setMeterModel` | 绑定当前电表模型 |
| `refreshTable` | 刷新当前分类数据 |
| `onRandomizeClicked` | 随机化全部数据 |
| `onRandomizeCurrentClicked` | 随机化当前列表数据 |

## 3. 持久化接口

类：`MeterDataStore`

| 方法 | 说明 |
| --- | --- |
| `isReady()` | 数据库是否就绪 |
| `hasMeterData(address)` | 是否存在该地址数据 |
| `loadMeterData(model)` | 从数据库加载到模型 |
| `saveMeterData(model)` | 将模型快照保存到数据库 |

## 4. 模型接口

类：`MeterModel`

| 方法 | 说明 |
| --- | --- |
| `setMeterAddress()` | 设置电表地址 |
| `getDataValue(di)` | 读取单项值 |
| `setDataValue(di, value)` | 写入单项值 |
| `randomizeData()` | 随机化全部 |
| `randomizeData(diCodes)` | 随机化指定列表 |
| `resetAllData()` | 重置全部 |
| `applyPersistedData(snapshot)` | 应用数据库快照 |

## 5. 日志接口

类：`Logger`

日志流转：

```text
业务代码 -> Logger::instance()
         -> logEntryAdded
         -> MainWindow::onLoggerLogEntryAdded
         -> OperationLogWidget
```

## 6. 通信接口

| 模块 | 接口说明 |
| --- | --- |
| `SerialController` | 串口打开、关闭、读写 |
| `TcpController` | TCP 打开、关闭、读写 |
| `ProtocolController` | 帧接收、帧发送、原始数据收发通知 |
| `ProtocolParser` | 协议帧解析与封装 |

## 7. 数据格式约定

- 电表地址：12 位字符串
- 时间紧凑格式：`yyMMddHHmm`
- 变更日志：以文本消息输出到 `OperationLogWidget`
- 数据库存值：`QVariant` 序列化为 `value_text + value_type`

