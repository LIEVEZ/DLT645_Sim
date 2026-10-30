# 贡献指南

感谢你愿意参与 DLT645_Sim！本文说明如何搭建开发环境、提交 Issue 和 Pull Request。

## 报告问题

提交 Issue 时请尽量包含：

- 操作系统与版本、Qt 版本、编译器版本
- 复现步骤、期望结果、实际结果
- 相关报文（HEX）或截图
- 日志文件内容：
  - Windows：`%APPDATA%\dlt645_sim.log`
  - Linux：`~/.local/share/dlt645_sim.log`

如果是协议实现问题，请附上标准条款或可对照的真实设备报文，便于核对。

## 开发环境

- Qt 5.14.2+（推荐 5.15.x），需要 `widgets`、`serialport`、`network`、`sql`
- C++17 编译器
- qmake + make / mingw32-make / nmake

构建步骤见 [README 快速开始](README.md#快速开始)。

**注意**：qmake 不会自动发现新文件。新增 `.cpp` / `.h` 后，必须手动加入 [`Dlt645_Sim/Dlt645_Sim.pro`](Dlt645_Sim/Dlt645_Sim.pro) 的 `SOURCES` / `HEADERS`。

## 编码约定

- C++17；遵循 Qt 编码风格
- 类名 `UpperCamelCase`，函数/变量 `lowerCamelCase`，成员 `m_` 前缀，常量 `k` 前缀或全大写
- 每行不超过 120 字符
- 头文件使用 `#ifndef` / `#define` / `#endif` 守卫
- 公开接口使用 Doxygen `@brief` 注释；复杂逻辑优先写“为什么”
- 使用 Qt 容器类（`QList`、`QMap`、`QString`）

## 协议相关注意事项

修改协议层之前请先确认以下规则：

1. **数据域偏移**：发送时数据域整体 +0x33，接收时 -0x33。已有 `DLT645DataHelper::subtract33` 等工具，不要重复加减。
2. **地址/DI 字节序**：地址和 DI 均为低位字节在前；DI 存储与传输都要注意反向。
3. **帧结构**：`68 + 地址(6) + 68 + C + L + DATA + CS + 16`，可选 4 个前导 `FE`；校验和为 `68` 到数据域结束的模 256 累加和。
4. **格式串**：使用逗号分隔的 token（常用 `AA`、`xx`、`F9`、`PF`、`.`）。引擎按逗号分组计算字节数，每个分组对应 1 字节（小数点除外），不要用字母个数表达位数。
5. **单帧长度**：长度域只有 1 字节，单帧数据域不超过 255 字节；多帧续读（0x12）尚未实现。

## 测试

修改代码后请至少：

1. 完整构建主程序；
2. 构建并运行 `Dlt645_Sim/tests/` 下的相关测试；
3. 如果改动涉及协议应答，建议用 `ProtocolControllerTest` 或手工报文验证。

CI 会对每次 Push / PR 执行 Linux 构建和测试，见 [`.github/workflows/build.yml`](.github/workflows/build.yml)。

## 提交 Pull Request

1. Fork 仓库并从 `main` 创建分支，例如 `feature/support-freeze-command`
2. 保持改动聚焦，避免顺带重构无关代码
3. 提交信息建议使用 Conventional Commits，例如：
   - `feat(protocol): support freeze command 0x16`
   - `fix(parser): reject frames with invalid end flag`
   - `docs(readme): clarify TCP client mode`
4. 确认构建和测试通过，必要时同步更新 README / doc
5. 在 PR 描述中说明改动目的、验证方式和兼容性影响

## 不接收的内容

- 用于绕过真实电表认证、攻击生产设备或破坏电网的功能
- 提交构建产物、日志、配置文件或本机路径
- 大规模无关格式化改动

## 许可

提交的代码默认以本项目的 MIT License 发布。参与贡献即表示你同意以该许可分发你的修改。
