---
id: guides.os-backend-porting
type: how-to
audience: maintainer
source_language: zh_CN
---

# 维护或接入 OS 后端

已有后端先从 TOML 选择，不为每块板复制 OSAL。新后端必须实现明确的任务、同步、
时间和生命周期合同；不能仅增加 backend 名字就宣称支持。当前公共任务、通知、
queue 和 channel 接口由所选后端实现，最终软件候选和硬件资格仍须单独验证。

## 确定所需子集

先列调用方实际需要的静态任务、队列、通知、等待和时间能力。未实现能力明确拒绝；
ThreadX 的 Native／M4 子集不继承 FreeRTOS 的 tickless、隔离或端口范围。
CPU 原语、SoC 时钟与 OS 内核职责分别维护。

业务头只使用公共 opaque 引用。静态装配包含 `nexus/os/storage.h`，由目标的
include 路径选择完整布局；每个可执行文件只链接一个后端。不建立最大 union、
固定 TCB 字节池、运行时 OS 判断或第二层 vtable。裸机保持非阻塞机制，不提供
假任务；要求任务、有限 join 或关闭广播的消费者在能力不足时配置拒绝。

## 以一条实际路径接入

1. 锁定内核、端口、许可证与编译上下文，在 `os/<backend>/` 实现窄 adapter。
2. 完成公共 struct 标签及真实选定布局；调用者清零控制对象，提供字节栈容量和完整
   payload 容量。准入检查完整跨度、对齐和重叠，不仅检查有效 item 区间。
3. 实现共同符号及同域绝对单调 deadline；映射高数值高优先级，分离 Task／ISR
   入口，拒绝非法上下文，不忽略 Native 无法提供的优先级保证。
4. 验证真实任务执行、部分创建失败、通知竞争、超时、关闭和失败排空。
5. 编译所维护的 CPU／ABI 组合，再单独记录 MCU 上下文、栈峰值和时序实测。

应用拥有 scheduler 启动和产品 worker。Platform start 不进入内核；任务入口返回
或通知到达不授权回收 stack／TCB。内核仍使用的 Tick、时钟和 IRQ 必须保留。
公共 `nx_os_run` 通过真实 bootstrap 启动，成功不返回；Native 在回调后停驻原调用
线程以保留其栈。`nx_task_join_until` 成功必须证明真实内核引用撤离，有限 join
不能退成无界调用。Native 要求可真实链接的 GNU monotonic clock-join 能力。

## 与配置和构建组合

后端的能力与策略字段进入公开合同和 typed IR，实际 CMake 目标发布为所选
`Nexus::OS`。新增后端同时维护兼容性事实、依赖选择、生成、Source SDK profile
和实际消费者，不创建第二套实例或源码图。
配置工具处理能力与静态选择，不实现内核行为。复用同一 CPU／port 合同的新 MCU
提供相应 SoC、时钟和 IRQ 事实，不复制公共 OS API 或逐芯片增加行为分支。

消费者在显式调用 `nexus_add_platform` 或 `nexus_add_runtime`、并建立应用 target
后声明必要能力。下面是片段，假定装配和 `app` 已存在，不是完整工程：

```cmake
nexus_require_os_capabilities(TARGET app
    CAPABILITIES tasks notify queue channel join_until sleep_until)
target_link_libraries(app PRIVATE Nexus::OS)
```

该函数只校验选定 `Nexus::OS` 的能力元数据，不隐式添加链接。缺失能力、拼写错误、
尚未选择装配或不存在的应用 target 都在配置阶段失败。CMake 的 compatible-interface
约束另行拒绝同一可执行文件链接两个 task 后端，不依靠运行时发现冲突。

Native 不提供 `priority` 或硬件 `isr_notify`；Host ThreadX 不提供硬件 `isr_notify`；
裸机仅声明 `wait`，不能接纳要求 task-profile 的消费者。task-profile 是能力集合的
说明，不是新增 TOML 字段。配置与源码仍沿同一目标图维护。实际回归入口为
[`test_platform_entry.py`](../../tools/testing/execution/test_platform_entry.py)，不能仅凭此片段
宣称另一个后端或目标已验证。

详细 port、IRQ ceiling、Tick 和错误语义集中在
[后端集成合同](../reference/contracts/os/backend-integration.md)。应用停机见
[OS 停止](os-shutdown.md)，生产变更按[开发流程](../development/workflow.md)执行。
