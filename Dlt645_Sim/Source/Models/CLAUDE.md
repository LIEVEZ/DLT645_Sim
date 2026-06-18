[根目录](../../../CLAUDE.md) > [Source](../) > **Models**

# Models - 领域模型层

## 模块职责

领域模型层是业务逻辑的核心，封装电表的所有状态数据和业务规则。该层对外提供线程安全的数据访问接口，通过信号机制通知 UI 层数据变更，实现模型与视图的解耦。

**核心职责**：
- 管理电表的全部数据项（七大类）
- 维护电表配置（地址、密码、操作者代码）
- 提供数据查询、修改、重置、随机化接口
- 实现数据分类索引与快速检索
- 保证多线程访问安全
- 发出数据变更通知信号

## 入口与启动

模型实例由 `MainWindow` 根据用户操作动态创建：

```cpp
// 用户通过 NewMeterDialog 创建电表时
MeterModel *meter = new MeterModel(this);
meter->setMeterAddress("123456789012");
meter->setPassword("00000000");
meter->setOperatorCode("00000000");
meter->initializeDefaultDataItems();  // 初始化七类数据项

// 存储到映射表
m_meterModelsByAddress[address] = meter;
```

## 对外接口

### MeterModel（核心模型）

**配置接口**：
- `setMeterAddress(QString)` - 设置 12 位 BCD 地址
- `getMeterAddress()` - 获取电表地址
- `setPassword(QString)` - 设置 8 位密码
- `getPassword()` - 获取密码
- `setOperatorCode(QString)` - 设置 8 位操作者代码
- `getOperatorCode()` - 获取操作者代码
- `setIncludePrefixFE(bool)` - 设置是否包含前导 FE
- `getIncludePrefixFE()` - 获取前导 FE 设置
- `setMeterPhaseType(MeterPhaseType)` - 设置电表相型（单相/三相）
- `getMeterPhaseType()` - 获取相型
- `setEventRecordPresent(bool)` - 设置事件记录是否存在

**数据访问接口**：
- `getDataItem(QString diCode)` - 根据 DI 码获取数据项
- `getDataValue(QString diCode)` - 获取数据值
- `setDataValue(QString diCode, QVariant value)` - 设置数据值
- `getDataItemsByCategory(DataCategory)` - 按分类获取数据项列表
- `getAllDataItems()` - 获取全部数据项

**批量操作接口**：
- `resetAllData()` - 重置所有数据为默认值
- `randomizeData(DataCategory)` - 随机化指定分类数据
- `randomizeData(QStringList diCodes)` - 随机化指定数据项
- `exportData(QString filePath)` - 导出数据到文件
- `importData(QString filePath)` - 从文件导入数据

**初始化接口**：
- `initializeDefaultDataItems()` - 初始化所有七类数据项

**信号**：
- `dataChanged(QString diCode, QVariant value)` - 数据变更通知
- `meterInfoChanged()` - 电表信息变更通知

### DataItem（数据项对象）

**属性**：
- `diCode` - 数据标识码（如 "00000000"）
- `name` - 中文名称
- `value` - 当前值
- `unit` - 单位
- `format` - 数据格式串（如 "F9,AA,AA,.,AA"）
- `category` - 所属分类
- `description` - 详细描述
- `readonly` - 是否只读

**方法**：
- `setValue(QVariant)` - 设置值
- `getValue()` - 获取值
- `isValid()` - 校验值是否合法

### DataCategory（数据分类枚举）

```cpp
enum class DataCategory {
    Energy,      // 电能量（00 00 00 00 - 00 00 FF FF）
    Demand,      // 最大需量及发生时间（01 01 00 00 - 01 01 FF FF）
    Variable,    // 变量（02 01 00 00 - 02 80 FF FF）
    Event,       // 事件记录（03 00 00 00 - 03 FF FF FF）
    Parameter,   // 参变量（04 00 00 00 - 04 FF FF FF）
    Freeze,      // 冻结（05 00 00 00 - 05 FF FF FF）
    LoadRecord   // 负荷记录（06 00 00 00 - 06 FF FF FF）
};
```

### MeterPhaseType（电表相型）

```cpp
enum class MeterPhaseType {
    SinglePhase,   // 单相表
    ThreePhase     // 三相表
};
```

## 关键依赖与配置

### 依赖关系

```
MeterModel
    ├─ includes → DataItem
    ├─ includes → MeterPhaseType
    ├─ uses → QMutex (线程安全)
    └─ uses → QMap<QString, DataItem> (数据容器)

CategoryCatalog
    └─ provides → 数据项定义表
```

### 数据初始化流程

```cpp
// MeterModel::initializeDefaultDataItems() 调用链
initializeDefaultDataItems()
    ├─ initializeEnergyData()           // 电能量
    ├─ initializeDemandData()           // 最大需量
    ├─ initializeVariableData()         // 变量
    ├─ initializeEventData()            // 事件记录
    ├─ initializeParameterData()        // 参变量
    ├─ initializeFreezeData()           // 冻结
    └─ initializeLoadRecordData()       // 负荷记录
```

