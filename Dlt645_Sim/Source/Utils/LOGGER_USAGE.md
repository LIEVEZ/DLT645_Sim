# 日志系统使用说明

## 概述

DLT645_Sim 日志系统提供了完整的分级、轮转和双输出（控制台 + 文件）功能。

---

## 功能特性

### ✅ 日志分级
- **DEBUG** — 调试信息（开发环境）
- **INFO** — 一般信息（生产环境默认）
- **WARNING** — 警告信息
- **ERROR** — 错误信息
- **CRITICAL** — 严重错误

### ✅ 日志轮转
- 自动按文件大小轮转
- 保留指定数量的历史备份
- 轮转时添加时间戳标记

### ✅ 双输出
- **控制台输出** — 实时查看（可关闭）
- **文件输出** — 持久化存储（可关闭）

### ✅ 线程安全
- 使用 `QMutex` 保护，支持多线程日志记录

---

## 基本用法

### 1. 初始化配置（main.cpp）

```cpp
#include "Utils/Logger.h"
#include <QStandardPaths>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 配置日志系统
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(logDir);
    QString logFilePath = logDir + "/dlt645_sim.log";

    Logger *logger = Logger::instance();
    logger->setLogFile(logFilePath);
    logger->setLogRotation(10 * 1024 * 1024, 5); // 10MB per file, keep 5 backups
    logger->setLogLevel(LogLevel::Info);          // 生产环境使用 Info
    logger->setConsoleOutputEnabled(true);
    logger->setFileOutputEnabled(true);

    LOG_INFO(QString("Application started, log file: %1").arg(logFilePath));

    MainWindow w;
    w.show();
    return a.exec();
}
```

### 2. 记录日志

使用便捷宏：

```cpp
#include "Utils/Logger.h"

// 调试信息
LOG_DEBUG("Detailed debug information");

// 一般信息
LOG_INFO("User clicked export button");

// 警告
LOG_WARNING("Database connection slow");

// 错误
LOG_ERROR("Failed to open serial port");

// 严重错误
LOG_CRITICAL("Application crashed");
```

使用实例方法：

```cpp
Logger::instance()->info("Connection established");
Logger::instance()->error(QString("Invalid address: %1").arg(address));
```

---

## 配置参数

### setLogFile(filePath)
设置日志文件路径。

```cpp
logger->setLogFile("/var/log/dlt645_sim.log");
```

**默认位置**（Windows）:
```
C:\Users\<用户>\AppData\Local\DLT645_Sim\dlt645_sim.log
```

### setLogRotation(maxFileSize, maxBackupCount)
配置日志轮转。

```cpp
// 单个文件最大 10MB，保留 5 个备份
logger->setLogRotation(10 * 1024 * 1024, 5);
```

**参数**:
- `maxFileSize` — 单个日志文件最大大小（字节），0 表示不限制
- `maxBackupCount` — 保留的历史文件数量

**轮转示例**:
```
dlt645_sim.log      ← 当前日志（达到 10MB 后轮转）
dlt645_sim.log.1    ← 上一个日志
dlt645_sim.log.2    ← 更早的日志
dlt645_sim.log.3
dlt645_sim.log.4
dlt645_sim.log.5    ← 最老的日志（下次轮转时删除）
```

### setLogLevel(level)
设置最低日志级别。

```cpp
// 生产环境：只记录 Info 及以上
logger->setLogLevel(LogLevel::Info);

// 开发环境：记录所有日志
logger->setLogLevel(LogLevel::Debug);

// 仅记录错误
logger->setLogLevel(LogLevel::Error);
```

### setConsoleOutputEnabled(enabled)
启用/禁用控制台输出。

```cpp
logger->setConsoleOutputEnabled(true);  // 开发环境
logger->setConsoleOutputEnabled(false); // 生产环境（仅文件）
```

### setFileOutputEnabled(enabled)
启用/禁用文件输出。

```cpp
logger->setFileOutputEnabled(true);  // 持久化日志
logger->setFileOutputEnabled(false); // 仅控制台（测试时）
```

---

## 日志格式

