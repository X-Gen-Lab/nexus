# 当前平台交付

本轮已将默认实现统一为 **Core / Arch / SoC / Board / typed I/O / 可选 OS / 可选组件**。
旧 HAL、OSAL、factory、registry、运行时配置图和默认业务服务已移除。应用、产品任务、
私有 PCB、Flash 分区、恢复与安全策略由外部工程维护。

| 阅读入口 | 说明 |
|---|---|
| [当前架构](architecture.md) | 物理目录、依赖方向、执行与资源责任 |
| [支持范围](support.md) | 首发模式、三块 Board 与精确未知项 |
| [软件证据](verification.md) | 实际执行与尚待 clean-source 封存的区别 |
| [迁移手册](migration.md) | 旧 API、配置、对象与外部应用迁移 |
| [后续迭代](next-iterations.md) | 实板验收、测量驱动的扩展与团队流程 |
| [33 项实施台账](../design/next-generation-execution.csv) | 每项实现路径、证据入口、软件和物理退出状态 |

截至本轮开发验证，已有生产 `.c` 的 Native/寄存器故障模型、真实 ARM 编译与链接、
三板资源矩阵、外部应用开发构建、工具负路径与 HIL 准入工装。开发执行结果不自动
变成最终候选资格：clean HEAD、源码 SDK 独立消费、正式离线双构建与候选封存以
各自新生成的证据为准。硬件未接入，所有物理资格仍为 `not_executed`。

`docs/archive/` 保留历史设计、结果和旧 Sphinx 教程，不能继承为当前支持或验收。
当前公共 API 文档只从现有公开 include 树生成；代码格式和反斜杠 Doxygen 风格保持
根 `.clang-format`、`.editorconfig` 与现有规范。
