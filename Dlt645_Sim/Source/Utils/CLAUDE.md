[根目录](../../../CLAUDE.md) > [Source](../) > **Utils**

# Utils - 工具层

## 模块职责

工具层提供底层基础能力，封装 BCD 编解码、校验和计算、数据格式处理、日志记录等通用功能。所有工具类保持无状态设计，可在任意上下文中安全调用，不依赖具体业务逻辑。

**核心职责**：
- BCD 码与十进制的互转
- 校验和计算（模 256 累加）
- DLT645 数据域偏移编解码
- 数据格式串解析与编码
- 协议帧格式化输出
- 线程安全的日志记录

## 入口与启动

工具类通常以静态方法或单例模式提供服务，无需显式初始化：

```cpp
// 静态方法调用
QByteArray bcd = BCDHelper::toBCD("123456");
quint8 cs = ChecksumHelper::calculate(data);

// 单例模式调用
Logger::instance()->info("Application started");
```

## 对外接口

### BCDHelper（BCD 编解码）

**核心方法**：
- `toBCD(QString decimal)` - 十进制字符串转 BCD 字节数组
- `fromBCD(QByteArray bcd)` - BCD 字节数组转十进制字符串
- `isValidBCD(QString str)` - 校验字符串是否为合法 BCD 格式
- `padBCD(QString str, int length)` - 左侧补零到指定长度

**使用示例**：
```cpp
// 电表地址 "123456789012" → BCD 编码
QByteArray bcd = BCDHelper::toBCD("123456789012");
// 结果：12 34 56 78 90 12（6 字节，低位在前）

// BCD 解码
QString address = BCDHelper::fromBCD(QByteArray::fromHex("123456789012"));
// 结果："123456789012"
```

**注意事项**：
- BCD 编码采用低位在前（Little Endian）
- 非数字字符会被忽略
- 奇数长度字符串会自动补零

### ChecksumHelper（校验和计算）

**核心方法**：
- `calculate(QByteArray data)` - 计算校验和（模 256 累加）
- `verify(QByteArray data, quint8 checksum)` - 校验校验和是否正确

**使用示例**：
```cpp
// 计算协议帧校验和（从第一个 68H 到数据域结束）
QByteArray frame = /* ... */;
quint8 cs = ChecksumHelper::calculate(frame);

// 校验接收帧
bool valid = ChecksumHelper::verify(frame, receivedChecksum);
```

**算法**：
```cpp
quint8 ChecksumHelper::calculate(const QByteArray &data) {
    quint8 sum = 0;
    for (quint8 byte : data) {
        sum += byte;
    }
    return sum;
}
```

### DLT645DataHelper（数据域偏移）

**核心方法**：
- `applyOffset(QByteArray &data)` - 对数据域执行 +33H 偏移（发送前）
- `removeOffset(QByteArray &data)` - 对数据域执行 -33H 还原（接收后）
- `offsetByte(quint8 byte)` - 单字节偏移
- `unOffsetByte(quint8 byte)` - 单字节还原

**使用示例**：
```cpp
// 发送前编码
QByteArray dataField = QByteArray::fromHex("00 00 00 00");
DLT645DataHelper::applyOffset(dataField);
// 结果：33 33 33 33

// 接收后解码
QByteArray received = QByteArray::fromHex("33 33 33 33");
DLT645DataHelper::removeOffset(received);
// 结果：00 00 00 00
```

### DLT645FrameFormatter（帧格式化输出）

**核心方法**：
- `formatFrame(ProtocolFrame frame)` - 格式化协议帧为可读字符串
- `formatHex(QByteArray data)` - 格式化字节数组为十六进制
- `formatFields(ProtocolFrame frame)` - 格式化帧各字段
- `parseAndFormat(QByteArray rawData)` - 解析并格式化原始数据

**使用示例**：
```cpp
QString formatted = DLT645FrameFormatter::formatFrame(frame);
// 输出：
// ======== 协议帧 ========
// 起始符: 0x68
// 地址域: 12 34 56 78 90 12
// 控制码: 0x11 (读数据)
// 数据长度: 4
// 数据域: 00 00 00 00
// 校验和: 0xAB
// 结束符: 0x16
// =======================
```

### DLT645BlockHandler（数据块处理）

**核心方法**：
- `splitBlocks(QByteArray data, int blockSize)` - 按块大小拆分数据
- `mergeBlocks(QList<QByteArray> blocks)` - 合并数据块
- `validateBlock(QByteArray block)` - 校验数据块完整性

