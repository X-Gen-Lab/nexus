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
| [支持范围](support.md) | F407/F470 首发模式、G474/H743/U575 参考纵切片与精确未知项 |
| [派生能力矩阵](capabilities.md) | facts/routes、参考 Board、软件夹具和生产定义的一致性 |
| [软件证据](verification.md) | 实际执行与尚待 clean-source 封存的区别 |
| [SoC 基线执行记录](soc-refinement-verification.md) | 已封存基线的精确源码、执行结果与物理未执行边界 |
| [SoC 扩展执行记录](soc-extensions-verification.md) | G474 EXTI、H743 Cache/DMA、U575 双域参考及后续验收状态 |
| [SoC 扩展结果索引](soc-extensions-results.json) | 历史 SoC 固定源码、完整 CI、离线候选和归档摘要 |
| [构建系统契约](../design/build-system.md) | 单一配置权威、CMake 目标图、执行及工具边界 |
| [构建系统执行记录](build-system-verification.md) | `cc6a065c` 的完整软件 gates、精确候选及失败尝试 |
| [构建系统结果索引](build-system-results.json) | 当前源码与真实 CI、SDK、候选归档的字节身份 |
| [48 项构建实施台账](../design/build-system-execution.csv) | 原始退出条件、实现与精确软件证据 |
| [迁移手册](migration.md) | 旧 API、配置、对象与外部应用迁移 |
| [安全与生命周期审查](security-review.md) | 扫描范围、测试工装修复与借用／恢复限制 |
| [后续迭代](next-iterations.md) | 实板验收、测量驱动的扩展与团队流程 |
| [33 项实施台账](../design/next-generation-execution.csv) | 每项实现路径、证据入口、软件和物理退出状态 |
| [Factory 与 TDD 迭代台账](../design/factory-platform-execution.csv) | 本次 P0–P5 的具体实现与条件验收 |

CPU 软件层覆盖 M0/M0+/M3/M4/M7/M23/M33/M55/M85。外部工程可通过一份
严格 CPU runtime TOML/事实包使用 `nexus_add_runtime` 或安装包的 Runtime
component，复用 Core/Arch/I/O face 与选定裸机/FreeRTOS adapter。运行时装配
不生成 SoC、Board、启动或 factory，具体硬件、产品任务和安全策略仍由外部工程
负责。F407/F470 三板维护首发模式，G474/H743/U575 增加精确 GPIO／IRQ UART／
timebase 参考装配。G474 另有 selected EXTI，H743 另有固定 Cache 配置与有限
TX DMA，U575 另有独立无 OS 的同步双域参考；这些纵切片不代表全外设或完整
安全域接入，物理 HIL 全部未执行。

历史 SoC 扩展基线固定源码 `98270512` 已完成全部 Native 契约、工具和配置测试、121 个 ARM
镜像、104 个 CPU runtime 变体、SDK 独立消费及完整 GitHub CI。八个 H743/U575
公开参考配置分别完成两次独立断网构建，ELF/BIN 逐字节一致，并封存为无签名软件
候选；具体范围和原始证据见结果索引。后续文档提交只记录该源码，不继承为新的
代码执行资格。实板未接入，物理 HIL 均为 `not_executed`；运行态交接、DVFS 和
深 STOP 连续计时尚未实现，55 项原始退出条件保持不变。

本次构建系统交付绑定 `cc6a065c`，完整结果单独记录在构建结果索引，不能继承
上面的历史 SoC 执行身份。八个当前候选使用 H743 HSI64／Cache off 和 U575
MSIS4／TZEN=0 的 empty／UART、裸机／FreeRTOS 参考组合；这不扩大实板支持。
72 阶段软件总计划未作为一次完整 plan invocation 执行，OCI recipe 重建、
CP311 离线安装和物理 HIL 仍未执行。后续文档提交只记录其精确代码源码。

`docs/archive/` 保留历史设计、结果和旧 Sphinx 教程，不能继承为当前支持或验收。
当前公共 API 文档只从现有公开 include 树生成；代码格式和反斜杠 Doxygen 风格保持
根 `.clang-format`、`.editorconfig` 与现有规范。

行为变更遵循 TDD。GoogleTest／GoogleMock 只进入 host 测试目标，生产固件保持
C11。commit hooks 实际执行完整 testing／configure／ci／workflows 四域工具测试
和全部 GoogleTest 契约，拒绝未完成、过滤或跳过的报告。执行结果证明当前合同；真实 RED→GREEN 过程由 PR 和原始日志记录。

- [SoC refinement and physical acceptance](../design/soc-refinement.md)
