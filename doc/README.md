# DLT645_Sim 项目文档集

## 1. 说明

本目录用于集中存放 `DLT645_Sim` 项目的阶段性文档。文档内容基于以下信息整理：

- 根目录已有说明文档：`readme.md`、`ARCHITECTURE.md`、`CURRENT_IMPLEMENTATION.md`、`dataItem.md`
- 当前 Qt5/qmake 工程源码
- 截至 `2026-04-07` 的仓库现状

本文档集分为两类：

- 现状文档：尽量严格按当前代码实现编写
- 管理文档：给出可直接落地的项目交付草案，需项目经理、客户或运维继续补充签字信息

## 2. 文档列表

| 文件 | 对应阶段 | 说明 |
| --- | --- | --- |
| `01_project_charter.md` | 项目启动 | 项目章程 |
| `02_feasibility_study.md` | 项目启动 | 可行性分析报告 |
| `03_requirements_research.md` | 项目启动 | 需求调研记录 |
| `04_srs.md` | 需求分析 | 需求规格说明书 |
| `05_user_stories_and_use_cases.md` | 需求分析 | 用户故事与用例 |
| `06_business_process.md` | 需求分析 | 业务流程说明 |
| `07_system_architecture.md` | 设计 | 系统架构设计文档 |
| `08_database_design.md` | 设计 | 数据库设计文档 |
| `09_interface_design.md` | 设计 | 接口设计文档 |
| `10_ui_ux_design.md` | 设计 | UI/UX 设计说明 |
| `11_security_design.md` | 设计 | 安全设计文档 |
| `12_coding_standards.md` | 开发 | 代码规范文档 |
| `13_technical_implementation.md` | 开发 | 技术实现文档 |
| `14_unit_test_report.md` | 开发 | 单元测试报告 |
| `15_test_plan.md` | 测试 | 测试计划 |
| `16_test_cases.md` | 测试 | 测试用例文档 |
| `17_defect_report.md` | 测试 | 缺陷报告 |
| `18_integration_test_report.md` | 测试 | 集成测试报告 |
| `19_deployment_manual.md` | 部署上线 | 部署手册 |
| `20_operations_manual.md` | 部署上线 | 运维手册 |
| `21_user_manual.md` | 部署上线 | 用户操作手册 |
| `22_system_configuration.md` | 部署上线 | 系统配置文档 |
| `23_project_summary.md` | 项目收尾 | 项目总结报告 |
| `24_acceptance_report.md` | 项目收尾 | 验收报告 |
| `25_handover_document.md` | 项目收尾 | 移交文档 |

## 3. 当前项目基线

- 项目类型：Qt5/C++ 桌面版 DLT645-2007 电表模拟器
- 构建方式：`qmake`
- 主要模块：主窗口、设备树、数据表格、操作日志、串口控制器、TCP 控制器、协议控制器、协议解析、数据模型、SQLite 持久化
- 数据持久化：SQLite，本地库文件 `meter_data.db`
- 当前数据库策略：一块表地址对应一张表，表名形如 `meter_<address>`

## 4. 使用建议

- 如果用于项目立项或验收，请优先补充真实客户、预算、时间节点和签字信息。
- 如果用于开发接手，请优先阅读：
  - `07_system_architecture.md`
  - `08_database_design.md`
  - `13_technical_implementation.md`
  - `19_deployment_manual.md`

