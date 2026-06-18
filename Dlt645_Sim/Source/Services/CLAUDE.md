[根目录](../../../CLAUDE.md) > [Source](../) > **Services**

# Services - 服务层

## 模块职责

服务层提供跨模块的业务能力，封装协议解析、数据生成、配置持久化等核心逻辑。该层不直接操作 UI，通过接口与控制器层和模型层交互，实现业务逻辑的复用与解耦。

**核心职责**：
- DLT645-2007 协议帧的解析与封装
- 模拟电表数据的生成与自动刷新
- 电表数据持久化存储
- 配置文件的序列化与反序列化
- 应答数据的智能构建

## 入口与启动

服务实例通常由控制器或主窗口创建并持有：

```cpp
// 在 MainWindow 或 ProtocolController 中
m_protocolParser = new ProtocolParser(this);
m_dataGenerator = new DataGenerator(this);
m_meterDataStore = new MeterDataStore(this);
m_configSerializer = new ConfigurationSerializer(this);
```

## 对外接口

### ProtocolParser（协议解析服务）

**核心方法**：
- `parseFrame(QByteArray data, ProtocolFrame &frame)` - 解析接收到的字节流为协议帧
- `buildFrame(ProtocolFrame frame)` - 将协议帧结构体序列化为字节流
- `buildResponseFrame(address, controlCode, dataField)` - 构建应答帧
- `extractDataIdentifier(QByteArray dataField)` - 从数据域提取 DI 码
- `applyDataOffset(QByteArray &data)` - 对数据域执行 +33H 偏移
- `removeDataOffset(QByteArray &data)` - 对数据域执行 -33H 还原

**关键结构**：
```cpp
struct ProtocolFrame {
    quint8 startFlag;           // 0x68
    QByteArray address;         // 6 字节地址域
    quint8 startFlag2;          // 0x68
    quint8 controlCode;         // 控制码
    quint8 dataLength;          // 数据域长度
    QByteArray dataField;       // 数据域（已 -33H）
    quint8 checksum;            // 校验和
    quint8 endFlag;             // 0x16
    int parsedSize;             // 解析的字节数
    bool checksumValid;         // 校验和是否正确
    
    bool isValid() const;       // 检查帧有效性
    QByteArray toByteArray() const;  // 转换为字节流
    QString toString() const;   // 转换为可读字符串
};
```

**控制码枚举**：
```cpp
enum class ControlCode : quint8 {
    ReadData = 0x11,              // 读数据
    ReadNextData = 0x12,          // 读后续数据
    ReadNextDataResponse = 0x92,  // 读后续数据响应
    WriteData = 0x14,             // 写数据
    ReadAddress = 0x13,           // 读通信地址
    WriteAddress = 0x15,          // 写通信地址
    BroadcastTime = 0x16,         // 广播校时
    FreezeData = 0x18,            // 冻结命令
    ClearDemand = 0x1C,           // 最大需量清零
    ClearMeter = 0x1D,            // 电表清零
    // ... 更多控制码
};
```

### DataGenerator（数据生成服务）

**核心方法**：
- `start(int interval)` - 启动定时刷新，默认 1000ms
- `stop()` - 停止刷新
- `setRandomFluctuationEnabled(bool)` - 启用随机波动
- `setVoltageFluctuationRange(double)` - 设置电压波动范围
- `setCurrentFluctuationRange(double)` - 设置电流波动范围
- `triggerUpdate()` - 手动触发一次数据更新

**信号**：
- `dataUpdated()` - 数据已更新（触发 UI 刷新）

**工作原理**：
```cpp
// 定时器触发更新
void DataGenerator::onTimerTimeout() {
    updateVariableData();   // 更新电压、电流、功率等实时变量
    updateEnergyData();     // 累加电能数据
    emit dataUpdated();     // 通知 UI 刷新
}
```

### MeterDataStore（数据存储服务）

**核心方法**：
- `saveMeterData(MeterModel*, QString filePath)` - 保存电表数据到文件
- `loadMeterData(MeterModel*, QString filePath)` - 从文件加载数据
- `exportToCSV(MeterModel*, QString filePath)` - 导出为 CSV 格式
- `exportToJSON(MeterModel*, QString filePath)` - 导出为 JSON 格式
- `importFromJSON(MeterModel*, QString filePath)` - 从 JSON 导入

**存储格式**（JSON 示例）：
```json
{
  "meterAddress": "123456789012",
  "operatorCode": "00000000",
  "password": "00000000",
  "includePrefixFE": true,
  "phaseType": "ThreePhase",
  "dataItems": [
    {
      "diCode": "00000000",
      "name": "(当前)组合有功总电能",
      "value": "123456.78",
      "unit": "kWh",
      "format": "F9,AA,AA,.,AA"
    }
  ]
}
```

### ConfigurationSerializer（配置序列化服务）

**核心方法**：
- `saveConfiguration(QString filePath)` - 保存项目配置
- `loadConfiguration(QString filePath)` - 加载项目配置
- `exportConnections(QString filePath)` - 导出连接配置
- `importConnections(QString filePath)` - 导入连接配置

**配置内容**：
- 所有连接的串口/TCP 参数
- 每个连接下的电表列表
- 电表的地址、密码、相型等信息
- UI 布局状态（窗口位置、分割器比例）

## 关键依赖与配置

### 依赖关系