## 数据模型

### 数据项结构示例

**电能量数据项**（DI: 00 00 00 00）：
```cpp
DataItem item;
item.diCode = "00000000";
item.name = "(当前)组合有功总电能";
item.value = "+000000.00";
item.unit = "kWh";
item.format = "F9,AA,AA,.,AA";  // F9=正负号, AA=字节, .=小数点
item.category = DataCategory::Energy;
item.readonly = false;
```

**变量数据项**（DI: 02 01 01 00）：
```cpp
DataItem item;
item.diCode = "02010100";
item.name = "A相电压";
item.value = "220.0";
item.unit = "V";
item.format = "xx,xx,.,xx";  // xx=除以10
item.category = DataCategory::Variable;
item.readonly = true;  // 实时变量通常只读
```

### 格式串规则（format 字段）

| Token | 含义 | 示例 |
|-------|------|------|
| `AA` | 直接转换为字符显示 | `AA,AA` → 1234 |
| `AS` | 转为 ASCII 字符 | `AS,AS,AS` → ABC |
| `xx` | 除以 10 后显示（加小数点） | `xx,.,xx` → 12.34 |
| `F9` | 处理正负号（≥80 为负） | `F9,AA,AA` → -1234 |
| `bin` | 后续是表名，按位查表 | `bin,tableName` |
| `bit` | 转二进制后按位查表 | `bit,tableName` |
| `.` | 小数点分隔符 | `xx,.,xx` → 12.34 |

### 分类索引结构

```cpp
// 内部数据组织
QMap<QString, DataItem> m_dataItems;  // 主索引：DI 码 -> DataItem
QMutex m_dataItemsMutex;              // 保护并发访问

// 分类索引由 getDataItemsByCategory() 动态生成
QList<DataItem> getDataItemsByCategory(DataCategory cat) {
    QList<DataItem> result;
    for (auto &item : m_dataItems) {
        if (item.category == cat)
            result.append(item);
    }
    return result;
}
```

## 测试与质量

### 线程安全保证

```cpp
// 所有数据访问方法使用互斥锁
QVariant MeterModel::getDataValue(const QString &diCode) {
    QMutexLocker locker(&m_dataItemsMutex);
    if (m_dataItems.contains(diCode))
        return m_dataItems[diCode].value;
    return QVariant();
}
```

### 数据校验

- 地址长度：必须 12 位数字
- 密码/操作者代码：必须 8 位数字
- DI 码格式：必须 8 位十六进制
- 数值范围：根据数据项类型校验（如电压 0-500V）

### 性能考虑

- 使用 `QMap` 实现 O(log n) 查找
- 避免频繁信号发射（批量操作时合并通知）
- 惰性初始化：仅初始化当前需要的数据项

## 常见问题

### Q1: 如何添加新数据项？

A: 在对应的初始化方法中添加（如 `initializeEnergyData()`），按格式填充 DI 码、名称、格式串等：

```cpp
DataItem item;
item.diCode = "00010000";
item.name = "新数据项";
item.value = "0.00";
item.unit = "kWh";
item.format = "xx,xx,.,xx";
item.category = DataCategory::Energy;
m_dataItems[item.diCode] = item;
```

### Q2: 如何实现数据自动累加（模拟电能增长）？

A: 通过 `DataGenerator` 服务定时调用 `setDataValue()` 更新电能数据：

```cpp
// DataGenerator::updateEnergyData()
double currentEnergy = model->getDataValue("00000000").toDouble();
currentEnergy += 0.01;  // 每秒累加 0.01 kWh
model->setDataValue("00000000", currentEnergy);
```

### Q3: 格式串如何解析？

A: 由 `DLT645ValueFormatter` 工具类负责解析，根据逗号分隔的 token 序列处理字节数组：

```cpp
// 格式串 "F9,AA,AA,.,AA" 表示：
// [F9] 第1字节：正负号判断
// [AA] 第2字节：直接显示
// [AA] 第3字节：直接显示
// [.] 插入小数点
// [AA] 第4字节：直接显示
```

## 相关文件清单

| 文件 | 说明 |
|------|------|
| `MeterModel.h/cpp` | 电表核心模型 |
| `MeterModelPrivate.h` | 私有实现头文件 |
| `MeterModelInitialization*.cpp` | 数据项初始化实现（按分类拆分） |
| `MeterModelQueries.cpp` | 查询接口实现 |
| `MeterModelAccess.cpp` | 访问接口实现 |
| `DataItem.h/cpp` | 数据项对象 |
| `MeterInfo.h` | 电表信息结构 |
| `ConnectionConfig.h` | 连接配置结构 |
| `ConnectionInfo.h` | 连接信息结构 |
| `CategoryCatalog.h/cpp` | 数据分类目录 |
| `MeterPhaseType.h` | 相型枚举 |

## 变更记录

### [2026-06-17] - 初始文档
- 完成模型层文档结构
- 详细说明 MeterModel 接口与数据组织
- 添加格式串解析规则说明
- 添加线程安全与性能说明

---

**维护者**：LIEVEZ  
**最后更新**：2026-06-17
