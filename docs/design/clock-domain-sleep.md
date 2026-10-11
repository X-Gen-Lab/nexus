# 显式时钟借用与浅睡眠

IO58 当前交付是静态 caller-owned 时钟接纳、排空屏障，以及 STM32F407 / GD32F470
固定 RUN 配置下的一次浅睡眠。组件不创建全局 PM 注册表、任务、定时器、最大对象池或
默认状态。未显式选用的 `Nexus::ClockDomain` 是 `EXCLUDE_FROM_ALL` 静态目标；两个
SoC 的 sleep 源也须由显式可选目标引用。板卡不自动选 GPIO 唤醒或改变电气连接。

公共 API 使用 opaque `nx_clock_domain_t`，mutable storage / constructor 只供私有
装配。消费者提供自然对齐、零初始化的 `nx_clock_lease_t`。接纳成功后，token 的对象
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
同一个保存掩码下最终验证 closed barrier、真实寄存器固定配置和 wake sequence，再
进入既有生产 `nx_arch_wait_for_interrupt`。WFI 保持 PRIMASK，已使能 IRQ 变为 pending
可以唤醒，恢复掩码后运行；返回后 owner 必须重检真实业务谓词。序列变化则成功返回
`slept=false`。SCR.SLEEPDEEP 已设置、STOP、动态 RUN 均明确拒绝；从不改写 SCR、
RCC、Flash、Tick、pending IRQ 或 deadline。32-bit wake sequence 需要调用者证明
观察窗口内不会完整绕回；不声称用一个快照解决 DMA/NMI/SMP 发布或无界 ABA。

实际验证的 STM32 profile 为 nominal HSE8 MHz / M8 / N336 / P2 / Q7，SYSCLK/AHB168、
APB1 42、APB2 84 MHz，Flash5WS，SSCG disabled；GD 为 nominal HXTAL25 / M25 /
N400 / P2 / Q9，SYSCLK/AHB200、APB1 50、APB2 100 MHz，Flash7WS，SSCG disabled、
TIMERSEL=0。来源是 pinned SDK 的 RCC/RCU/FMC register definitions 与当前 SoC system.c
固定启动值。GD main Q9 不能宣称 USB48 MHz，独立 USB clock owner 另负其责。晶振实际
频率、供电、热漂移和板卡布局不能由 ready/divisor 读回测得，仍需 physical qualification。

owner 必须独占 RCC 决策，登记全部相关消费者，并提供实际 enabled wake IRQ；模块不
发现遗漏消费者或隐式暂停内核。实际 deep STOP / PLL 改频扩展必须先排空，停止全部
Tick/time providers、scheduler deadline borrowers 与 IRQ，再由独占硬件端口实施有限
clock restore / rollback；clock/time 的新域需 cold 重建，旧绝对 deadline 不可继承。
本切片没有实现这些扩展，不把 admission epoch 伪装成时间域转换。

Google 测试使用真实生产 domain、真实 Native 排他、两家真实 SDK 的 MMIO布局和既有
生产 Arch sleep，只替换 CPU context / WFI 外部叶子。原始链接 RED、SSCG/TIMERSEL/
non-head token 真实失败回归、GREEN、ASan/UBSan、strict ARM 和身份见
`tools/testing/evidence/clock-domain-iteration/manifest.json`。这不是 clean release、
整机零 RAM/双离线重建或 physical HIL/WCET/功耗资格；这些由后续平台集成/工位证据建立。
