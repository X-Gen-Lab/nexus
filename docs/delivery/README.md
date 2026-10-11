# 当前平台交付

默认实现统一为 **Core / Arch / SoC / Board / 静态 factory / 多态 I/O / 可选 OS /
可选组件**。一份 TOML 经严格校验和不可变 IR 生成精确实例；factory 返回稳定的
只读接口对象，共享只读操作表，独立状态留在 provider 内部。获取接口不启动硬件。
应用、产品任务、私有 PCB、Flash 分区、恢复与安全策略由外部工程维护。

| 阅读入口 | 说明 |
|---|---|
| [当前架构](architecture.md) | 物理目录、依赖方向、执行与资源责任 |
| [Cortex-M 软件支持](cortex-m.md) | 九核 ABI/feature、Core/Arch/OS runtime 与配置示例 |
| [OS 交付](os.md) | 配置、显式存储、外部多任务示例与 OS HIL 范围 |
| [Arch 契约](../design/arch-contracts.md) | 原子操作、掩码、cache/MPU/SAU 的上下文和所有权 |
| [支持范围](support.md) | 首发模式、三块 Board 与精确未知项 |
| [派生能力矩阵](capabilities.md) | facts/routes、参考 Board、软件夹具和生产定义的一致性 |
| [软件证据](verification.md) | 实际执行与尚待 clean-source 封存的区别 |
| [迁移手册](migration.md) | 旧 API、配置、对象与外部应用迁移 |
| [安全与生命周期审查](security-review.md) | 扫描范围、测试工装修复与借用／恢复限制 |
| [后续迭代](next-iterations.md) | 实板验收、测量驱动的扩展与团队流程 |
| [33 项实施台账](../design/next-generation-execution.csv) | 每项实现路径、证据入口、软件和物理退出状态 |
| [Factory 与 TDD 迭代台账](../design/factory-platform-execution.csv) | 本次 P0–P5 的具体实现与条件验收 |

CPU 软件层覆盖 M0/M0+/M3/M4/M7/M23/M33/M55/M85。外部工程可通过一份
严格 CPU runtime TOML/事实包使用 `nexus_add_runtime` 或安装包的 Runtime
component，复用 Core/Arch/I/O face 与选定裸机/FreeRTOS adapter。运行时装配
不生成 SoC、Board、启动或 factory，具体硬件、产品任务和安全策略仍由外部工程
负责；当前完整平台保持 F407/F470 三板，物理 HIL 全部未执行。

截至本轮开发验证，已有生产 `.c` 的 Native/寄存器故障模型、真实 ARM 编译与链接、
三板资源矩阵、外部应用开发构建、工具负路径与 HIL 准入工装。开发执行结果不自动
变成最终候选资格：clean HEAD、源码 SDK 独立消费、正式离线双构建与候选封存以
各自新生成的证据为准。硬件未接入，所有物理资格仍为 `not_executed`。

`docs/archive/` 保留历史设计、结果和旧 Sphinx 教程，不能继承为当前支持或验收。
当前公共 API 文档只从现有公开 include 树生成；代码格式和反斜杠 Doxygen 风格保持
根 `.clang-format`、`.editorconfig` 与现有规范。

行为变更遵循 TDD。GoogleTest／GoogleMock 只进入 host 测试目标，生产固件保持
C11。commit hooks 实际执行配置工具和全部 GoogleTest 契约，拒绝未完成、过滤或
跳过的报告。执行结果证明当前合同；真实 RED→GREEN 过程由 PR 和原始日志记录。