```
ProtocolParser
    ├─ uses → BCDHelper (BCD 编解码)
    ├─ uses → ChecksumHelper (校验和计算)
    └─ uses → DLT645DataHelper (数据域偏移)

DataGenerator
    ├─ uses → MeterModel (读写数据项)
    └─ uses → QTimer (定时触发)

MeterDataStore
    ├─ uses → MeterModel (数据源)
    └─ uses → QFile / QJsonDocument (文件 I/O)

ConfigurationSerializer
    ├─ uses → ConnectionManager (连接信息)
    ├─ uses → MeterDataStore (电表数据)
    └─ uses → QJsonDocument (序列化)
```

### 协议解析流程

```
原始字节流
    ↓
1. 跳过前导 FE FE FE FE
    ↓
2. 查找起始符 0x68
    ↓
3. 解析地址域（6 字节）
    ↓
4. 校验第二个起始符 0x68
    ↓
5. 读取控制码、数据长度
    ↓
6. 读取数据域（长度由步骤 5 确定）
    ↓
7. 对数据域执行 -33H 还原
    ↓
8. 计算校验和并对比
    ↓
9. 校验结束符 0x16
    ↓
构建 ProtocolFrame 对象
```

### 应答帧构建流程

```
接收读数据命令（0x11）
    ↓
1. 提取地址域 → 查找 MeterModel
    ↓
2. 提取 DI 码 → 查询数据值
    ↓
3. 根据格式串编码数据值
    ↓
4. 构建数据域 = DI(4字节) + 编码后的值
    ↓
5. 对数据域执行 +33H 偏移
    ↓
6. 构建应答帧：
   - 控制码 = 0x91（读数据应答）
   - 地址域 = 原地址
   - 数据域 = 步骤 5 结果
    ↓
7. 计算校验和
    ↓
8. 封装为完整帧并发送
```

## 数据模型

### 协议帧结构示例

**读数据请求帧**：
```
FE FE FE FE                    // 前导符
68                             // 起始符
12 34 56 78 90 12              // 地址域（BCD，低位在前）
68                             // 起始符 2
11                             // 控制码（读数据）
04                             // 数据长度
33 33 33 33                    // 数据域（DI=00000000，已+33H）
CS                             // 校验和
16                             // 结束符
```

**读数据应答帧**：
```
FE FE FE FE
68
12 34 56 78 90 12
68
91                             // 应答控制码（0x11 + 0x80）
09                             // 数据长度
33 33 33 33                    // DI（已+33H）
34 45 56 30 34                 // 值（已+33H）
CS
16
```

### 数据生成策略

**电能累加**：
```cpp
// 每秒累加 0.01 kWh
currentEnergy += 0.01 * (interval / 1000.0);
```

**电压波动**：
```cpp
// 220V ± 5V 随机波动
voltage = 220.0 + (QRandomGenerator::global()->bounded(-50, 50) / 10.0);
```

**电流波动**：
```cpp
// 5A ± 0.5A 随机波动
current = 5.0 + (QRandomGenerator::global()->bounded(-50, 50) / 100.0);
```

## 测试与质量

### 协议解析测试

**测试用例**：
- 正常帧解析
- 校验和错误帧
- 不完整帧处理
- 多帧粘连处理
- 前导符缺失情况

**建议方法**：
```cpp
void testParseFrame() {
    QByteArray data = QByteArray::fromHex(
        "68 12 34 56 78 90 12 68 11 04 33 33 33 33 XX 16"
    );
    ProtocolFrame frame;
    bool success = parser->parseFrame(data, frame);
    QVERIFY(success);
    QCOMPARE(frame.controlCode, 0x11);
    QVERIFY(frame.checksumValid);
}
```

### 数据生成测试

- 验证电能累加逻辑正确性
- 验证波动范围在预期内
- 验证定时器触发频率
- 验证信号发射时机

### 性能考虑

- **协议解析**：缓存不完整帧，避免重复解析
- **数据生成**：仅更新变化的数据项，减少信号开销
- **文件 I/O**：使用缓冲写入，避免频繁磁盘操作

## 常见问题

### Q1: 如何处理不完整帧？

A: `ProtocolParser` 内部维护缓冲区，收到数据后追加到缓冲区，尝试解析完整帧，解析成功后从缓冲区移除已处理数据：

```cpp
m_buffer.append(newData);
while (parseFrame(m_buffer, frame)) {
    m_buffer.remove(0, frame.parsedSize);
    emit frameReceived(frame);
}
```

### Q2: 如何支持异常应答（无请求数据）？

A: 在应答构建逻辑中检查数据项是否存在，不存在时构建异常应答帧：

```cpp
if (!meterModel->hasDataItem(diCode)) {
    frame.controlCode = 0xD1;  // 异常应答
    frame.dataField = QByteArray(1, 0x02);  // 错误码：无请求数据
}
```

### Q3: 如何实现数据域偏移（+33H/-33H）？

A: 遍历字节数组，对每个字节执行加减操作：

```cpp
void applyDataOffset(QByteArray &data) {
    for (int i = 0; i < data.size(); ++i) {
        data[i] = (data[i] + 0x33) & 0xFF;
    }
}

void removeDataOffset(QByteArray &data) {
    for (int i = 0; i < data.size(); ++i) {
        data[i] = (data[i] - 0x33) & 0xFF;
    }
}
```

## 相关文件清单

| 文件 | 说明 |
|------|------|
| `ProtocolParser.h/cpp` | 协议解析服务 |
| `DataGenerator.h/cpp` | 数据生成服务 |
| `MeterDataStore.h/cpp` | 数据存储服务 |
| `ConfigurationSerializer.h/cpp` | 配置序列化服务 |

## 变更记录

### [2026-06-17] - 初始文档
- 完成服务层文档结构
- 详细说明协议解析流程
- 添加数据生成策略说明
- 添加配置持久化示例

---

**维护者**：LIEVEZ  
**最后更新**：2026-06-17