**使用场景**：
- 处理大数据量读取（如负荷曲线）
- 实现分块传输与后续帧处理

### DLT645ValueFormatter（数值格式化）

**核心方法**：
- `formatValue(QByteArray data, QString formatString)` - 根据格式串解析数据
- `encodeValue(QString value, QString formatString)` - 根据格式串编码数值
- `parseFormatTokens(QString formatString)` - 解析格式串为 token 列表

**格式串语法**：
| Token | 含义 | 示例 |
|-------|------|------|
| `AA` | 直接转换为字符 | `AA,AA` → "1234" |
| `AS` | 转为 ASCII | `AS,AS` → "AB" |
| `xx` | 除以 10 | `xx,.,xx` → "12.34" |
| `F9` | 正负号处理 | `F9,AA,AA` → "-1234" |
| `bin` | 二进制查表 | `bin,tableName` |
| `bit` | 按位查表 | `bit,tableName` |

**使用示例**：
```cpp
// 解析电能数据（格式串 "F9,AA,AA,.,AA"）
QByteArray data = QByteArray::fromHex("12 34 56 78");
QString value = DLT645ValueFormatter::formatValue(data, "F9,AA,AA,.,AA");
// 结果："+123456.78" 或 "-123456.78"

// 编码电能数据
QString value = "+123456.78";
QByteArray encoded = DLT645ValueFormatter::encodeValue(value, "F9,AA,AA,.,AA");
// 结果：12 34 56 78
```

### Logger（日志工具）

**核心方法**：
- `instance()` - 获取单例实例
- `debug(QString message)` - 记录调试日志
- `info(QString message)` - 记录信息日志
- `warning(QString message)` - 记录警告日志
- `error(QString message)` - 记录错误日志
- `critical(QString message)` - 记录严重错误日志
- `setLogFile(QString filePath)` - 设置日志文件路径
- `setLogRotation(qint64 maxSize, int maxBackups)` - 设置日志轮转
- `setLogLevel(LogLevel level)` - 设置最低日志级别
- `setConsoleOutputEnabled(bool)` - 启用/禁用控制台输出
- `setFileOutputEnabled(bool)` - 启用/禁用文件输出

**使用示例**：
```cpp
// 初始化日志系统（在 main 函数中）
Logger *logger = Logger::instance();
logger->setLogFile("dlt645_sim.log");
logger->setLogRotation(10 * 1024 * 1024, 5);  // 10MB, 5 个备份
logger->setLogLevel(LogLevel::Info);

// 记录日志
LOG_INFO("Application started");
LOG_DEBUG("Meter address: 123456789012");
LOG_WARNING("Connection timeout, retrying...");
LOG_ERROR("Failed to parse protocol frame");
```

**日志级别**：
```cpp
enum class LogLevel {
    Debug,      // 调试信息
    Info,       // 一般信息
    Warning,    // 警告
    Error,      // 错误
    Critical    // 严重错误
};
```

**日志格式**：
```
[2026-06-17 20:08:59] [INFO] Application started
[2026-06-17 20:09:01] [DEBUG] Parsing frame: 68 12 34 56 78 90 12...
[2026-06-17 20:09:03] [WARNING] Connection timeout, retrying...
[2026-06-17 20:09:05] [ERROR] Failed to parse protocol frame
```

**信号**：
- `logEntryAdded(LogLevel, QDateTime, QString)` - 新日志添加（用于 UI 实时显示）

**线程安全**：
- 使用 `QMutex` 保护日志写入
- 支持多线程并发记录

## 关键依赖与配置

### 依赖关系

```
DLT645FrameFormatter
    ├─ uses → DLT645ValueFormatter (值格式化)
    ├─ uses → BCDHelper (地址解析)
    └─ uses → ProtocolParser (帧结构)

DLT645ValueFormatter
    └─ uses → BCDHelper (BCD 解析)

Logger
    ├─ uses → QMutex (线程安全)
    └─ uses → QFile (文件输出)
```

### 工具类特点

- **无状态设计**：所有方法都是静态或单例模式
- **线程安全**：Logger 使用互斥锁保护
- **高性能**：避免不必要的内存分配
- **可测试**：纯函数，易于单元测试

## 数据模型

### BCD 编码规则

**正向编码**（十进制 → BCD）：
```
"123456" → [0x12, 0x34, 0x56]
"12"     → [0x12]
"1"      → [0x01]
```

