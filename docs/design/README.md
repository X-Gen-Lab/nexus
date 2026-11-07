---
id: design.readme
type: reference
audience: maintainer
source_language: zh_CN
---

# 当前架构与工程合同

当前生产 SoC 只维护 STM32F4，具体实现限定 F407VE／ZG。合同描述通用机制与精确
模式，不能从一份规范、厂商 SDK 或旧执行台账推导新源码已经通过。

| 任务 | 权威入口 |
|---|---|
| 阅读当前结构与支持 | [架构](../delivery/architecture.md)、[支持](../delivery/support.md) |
| 修改公共 I/O | [所有权](../explanation/ownership.md)、[I/O 机制](io-evolution.md) |
| 接入 F4 型号或外设 | [官方库边界](official-stm32-provider.md)、[接入合同](integration-contracts.md) |
| 维护 CPU 与 OS | [Arch](arch-contracts.md)、[OS](os-contracts.md) |
| 构建与 SDK | [构建系统](build-system.md)、[SDK 教程](../tutorials/source-sdk.md) |
| 代码与测试 | [工程手册](engineering-handbook.md)、[开发流程](../development/workflow.md) |
| 文档与图 | [内容标准](documentation-standards.md)、[页面模板](documentation-templates.md) |

旧专项 CSV 和执行记录保留原始身份，不能作为当前支持目录。原范围从
[固定重构前源码](https://github.com/X-Gen-Lab/nexus/tree/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18/docs/design)
追溯；不为退役平台继续维护一套当前设计页。
