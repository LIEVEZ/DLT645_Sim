[根目录](../../../CLAUDE.md) > [Source](../) > **Controllers**

# Controllers - 控制器层

## 模块职责

控制器层负责协调通信链路、协议处理和业务逻辑。作为连接物理层（串口/TCP）与应用层（业务模型）的桥梁，处理所有网络 I/O、协议帧解析与电表实例路由。

**核心职责**：
- 管理串口和 TCP 连接的生命周期
- 协调多连接并发访问
- 接收原始字节流并委托协议解析
- 将业务数据组装为协议帧并发送
- 路由协议帧到对应电表模型
- 处理主站查询请求

## 入口与启动

控制器实例由 `MainWindow` 创建和持有，通过 Qt 信号槽机制与 UI 层和模型层通信。

**关键初始化流程**：
```cpp
// MainWindow 构造函数中
m_connectionManager = new ConnectionManager(this);
m_serialController = new SerialController(this);
m_tcpController = new TcpController(this);
m_protocolController = new ProtocolController(this);

// 绑定信号槽
connect(m_serialController, &SerialController::dataReceived, 
        m_protocolController, &ProtocolController::onRawDataReceived);
```

## 对外接口

### ConnectionManager

**核心方法**：
- `addConnection(ConnectionInfo)` - 添加新连接
- `removeConnection(alias)` - 移除连接
- `openConnection(alias)` - 打开连接
- `closeConnection(alias)` - 关闭连接
- `getConnectionInfo(alias)` - 查询连接信息
- `isConnectionOpen(alias)` - 检查连接状态

### SerialController

**核心方法**：
- `openPort(SerialConfig)` - 打开串口
- `closePort()` - 关闭串口
- `sendData(QByteArray)` - 发送数据
- `isOpen()` - 检查串口状态
- `getAvailablePorts()` - 静态方法，列举可用串口

**信号**：
- `dataReceived(QByteArray)` - 收到数据
- `dataSent(QByteArray)` - 已发送数据
- `errorOccurred(QString)` - 发生错误

### TcpController

**核心方法**：
- `connectToHost(QString host, quint16 port)` - 连接 TCP 服务器
- `disconnectFromHost()` - 断开连接
- `sendData(QByteArray)` - 发送数据
- `isConnected()` - 检查连接状态

**信号**：
- `dataReceived(QByteArray)` - 收到数据
- `dataSent(QByteArray)` - 已发送数据
- `connected()` - 连接成功
- `disconnected()` - 连接断开
- `errorOccurred(QString)` - 发生错误

### ProtocolController

**核心方法**：
- `onRawDataReceived(QByteArray)` - 处理原始接收数据
- `sendResponse(ProtocolFrame)` - 发送响应帧
- `setMeterModels(QMap<QString, MeterModel*>)` - 设置电表模型映射

**信号**：
- `frameReceived(ProtocolFrame, QByteArray)` - 解析出完整帧
- `frameSent(ProtocolFrame, QByteArray)` - 已发送帧
- `parseError(QString)` - 解析错误

### MasterQueryController

负责模拟主站发起查询（用于测试）。

**核心方法**：
- `sendReadDataCommand(address, di)` - 发送读数据命令
- `sendWriteDataCommand(address, di, data)` - 发送写数据命令

## 关键依赖与配置

### 依赖关系

```
ConnectionManager
    └─ uses → SerialController / TcpController

ProtocolController
    ├─ uses → ProtocolParser (协议解析)
    ├─ uses → DataGenerator (数据生成)
    └─ uses → MeterModel (电表模型)

MasterQueryController
    └─ uses → SerialController / TcpController
```

### 配置参数

**SerialConfig 结构**：
```cpp
struct SerialConfig {
    QString portName;              // COM3 / /dev/ttyUSB0
    qint32 baudRate;               // 2400 / 9600
    QSerialPort::DataBits dataBits;
    QSerialPort::Parity parity;
    QSerialPort::StopBits stopBits;
    QSerialPort::FlowControl flowControl;
};
```

**TcpConfig 结构**：
```cpp
struct TcpConfig {
    QString host;    // 127.0.0.1
    quint16 port;    // 8080
};
```

## 数据模型

### ProtocolFrame 结构（定义在 ProtocolParser.h）

```cpp
struct ProtocolFrame {
    quint8 startFlag;           // 0x68
    QByteArray address;         // 6 字节地址域
    quint8 startFlag2;          // 0x68
    quint8 controlCode;         // 控制码
    quint8 dataLength;          // 数据域长度
    QByteArray dataField;       // 数据域
    quint8 checksum;            // 校验和
    quint8 endFlag;             // 0x16
    int parsedSize;
    bool checksumValid;
};
```

### 连接状态机

```
[Disconnected] --openConnection--> [Connecting] 
    ↓                                   ↓
    |                            [Connected]
    |                                   ↓
    +------closeConnection------[Disconnecting]
```

## 测试与质量

### 线程安全

- `SerialController` 和 `TcpController` 的数据收发在 Qt 事件循环中执行
- `ProtocolController` 通过信号槽异步处理，避免阻塞 UI 线程
- `ConnectionManager` 使用 `QMutex` 保护连接映射表

### 错误处理

- 串口打开失败：发出 `errorOccurred` 信号，记录到日志
- 协议解析失败：发出 `parseError` 信号，丢弃非法帧
- 超时处理：通过 `QTimer` 实现超时重试机制（计划中）

### 性能考虑

- 原始数据缓冲：使用 `QByteArray` 缓存不完整帧
- 帧解析优化：一次性解析，避免多次遍历
- 日志级别控制：生产环境使用 Info 级别，减少日志开销

## 常见问题

### Q1: 如何支持多连接并发？

A: `ConnectionManager` 维护连接映射表（`QMap<QString, ConnectionInfo>`），每个连接有独立的 `SerialController` 或 `TcpController` 实例。

### Q2: 如何处理地址冲突？

A: `ProtocolController` 根据协议帧的地址域查找对应的 `MeterModel`。如果同一地址挂在不同连接下，需在映射键中包含连接别名（如 `"COM3:123456789012"`）。

### Q3: 如何添加新控制码支持？

A: 在 `ProtocolParser.h` 的 `ControlCode` 枚举中添加新值，然后在 `ProtocolController` 的帧处理逻辑中添加对应分支。

## 相关文件清单

| 文件 | 说明 |
|------|------|
| `ConnectionManager.h/cpp` | 连接管理器 |
| `SerialController.h/cpp` | 串口控制器 |
| `TcpController.h/cpp` | TCP 控制器 |
| `ProtocolController.h/cpp` | 协议控制器 |
| `MasterQueryController.h/cpp` | 主站查询控制器 |

## 变更记录

### [2026-06-17] - 初始文档
- 完成控制器层文档结构
- 明确各控制器职责与接口
- 添加配置示例与常见问题

---

**维护者**：LIEVEZ  
**最后更新**：2026-06-17