```
[2026-06-17 14:23:45.123] [INFO] Application started
[2026-06-17 14:23:46.456] [DEBUG] Loading configuration
[2026-06-17 14:23:47.789] [WARNING] Serial port not found
[2026-06-17 14:23:48.012] [ERROR] Failed to connect
[2026-06-17 14:23:49.345] [CRITICAL] Out of memory
```

**格式**: `[时间戳] [级别] 消息内容`

---

## 使用场景

### 场景 1: 开发环境
```cpp
logger->setLogLevel(LogLevel::Debug);        // 记录所有日志
logger->setConsoleOutputEnabled(true);       // 实时查看
logger->setFileOutputEnabled(true);          // 同时保存
logger->setLogRotation(5 * 1024 * 1024, 3);  // 5MB, 3 个备份
```

### 场景 2: 生产环境
```cpp
logger->setLogLevel(LogLevel::Info);         // 过滤 Debug
logger->setConsoleOutputEnabled(false);      // 不输出控制台
logger->setFileOutputEnabled(true);          // 仅保存文件
logger->setLogRotation(50 * 1024 * 1024, 10); // 50MB, 10 个备份
```

### 场景 3: 快速测试
```cpp
logger->setLogLevel(LogLevel::Debug);
logger->setConsoleOutputEnabled(true);       // 仅控制台
logger->setFileOutputEnabled(false);         // 不保存文件
```

### 场景 4: 调试特定模块
```cpp
// 临时降低日志级别
logger->setLogLevel(LogLevel::Debug);
LOG_DEBUG("Entering ProtocolParser::parseFrame");
// ... 调试代码
LOG_DEBUG("Exiting ProtocolParser::parseFrame");
// 恢复原级别
logger->setLogLevel(LogLevel::Info);
```

---

## 性能考虑

### 日志级别过滤
日志级别过滤在 `writeLog` 开始时进行，避免不必要的字符串格式化：

```cpp
// ❌ 浪费：即使 Debug 被过滤，字符串仍会拼接
LOG_DEBUG(QString("Value: %1, Size: %2").arg(value).arg(size));

// ✅ 推荐：先检查级别
if (Logger::instance()->getLogLevel() <= LogLevel::Debug) {
    LOG_DEBUG(QString("Value: %1, Size: %2").arg(value).arg(size));
}
```

### 文件 I/O
- 每次日志写入都会 `flush()`，保证数据不丢失
- 轮转时会短暂阻塞（关闭/重命名/重开）

### 线程安全
- 所有日志方法内部使用 `QMutexLocker` 保护
- 多线程环境下自动同步，无需额外处理

---

## 故障排查

### 日志文件未生成
1. 检查目录权限
2. 检查 `setFileOutputEnabled(true)` 是否调用
3. 检查日志级别是否过滤了所有消息

### 日志文件过大
1. 检查 `setLogRotation` 是否配置
2. 降低日志级别（Debug → Info）
3. 减少日志输出频率

### 性能问题
1. 关闭控制台输出（生产环境）
2. 提高日志级别（过滤 Debug）
3. 增大轮转大小，减少轮转频率

---

## 信号接口

日志系统提供 Qt 信号，可连接到 UI 组件：

```cpp
connect(Logger::instance(), &Logger::logEntryAdded,
        this, &OperationLogWidget::onLogEntry);

void OperationLogWidget::onLogEntry(LogLevel level, 
                                    const QDateTime &timestamp, 
                                    const QString &message)
{
    // 更新 UI 显示
    QString formattedLog = QString("[%1] [%2] %3")
        .arg(timestamp.toString("HH:mm:ss"))
        .arg(levelToString(level))
        .arg(message);
    m_logTextEdit->append(formattedLog);
}
```

---

## 最佳实践

### ✅ DO
- 使用合适的日志级别
- 生产环境关闭 Debug 日志
- 定期检查日志文件大小
- 在关键操作前后记录日志
- 使用结构化消息（包含上下文）

### ❌ DON'T
- 在循环中大量记录 Debug 日志
- 记录敏感信息（密码、密钥）
- 在性能敏感代码中使用 Debug
- 忘记配置日志轮转（导致磁盘满）

---

**文档版本**: 1.0  
**最后更新**: 2026-06-17
