---
id: delivery.readme
type: reference
audience: consumer
source_language: zh_CN
---

# 当前平台交付边界

当前重构将生产 SoC 收敛为 STM32F4，现有 F407VE／ZG 及四个 Board 包迁入系列入口。
全部 vendor 来源保留；Native 移到显式测试支撑。应用任务、产品策略和私有 PCB
继续由外部工程维护。

本页说明当前阅读与验收入口，不把源码存在、文档构建或历史软件结果写成本次通过。
当前主线是剩余冷态行为收敛和精确 F4 型号扩展，按实际回归与源码身份验收。当前物理 HIL
为 `not_executed`，本次重构没有自动建立软件候选。

[Host C11 配置器](../../tools/nxconfig/README.md)是在 PC 上运行的可选试点，默认平台
配置仍使用 Python。试点不进入固件，其全面替换不作为平台使用、固件 C 化或软件
交付的前置条件。它改变的是主机工具实现，不直接改变 MCU 热路径或资源占用。

| 入口 | 用途 |
|---|---|
| [当前架构](architecture.md) | 目录、依赖、运行路径和状态责任 |
| [精确支持](support.md) | 型号、板卡、模式与限制 |
| [能力矩阵](capabilities.md) | 事实、路线、装配与实际生产定义关联 |
| [源码定位](../explanation/source-map.md) | 各层权威入口与源码归属 |
| [资格边界](../reference/platforms/qualification.md) | 模型、ARM、SDK、候选和物理验收分开 |
| [软件证据导航](https://github.com/X-Gen-Lab/nexus/blob/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18/docs/status/software-verification.md) | 按原源码与配置读取执行记录 |

当前源码版本号保持 0.2.1；已发布版本仍以
[GitHub Releases](https://github.com/X-Gen-Lab/nexus/releases)和其固定 manifest 为准。
重构不移动旧标签，不重新标记旧产物。

退出平台的原始实现和记录可从
[重构前固定源码](https://github.com/X-Gen-Lab/nexus/tree/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18)
追溯。原日志、来源和状态保持原身份，恢复或阅读它们不授予新源码资格。
