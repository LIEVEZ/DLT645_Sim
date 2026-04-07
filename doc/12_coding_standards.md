# 代码规范文档

## 1. 目标

统一 Qt/C++ 项目编码风格，降低维护成本，提高模块协作效率。

## 2. 语言与框架

- 语言标准：C++17
- UI 框架：Qt5 Widgets
- 构建方式：qmake
- 数据库：Qt SQL + SQLite

## 3. 命名规范

| 对象 | 规范 |
| --- | --- |
| 类名 | `UpperCamelCase`，如 `MeterDataStore` |
| 函数名 | `lowerCamelCase`，如 `saveMeterData` |
| 成员变量 | `m_` 前缀，如 `m_meterDataStore` |
| 常量 | 可使用 `k` 前缀或 `constexpr` |
| 信号/槽 | 使用动词或事件语义，如 `dataChanged` |

## 4. 目录规范

- `Source/Views`：界面组件
- `Source/ViewModels`：界面状态组织
- `Source/Controllers`：连接和协议协调
- `Source/Services`：服务和基础能力
- `Source/Models`：业务模型
- `Source/Utils`：通用工具

## 5. Qt 编码规范

- `QObject` 派生类需要信号槽时必须保留 `Q_OBJECT`
- 优先使用父子对象管理生命周期
- UI 更新应通过信号槽或明确调用，不应跨线程直接操作控件
- 公共信号语义应清晰，不混用“状态”和“命令”

## 6. 数据与持久化规范

- 所有电表数据修改统一通过 `MeterModel`
- 不允许 UI 绕过模型直接改数据库
- 新增持久化字段时必须同步更新序列化/反序列化逻辑
- 新增聚合型 DI 时必须补充总值同步逻辑

## 7. 日志规范

- 正常业务结果使用 `LOG_INFO`
- 异常与失败使用 `LOG_ERROR` 或 `LOG_WARNING`
- 日志内容应包含对象标识，例如连接别名、表地址、DI 号

## 8. 注释规范

- 优先写“为什么”，少写“这行代码做了什么”
- 复杂数据同步、协议映射、数据库策略必须留注释
- 不保留失效的大段注释代码

## 9. 提交前检查

1. 能完成构建
2. 关键流程手动验证
3. 不引入未使用代码
4. 不保留明显死代码
5. 文档同步更新

