---
id: guides.os-backend-porting
type: how-to
audience: maintainer
source_language: zh_CN
---

# 维护或接入 OS 后端

已有后端先从 TOML 选择，不为每块板复制 OSAL。新后端必须实现明确的任务、同步、
时间和生命周期合同；不能仅增加 backend 名字就宣称支持。

## 确定所需子集

先列调用方实际需要的静态任务、队列、通知、等待和时间能力。未实现能力明确拒绝；
ThreadX 的 Native／M4 子集不继承 FreeRTOS 的 tickless、隔离或端口范围。
CPU 原语、SoC 时钟与 OS 内核职责分别维护。

## 以一条实际路径接入

1. 锁定内核、端口、许可证与编译上下文，在 `os/<backend>/` 实现窄 adapter。
2. 明确调用者提供的 stack、TCB、队列和控制状态，固定返回、等待、关闭与 join。
3. 实现同一绝对单调 deadline 和最终谓词观察，拒绝非法 ISR 调用和优先级。
4. 验证真实任务执行、部分创建失败、通知竞争、超时、关闭和失败排空。
5. 编译所维护的 CPU／ABI 组合，再单独记录 MCU 上下文、栈峰值和时序实测。

应用拥有 scheduler 启动和产品 worker。Platform start 不进入内核；任务入口返回
或通知到达不授权回收 stack／TCB。内核仍使用的 Tick、时钟和 IRQ 必须保留。

## 与配置和构建组合

后端的能力与策略字段进入公开合同和 typed IR，实际 CMake 目标发布为所选
`Nexus::OS`。新增后端同时维护兼容性事实、依赖选择、生成、Source SDK profile
和实际消费者，不创建第二套实例或源码图。

详细 port、IRQ ceiling、Tick 和错误语义集中在
[后端集成合同](../reference/contracts/os/backend-integration.md)。应用停机见
[OS 停止](os-shutdown.md)，生产变更按[开发流程](../development/workflow.md)执行。
