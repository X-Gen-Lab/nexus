---
id: design.clock_domain_sleep
type: explanation
audience: maintainer
source_language: zh_CN
---

# 显式时钟借用与睡眠边界

ClockDomain 提供 caller-owned 时钟借用与排空屏障；F407 的浅 Sleep 与深 STOP
通过不同的显式硬件入口使用屏障。组件不建立全局 PM 注册表、任务、定时器、最大
对象池或默认状态。未选的 `Nexus::ClockDomain` 和 SoC 电源目标保持按需选编。
Board 不自动选择唤醒接线。借用结束条件见
[请求生命周期图](../assets/diagrams/request-lifecycle.svg)，整平台协调见
[系统架构](../delivery/architecture.md)。图中的借用终止不是时钟域转换凭证。

普通消费者通过 `clock_domain.h` 使用 opaque `nx_clock_domain_t`。可信静态装配
owner 通过公开的 `clock_domain_owner.h` 提供实际存储并构造，不需要私有 include
路径；结构字段不是应用控制接口，冷态生命周期和内部 span 协议仍为私有。
消费者提供自然对齐、零初始化的 `nx_clock_lease_t`。接纳成功后，token 的对象
身份和存储被借用，字段由 provider 管理；复制字段不能释放原对象。每个 domain 的显式
capacity 为别名和 retained identity 扫描给出上限，不承诺与容量无关的常数时间。

生命周期的唯一 privileged Thread owner 调用 `close`，禁止新增 lease，再自行推动
每个设备停止、DMA/IRQ/完成通知排空。消费者成功 detach 后才 release。close 本身
不取消设备，release 不证明外设已停止。现存 lease 不因超时、关闭或睡眠被撤回。
`quiescent` 仅在当前 closed epoch 且 count/list 同时为空时返回成功；reopen 只有在
排空后才能单调提升 admission epoch，最大值永久拒绝复用。旧控制值不能跨 epoch
重开或建立排空凭证。domain storage 的生命周期长于全部控制副本，禁止 memset 重置。

metadata 保存恢复 incoming PRIMASK；Native 使用真实递归线程排他模型。acquire /
release 可在 same-CPU privileged Thread、PendSV、SysTick 或外部 configurable IRQ
使用；构造、close、quiescent、reopen 为 serialized privileged Thread。NMI、fault、
SMP、signal 或其他安全域调用不在合同内。输出和 wake word 不能覆盖私有 provider、
retained token 或彼此；拒绝不能破坏已借用控制。没有回调或 token release 后的访问。

SoC sleep 在 incoming PRIMASK/BASEPRI/FAULTMASK 全清的 privileged Thread 调用。
同一个保存掩码下最终验证 closed barrier、所选静态 ClockPlan 的真实寄存器配置和 wake sequence，再
进入既有生产 `nx_arch_wait_for_interrupt`。WFI 保持 PRIMASK，已使能 IRQ 变为 pending
可以唤醒，恢复掩码后运行；返回后 owner 必须重检真实业务谓词。序列变化则成功返回
`slept=false`。浅 Sleep 入口拒绝已设置的 SCR.SLEEPDEEP、深 STOP 和动态 RUN；它不改写 SCR、
RCC、Flash、Tick、pending IRQ 或 deadline。32-bit wake sequence 需要调用者证明
观察窗口内不会完整绕回；不声称用一个快照解决 DMA/NMI/SMP 发布或无界 ABA。

F407 的默认 profile 为 HSE8／PLL168、APB1 42 MHz、APB2 84 MHz 与 Flash5WS。
静态 custom／直接输入使用同一 ClockPlan 的实际常量和只读 RCC 检查，不从默认
数字推导可用性；晶振频率、供电和温漂也不能由 ready/divisor 读回测得。

深 STOP 使用 `nx_stm32_power_enter`，调用者先完整暂停 Platform、排空自己的
ClockDomain 和 OS 时间借用，再显式提供已接纳的唤醒集合。恢复建立新的 Core epoch，
不能复用旧绝对 deadline；可选独立 RTC elapsed envelope 与 OS 时间恢复分别证明。
Standby 的复位、存储丢失及 RTC wake 握手由同一 RTC／电源 owner 协作承担。
完整前提、失败保留与已维护范围以 [F407 System 合同](f407-system-power.md)为准。
ClockDomain 自身不发现遗漏消费者，不暂停调度器、不计算跨 STOP 时间，也不替代
实际硬件关停与唤醒序列。当前不提供运行期 DVFS。

当前测试直接执行生产 domain、Native 排他和 F4 sleep C 实现，只替换 CPU context／
WFI 外部叶子。历史跨系列 RED／GREEN 与测量记录保留原始身份，见
[固定旧源码](https://github.com/X-Gen-Lab/nexus/blob/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18/docs/design/clock-domain-sleep.md)。
本次软件结果须由当前门禁重新建立；主机模型不建立 physical HIL、WCET 或功耗资格。

## 资格边界

公共屏障、浅 Sleep、深 STOP 和所选自主唤醒分别取得证据；通用组件存在不能替代
精确 SoC 集成。旧 H743 pause/resume 不属于当前 F4 provider。主机模型只证明实际
执行的软件合同，ARM 链接只证明 ABI、所选闭包和静态预算。功耗、唤醒边沿、IRQ
延迟和物理保留仍需独立 HIL，当前为 `not_executed`。
