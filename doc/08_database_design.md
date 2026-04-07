# 数据库设计文档

## 1. 数据库选型

- 类型：SQLite
- 文件名：`meter_data.db`
- 存放位置：应用程序目录下
- 设计目标：零依赖、单机部署、轻量快速恢复

## 2. 当前数据库策略

系统采用“按表地址分表”的策略，而不是所有电表共用一张明细表。

### 2.1 设计原因

- 按电表地址查找更直接
- 结构上更贴近“每块表一个快照”
- 读取和覆盖保存逻辑简单
- 更适合当前单机仿真器场景

## 3. 表结构

### 3.1 元数据表

表名：`meter_store_meta`

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `meta_key` | TEXT PK | 元数据键 |
| `meta_value` | TEXT | 元数据值 |

当前用于记录：

- `schema_mode = per_meter_table`

### 3.2 电表数据表

表名规则：`meter_<address>`

示例：

- `meter_202008060056`

字段结构：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `di_code` | TEXT PK | 数据标识 |
| `value_text` | TEXT | 序列化后的值 |
| `value_type` | INTEGER | `QVariant::Type` |
| `updated_at` | TEXT | 更新时间 |

## 4. 数据保存规则

- 保存时按电表地址定位对应表。
- 若表不存在，则自动创建。
- 每次保存采用“整表快照覆盖”方式。
- 只保存可持久化的数据项：
  - 不保存数据块项
  - 不保存格式中带 `:` 的复合项
  - 不保存 `FF00` 结尾的数据块项

## 5. 数据加载规则

- 读取电表时，根据表地址计算表名。
- 表存在且有记录时，加载全部快照到 `MeterModel`。
- 表不存在或无记录时，使用默认初始化数据。

## 6. 迁移策略

- 启动时若发现旧共享表 `meter_data`，则直接删除。
- 当前不做旧表数据迁移，只保留新结构。

## 7. 与代码的映射关系

| 行为 | 代码入口 |
| --- | --- |
| 初始化数据库 | `MeterDataStore::initialize()` |
| 创建元数据表 | `MeterDataStore::createTables()` |
| 检查表是否存在 | `MeterDataStore::tableExists()` |
| 创建电表表 | `MeterDataStore::ensureMeterTable()` |
| 加载快照 | `MeterDataStore::loadMeterData()` |
| 保存快照 | `MeterDataStore::saveMeterData()` |

## 8. 风险与建议

- 当前为快照覆盖方式，单表数据量大时写放大较高。
- 当前没有独立索引扩展需求，因为主键已满足主要查询。
- 如果后续增加历史版本或审计，应新增历史表，而不是继续复用快照表。