**逆向解码**（BCD → 十进制）：
```
[0x12, 0x34, 0x56] → "123456"
[0x12]             → "12"
[0x01]             → "01"
```

### 格式串解析流程

```
输入：formatString = "F9,AA,AA,.,AA"
      data = [0x12, 0x34, 0x56, 0x78]

步骤1：按逗号分割 token
    tokens = ["F9", "AA", "AA", ".", "AA"]

步骤2：遍历 token 处理字节
    F9  → 读取 data[0] = 0x12，判断正负（< 0x80 为正）
    AA  → 读取 data[1] = 0x34，直接输出 "34"
    AA  → 读取 data[2] = 0x56，直接输出 "56"
    .   → 输出小数点 "."
    AA  → 读取 data[3] = 0x78，直接输出 "78"

步骤3：拼接结果
    result = "+3456.78"
```

## 测试与质量

### 单元测试建议

**BCDHelper 测试**：
```cpp
void testToBCD() {
    QCOMPARE(BCDHelper::toBCD("123456"), QByteArray::fromHex("123456"));
    QCOMPARE(BCDHelper::toBCD("1"), QByteArray::fromHex("01"));
    QCOMPARE(BCDHelper::toBCD(""), QByteArray());
}

void testFromBCD() {
    QCOMPARE(BCDHelper::fromBCD(QByteArray::fromHex("123456")), "123456");
    QCOMPARE(BCDHelper::fromBCD(QByteArray::fromHex("01")), "01");
}
```

**ChecksumHelper 测试**：
```cpp
void testCalculate() {
    QByteArray data = QByteArray::fromHex("68 12 34 56 78 90 12 68 11 04 33 33 33 33");
    quint8 expected = /* 手动计算 */;
    QCOMPARE(ChecksumHelper::calculate(data), expected);
}
```

**DLT645ValueFormatter 测试**：
```cpp
void testFormatValue() {
    QByteArray data = QByteArray::fromHex("12 34 56 78");
    QString result = DLT645ValueFormatter::formatValue(data, "F9,AA,AA,.,AA");
    QCOMPARE(result, "+3456.78");
}
```

### 性能优化

- **避免频繁字符串拼接**：使用 `QString::reserve()` 预分配
- **减少内存分配**：复用 `QByteArray` 对象
- **缓存格式串解析结果**：避免重复解析

## 常见问题

### Q1: BCD 编码为什么采用低位在前？

A: DLT645-2007 协议规定地址域采用 Little Endian（低位在前），与 x86 架构一致。例如地址 "123456789012" 编码为 `12 34 56 78 90 12`。

### Q2: 格式串 "xx" 如何处理小数？

A: "xx" 表示将字节值除以 10。例如字节 `0x12` 解析为 "1.2"，字节 `0x34` 解析为 "3.4"。

### Q3: 如何实现日志过滤？

A: Logger 通过 `setLogLevel()` 设置最低级别，低于该级别的日志不会输出：

```cpp
logger->setLogLevel(LogLevel::Warning);  // 只输出 Warning/Error/Critical
LOG_DEBUG("This will be ignored");
LOG_WARNING("This will be logged");
```

### Q4: 如何添加自定义格式串 token？

A: 在 `DLT645ValueFormatter::parseFormatTokens()` 中添加新 token 解析逻辑：

```cpp
if (token == "MY_TOKEN") {
    // 自定义处理
}
```

## 相关文件清单

| 文件 | 说明 |
|------|------|
| `BCDHelper.h/cpp` | BCD 编解码工具 |
| `ChecksumHelper.h/cpp` | 校验和计算工具 |
| `DLT645DataHelper.h/cpp` | 数据域偏移工具 |
| `DLT645FrameFormatter.h/cpp` | 帧格式化输出 |
| `DLT645FrameFormatterFields.h` | 帧字段格式化 |
| `DLT645FrameFormatterValueDisplay.h` | 值显示格式化 |
| `DLT645BlockHandler.h/cpp` | 数据块处理 |
| `DLT645ValueFormatter.h/cpp` | 数值格式化工具 |
| `Logger.h/cpp` | 日志工具 |
| `LoggerTypes.h` | 日志类型定义 |
| `LOGGER_USAGE.md` | 日志使用说明 |

## 变更记录

### [2026-06-17] - 初始文档
- 完成工具层文档结构
- 详细说明各工具类接口与用法
- 添加格式串解析流程说明
- 添加日志系统使用指南

---

**维护者**：LIEVEZ  
**最后更新**：2026-06-17
