---
id: reference.contracts.os.execution
type: reference
audience: maintainer
source_language: zh_CN
---

(reference.contracts.os.execution)=
# OS 执行与生命周期合同

OS 提供有界等待、显式任务和同步存储，设备接口不依赖 scheduler。默认固件是 C11、
静态装配、零平台堆；Native 的 pthread 库内部 bookkeeping 不构成 MCU 零堆证据。
产品的任务角色、优先级、栈、队列容量、启动、停止、安全输出和恢复策略由外部工程拥有。
本页描述当前分支已实现的公共接口；最终软件候选、SDK 消费和物理资格仍按各自
实际执行结果验收，不由接口或本文档自动取得。

## 分层与依赖

![当前等待与退出：ready-arm-ready-wait-ready，执行者与join责任由外部管理](../../../assets/diagrams/wait-wake-shutdown.svg)

当前执行语义见[FIG-08及源码依据](../../diagrams.md#fig-08-wait-wake与外部退出)。
后端和完整平台边界见[FIG-01](../../../assets/diagrams/platform-architecture.svg)。
- `Core` 保持无 OS/Arch 依赖。发布的 acquire/release 语义独立于内核。
- `Arch` 拥有 CPU-local IRQ 掩码、原子操作和浅睡眠原语，不拥有 Tick、任务或设备。
- `OS` 以共同接口连接所选后端并检验真实能力；公共 Wait 本身不创建任务、推进设备或派发。
- `SoC` 拥有时钟、控制器、实际 IRQ 和可用计时器；Board 拥有实际引脚、电气与外部器件。
- `Product` 创建 owner/producer/coordinator，提供 timer/trace/fail-stop port，并启动 scheduler。
  `nx_platform_start()` 不替产品启动 scheduler。

业务通过公共 opaque 任务、通知、queue 和 channel 引用调用共同接口。定义静态存储的
装配文件包含 `nexus/os/storage.h`，目标 include 路径提供所选后端的真实布局；不以
最大 union、固定字节 TCB 池或第二套 sizeof 清单统一二进制大小。每个可执行文件只
链接一个导出共同符号的后端，不增加运行时 OS 判断或分派表。专有能力继续使用明确
扩展。生成的 typed factory 返回既有设备 face，不创建 OS 对象。

Native entry 可以在 `pthread_create` 返回前执行。入口自己的 join 必须返回
`CONTEXT`，且不能读取外部 owner 尚未发布的 pthread identity 或 started 状态。
其他 start/join 操作仍由唯一外部 lifecycle owner 串行执行；这一修正不提供多 joiner
或跨任务竞争 start 的能力。创建失败不保留 entry/context；资源耗尽与非法参数分别报告。

## 等待、时钟与完成

`nx_wait_until()` 使用 `ready → arm → ready → wait → ready`。后端必须把序号观察与
进入等待作为不会丢失已锁存 wake 的操作。wake 只是提示；业务 predicate 是结果的
唯一权威，通知序号不转移 payload 所有权。单 port 只有一个 waiter，允许后端合同内
多个 publisher；从 arm 到 recheck 之间必须少于 2^32 次 wake。

任务后端的 deadline 是 `nx_os_now_us()` 时钟域中的绝对单调微秒。Native 使用真实
`CLOCK_MONOTONIC`，不混用外设模型的手动时钟；FreeRTOS／ThreadX 使用 Core
`nx_time_now_us()`。裸机使用初始化时提供的时钟。不得在活请求期间
切换时钟域。`NX_DEADLINE_NEVER` 真正表示无限等待，不把 UINT64_MAX 转成有限
`timespec`。微秒表达不保证微秒调度精度；阻塞误差还包括 Tick 量化和调度延迟。

有限 notify deadline 在检查 hint 时仍必须检查过期，持续无关 wake 不能续期。公共
helper 在后端错误后保留一次最终 `ready` 检查：已观察到真实完成可以胜出。
裸机未就绪返回 `BUSY`，调用方继续显式调度。队列立即可用的操作采用 try 语义；
只有需要阻塞时才消耗 deadline，失败不改调用方输出。
公共 queue 的 deadline 零表示 try，channel 关闭后发送和排空后的接收返回
`CANCELLED`。有限 `nx_sleep_until` 已过期时立即成功，拒绝无限 sentinel；`yield`
只提示调度，不承诺公平、指定下一任务或低优先级进度，两者都不创建 timer worker。

`nx_wait_select()` 的校验和就绪扫描随候选数线性增长。每个 predicate 必须有界，
不阻塞、不执行设备 service；调用者在成功消费后推进自己的 cursor，才能公平轮转。
比较 1、4、16 个候选时应记录扫描次数、任务负载和事件消费规则，不能从一次主机
耗时推导 MCU 最坏延迟。通知合并不累计业务事件，队列或锁存状态才拥有事件数量。

等待不推进 UART/SPI service。共享控制器的生产者不能在 `ready` 内 service；其唯一
execution owner 在取消、停止和排空期间仍须运行。timeout/cancel 请求不等于最终
settlement，不释放 request、operation、DMA buffer 或 notification storage。

## 中断政策与上下文

CPU/SoC facts 包含实际 IRQ 数、实现优先级位数和分组能力。OS runtime policy 明确
区分无内核、PRIMASK 内核和 BASEPRI 内核，不能仅凭 ceiling=0 推导内核不存在。
Mainline 调用内核 ISR API 前验证实际 publisher IRQ、priority width、分组和 syscall
ceiling；Baseline 按 PRIMASK port 合同检查实际 configurable IRQ。typed IRQ sink
和 priority policy 只约束所选 provider/backend/mode 已实现的通知路径；类型存在或
ceiling 校验通过不建立新模式、新内核或实板 IRQ 资格。不支持的组合明确拒绝。

NMI、HardFault 和内核专属异常不进入这些 kernel wake API。MCU Task API 要求当前
世界 privileged Thread mode，运行时手工掩码已恢复。阻塞、transfer、join 和 sleep
只在合法运行任务且 scheduler 未挂起时执行；构造与非阻塞发布按各后端合法的
task/bootstrap 范围执行，不依赖主动挂起内核后的专有可调用性。启动阶段只允许
所选 Port 的规范 bootstrap mask；数值相同不证明掩码所有权。已有上下文检查在
对象变更前拒绝违约调用，但不能代替外部生命周期和完整调用前提。

`nx_notify_wake` 面向 task/bootstrap，`nx_notify_wake_from_isr` 面向受审硬件 IRQ；
绑定后的 `port.wake` 保留该后端允许的 task/bootstrap/IRQ 发布域。Native 的 ISR
入口返回 `CONTEXT`，POSIX signal handler 不能借用普通任务合同。

只使用 CPU-local mask 不保护 SMP、DMA 对元数据写入或另一个安全世界的 publisher。
IRQ sink 和 context 必须一直存在到实际 provider 停止并证明 publisher quiescent。

## 静态存储与任务

首次使用前将控制对象清零。任务通过选定布局声明对象，提供非空名称、字节栈容量和
context；原生类型只留在装配、后端实现和明确扩展中。创建前按当前 Port 的初始
frame 和对齐要求校验实际栈容量；启动最低容量不能替代任务运行栈预算、异常嵌套 MSP 预算和 FP/MVE
context 预算。Native 栈须满足 pthread 平台下限并加上应用预算，不能继承成 MCU 成本。

逻辑任务优先级统一为高数值高优先级，由后端映射原生编号；ThreadX 原生优先级
方向相反，不直接传给公共接口。Native 只接受逻辑优先级 0，不忽略不支持的等级或
声称拥有 MCU 抢占保证。内核 tick、时间片与真实响应仍由所选后端和预算决定。

应用通过 `nx_os_run` 显式进入真实 bootstrap，成功不返回；Platform start 仍不启动
scheduler。Native 执行回调后停驻当前引导线程，保留其调用栈，不增加隐藏 coordinator；
进程退出由应用明确决定。不同内核的 bootstrap 可调用范围分别维护，不能把运行态
重新解释为冷态或在引导回调中等待未来任务。

MCU 首次 scheduler 启动采用 cold privileged Thread/MSP 合同：`CONTROL.nPRIV=0`、
`SPSEL=0`、`FPCA=0`。启动前不得留下 active FP context 或把 main 切换到 PSP；调用方
不能仅清掉标志来丢弃仍活跃的 FP 状态。MPU bootstrap guard 绑定真实 protected startup
指令、基本 exception frame 与正确 EXC_RETURN，一次性 lease 在 restore 前消耗，重复
raw START SVC 不能再次取得启动权限。任务开始后的 FP/MVE 使用由对应 Port 管理，
不是禁用应用浮点能力；实际 context retention 仍需要硬件验收。

可 join 任务返回后的机制由后端决定：FreeRTOS wrapper 发布完成后挂起，等待外部
joiner 删除；Native entry 返回 pthread，由真实 try/monotonic clock-join 证明线程
退出；ThreadX wrapper 发布完成提示后返回内核 completion 路径，join 仍检查完成状态并执行删除。
三者均由唯一外部 lifecycle owner 完成 join；成功前不得重用 TCB/栈/context，
join 超时保留全部存储。Native 禁止绕过 adapter 执行 `pthread_cancel`、detach
或另外一次 join。完成提示本身不能替代后端的实际退出与引用撤离证明。
公共入口为 `nx_task_join_until`。Native 有限等待使用可真实链接的 GNU clock-join，
不在完成标志后调用无界 join；仅显式 `NX_DEADLINE_NEVER` 允许无限等待。

永久任务使用独立显式 API，entry 不得返回，control/stack/context 保留到 reset；它
没有 completion semaphore、join 或 destroy。适用于产品确实具有永久生命周期的任务。

direct task notification 是显式 opt-in：调用方指定已存在的 receiver 和独占 notification
index，只有该 receiver 等待。停止并 join publisher 后才能解绑；receiver handle 和
storage 一直存在到所有保存的 port 都不再可用。平台不隐式占用应用 notification 槽。

## 队列与关闭

公共 raw queue 保持最小存储和后端阻塞路径，不包含 close 广播。初始化要求真实
`storage_bytes` 至少容纳 `depth * item_size`，完整声明容量跨度不得回绕或与完整控制
对象重叠；不能仅检查有效 item 区间。绝对 deadline 入口复用同一时钟域，并
避免连续操作把相对等待预算重复放大。较大数据优先排队稳定 slot/handle，而非在
内核临界区复制整个 payload；slot 本身仍有明确借用和回收规则。

可关闭 channel 使用单独 opt-in 类型和精确最大并发操作数。每个并发操作持有独立
调用方 waiter 存储；close 停止 admission，锁存并唤醒已注册 waiter。关闭后旧 item
可继续 drain，不能继续 send。destroy 需要 closed、drained、users/publishers quiescent。
不得用 periodic polling 冒充立即关闭唤醒，也不得让 raw queue 默默承担这项成本。

FreeRTOS closable registry 仅供 task 使用，以 scheduler suspend/resume guard 保护
admission、waiter 注册/退出和 close 广播。内核 give 可能触发 yield，不能假设一个
IRQ critical token 能跨越这个 yield 保持链表不变。maximum_waiters 预算 close 的
调度暂停时间；单 item 复制与 latch give 的 IRQ 屏蔽时间另行测量，不能混为一个成本。
Native 使用同一 mutex 下的条件变量广播和最小 waiter active guard，不复制 RTOS
等待者链；同样检查 waiter 独占与最大并发操作数。raw queue 和 channel 的控制类型分开。

## 配置与成本

平台 schema-2 和独立 runtime schema-1 TOML 沿既有配置路径选择后端。FreeRTOS 的
可选 `[os]` 由 `tools/configure/kernel.py` 解析为不可变 `KernelProfileIR`；ThreadX
独立政策见[ThreadX 配置](threadx.md)。不为普通公共接口再增加配置权威。FreeRTOS 默认
`standard` 延续 1 kHz/八级优先级/Idle 128 words/16-byte name；`minimal` 缩短
name 与优先级列表并关闭未用同步能力；`diagnostic` 开 trace；`lowpower` 显式开
custom tickless。生成的实际能力为准，不能用 profile 名推导芯片能力。

可覆写字段为 `tick_source`、`tick_hz`、`max_priorities`、`syscall_priority`、
`idle_stack_words`、`max_task_name_len`、`notification_slots`、`mutexes`、
`counting_semaphores`、`task_notifications`、`trace`、`runtime_stats`、`tickless`。
安全 profile 另有 `memory_protection`、`system_call_stack_words`、`security_model`、
`secure_idle_stack_bytes`。默认 MPU 关闭、single-world；MPU 系统调用栈默认 128 words，
64–65536 偶数 words。split 的 Secure Idle 默认 128 bytes，64–65536 且 8-byte 对齐；
single 不接受作者显式设置此字段。CPU 的 `mpu_regions` 是物理事实，未声明为 unknown，
不能由运行时政策凭空生成 MPU。受维护安全组合见下节，不由参数校验成功推导支持。
SysTick 使用真实 core clock，要求频率整除且 reload 在 24-bit 合法范围；外部 Tick
必须实现强 `nx_freertos_external_tick_setup(uint32_t)`。runtime stats 必须实现
counter start/now；缺失 hook 使链接失败，不提供伪时钟或永远为零的统计。
内核的 mutex／counting semaphore 配置开关不自动建立公共对象 API；实际公共头和
配置能力校验决定通用消费者能使用什么，不能从内核宏推导已维护接口。

成本报告分开计算 Wait face、notify、raw/closable queue、每个 waiter、joinable/
permanent TCB、调用方栈、Idle 栈和内核全局对象。保持现有 four-pointer Wait ABI；
共享 ops 表是否值得采用由实际 linked image 和实例数决定。构建时间报告只比较
RAM/Flash/instruction，CPU cycle/latency 必须实测，不能从反汇编字节数推导。

| 选择 | 适用条件与额外责任 |
| --- | --- |
| 普通 notify | 调用方持有独立通知控制；publisher 全部退出后才销毁 |
| 直接通知 | 已存在且唯一的 receiver、独占 index；receiver 生命周期覆盖所有 publisher |
| joinable task | entry 可以返回；completion 后仍保留 control/stack，直到外部 join 成功 |
| 永久任务 | entry 永不返回；没有 join/completion 存储，误返回必须 fail-stop |
| 原始队列 | 已有独立停止协议；payload copy 成本随 item 大小增长 |
| 可关闭队列 | 需要关闭 admission 并唤醒 waiter；额外 control 和每操作 waiter 由调用方提供 |

Native destroy 可能只释放部分 POSIX 资源。首次销毁撤销公开操作准入；每个资源只在
实际销毁成功后解除 ownership。失败后保持同一 control，仅重试 destroy；不得重新
init、进入 wait/send/receive 或释放存储。关闭和 join 仍须先完成，不能依靠 destroy
替代 publisher quiescence。内核 queue payload、调度器挂起区与实际 IRQ mask 区的
成本分别计算；scheduler suspended 不等于所有中断均被屏蔽。

## 产品停止顺序

1. coordinator 关闭 admission；owner stop 请求所有已接受操作取消。
2. owner 保持 service/drain；producer/observer 仅观察 acquire `SETTLED`。
3. coordinator join producer；必须已 acquire 观察每个 accepted request 的 `SETTLED`。
   再排空 slot control 并 detach/join observer；最后一次 callback 返回之前，request、slot
   和观察者存储均不可复用。业务 payload 还须满足其自身的其他借用合同。
4. coordinator 请求 owner 停止并 join；同时确认 owner idle 包含 publication 已退出。
5. 停止硬件 RX/IRQ publisher 并 detach wake；join 其他可能的发布者。
6. 销毁 notification/queue/guard。仅在全部消费者退出且所选生命周期允许时停止
   platform；正在运行的内核继续保留时钟和 Tick。没有通用 scheduler shutdown，
   不能从 I/O idle、任务返回或普通停机接口推导内核已经停止。

失败保留 storage，并由产品选择恢复。`QUARANTINED` 是仍有引用的状态，不是允许
释放的终态。一次 destroy 返回 BUSY 只是防护，不替代未来 publisher 已停止的证明。
每个成功 milestone 只执行一次；超时继续使用原 absolute deadline 的结果，不靠反复
构造新 deadline 掩盖失败。恢复重试由外部策略给出新预算，且继续保留未归还资源。
外部 [UARTOwner 示例](https://github.com/X-Gen-Lab/nexus-examples/tree/codex/next-generation-platform/apps/uart_owner_runtime)
实现单 coordinator 的单调停止 milestones；失败后只重试失败阶段，不重复已成功 join。

## 低功耗、诊断与安全边界

Arch 的序号复查与浅睡眠保存恢复 incoming PRIMASK；WFI 前 DSB，明确拒绝默认深睡眠。
应用负责唤醒 timer 和持续时钟。FreeRTOS tickless 使用 caller-owned timer port：精确
Tick phase、暂停/恢复、绝对 alarm、连续 clock 与异常 overshoot 都有独立合同。未绑定
port 时维持普通 Tick，不猜低功耗设备或 Board 唤醒引脚。

诊断 ring 由调用方提供 capacity；写满不覆盖旧事件，lost counter 饱和。内核 trace
默认 weak no-op，无默认大 buffer；产品覆盖 sink 并负责消费。异常 hook 是有界平台
扩展点，安全输出、watchdog/reset、事件持久化和释放策略归产品。

显式 MPU profile 使用真实 restricted-task kernel port、专用 linker regions 和窄 SVC
gateway。调用方在 privileged RAM 提供 TCB/context/syscall stack，在 user RAM 提供
实际任务栈；受保护任务至少 64 words，运行峰值另行预算。只允许普通 RAM 的 RO/RW
且 XN 数据授权，拒绝 MMIO、privileged RAM、越界、重叠或非法 context。默认普通代码、
数据和 provider 均 privileged；只有显式 user text/data 可进入用户区。

默认公开 user 服务为无指针的 delay/tick-read，经真实 MPU veneer/SVC。当前
F4 生产集成不提供 scalar GPIO gateway；公共 copied broker 机制与具体
内核／安全域入口分别维护，不建立任意指针调用或隐式升权。
受维护基础软件组合为 M3、M4 integer/
FP、M23、M33 integer/FP、M55 integer-MVE、M85 float-MVE 及明确 16-region 事实。
Native、M0/M0+、M7 kernel MPU 和 MPU+split 当前 fail closed。基础 Arch/普通 OS 对
这些 CPU 的支持仍独立存在。用户任务不是可 join wrapper，必须永久运行；单 privileged
owner 在所有外部用户 quiescent 后显式删除另一任务，自删拒绝。没有隐藏 reaper。

显式 split 由两个独立 Runtime 图像组成：Secure 侧是明确 Secure CPU facts、baremetal
runtime 与 `Nexus::SecureContext`；Nonsecure 侧选择匹配 CPU facts 和 split FreeRTOS。
匹配 ISA/FP/MVE/float ABI，Secure link 导出七个真实 SG veneers 与 CMSE import object，
NS link 消费该 import 并使用实际 CM23/33/55/85 的 TrustZone-aware non_secure ports，
不能给 NTZ port 设置宏来冒充跨世界 context save/load。

每个获准 NS task，包括内核 Idle，均由调用方提供 NS TCB/stack 和独立 Secure
metadata/stack。Secure startup 先 prepare，可信 immutable task→context selector 只
返回已准备身份；未知身份拒绝。Secure 栈须 8-byte 对齐，实际 allocation 包括
8-byte seal，64-byte port minimum 不包含 seal，也不代替业务栈预算。split 需要消费者
提供 Idle callback 和匹配 Secure Idle 存储；政策只控制 request budget，不提供默认池。

租约身份包含可信 task 与单调 epoch；free 保留 metadata/epoch 历史，join 与 observers
quiescent 后才能 rebind。已 load 的 context 不能 free。真实 PSP/PSPLIM assembly 处理
save/load，保留 Secure/NS incoming PRIMASK；有界完整性检查发现 seal、stack bounds 或
context 破坏后，保留 lease 并进入不返回的 Secure fail-stop，不释放或隐式恢复。
默认保持双域掩码并持续 spin，没有完成时间界；产品负责明确的安全记录/reset 策略。
NMI/HardFault 不访问这些共享 metadata。NS SVC/PendSV kernel 是受信任边界：普通 NS
Thread 不能伪装 Handler 调用，但机制不防御恶意 privileged NS kernel 伪造自身 task 参数。


## 自动化与实际证据

行为变更先记录真实 GoogleTest/GoogleMock RED，再实现 GREEN，最后重构。mock
外部硬件、CPU/内核边界或产品调度动作，不 mock 实现本身。等待竞态以 barrier、
可控时钟和显式调度构造，不能依赖 sleep 长短来证明同步。

配置和 evidence 工具使用 Python unittest；hook 和 CI 执行实际发现的完整 suite，
拒绝零测试、过滤、skip-only、陈旧 JUnit 与缺工具。真实 POSIX scheduler 验证 kernel
集成；ARM 编译/link/ELF 报告验证 ABI 和资源，不证明 MCU 时序或 context 电气行为。

`tools/hil/os_acceptance.py` 提供 OS49–55 的 unbound 计划与严格物理记录验证。验证
重建 admission，绑定实际 ELF/config/CPU ABI/station/workload/instrument，检查完整
register bank、MSP/task peaks、逐样本 deadline、无损低功耗事件和实际安全输出。
工具不运行探针、串口、供电或 Flash；没有设备时所有物理项保持 `not_executed`。

## 内核升级与新后端接入检查表

内核升级先在独立分支锁定 commit/tree 和所有实际使用 Port；不要只升级版本字符串。
审核配置宏、TCB/queue ABI、初始 frame、ISR priority gate、yield/critical 行为和任务返回
wrapper。M7 integer overlay 重新确认上游文件 SHA、预期替换次数、incoming PRIMASK
恢复及 errata barrier；原始 vendor 文件保持可核对。FP/MVE 保存与 coprocessor 初始化
需要编译宏、真实 ASM/disassembly 和最终硬件 context-switch 记录共同确认。

升级通过全部 GoogleMock/context tests、真实 POSIX lifecycle、九核链接、profile
正负路径和 formal installed SDK 后，重新记录成本与当前 source identity。旧 clean
candidate、旧 ELF 和旧物理报告都不能自动授予新内核资格。产品负责批准其实际任务
预算、timing/HIL、恢复策略和发行范围。

新 RTOS/backend 必须逐项提供：

- 明确 startup/running/suspended/task/IRQ 上下文、scheduler 启动责任和中断政策。
- 同域单调时钟、有限及无限 deadline、arm/recheck/wait race 和单 waiter 合同。
- 每类对象的精确存储、对齐、干净拒绝无新增借用、部分清理失败保留资源及关闭／destroy 条件。
- returning/permanent 任务生命周期、单 joiner、超时后引用保留和存储复用证明。
- producer/owner stop 与 request drain 能继续进展，不用强制删除任务解除 borrow。
- CPU Port/ABI/context save、实际源依赖、最小资源报告和 missing-hook 链接拒绝。
- 真实 backend execution 与同一 GoogleTest 合同；新 kernel 不通过只有 mock 的验证。
- 正确标注 Native/model、ARM link 和 physical station 的不同资格，禁止推测新板接线。

## 错误与恢复责任

| 结果/状态 | 调用方责任 |
|---|---|
| `BUSY` | 保持 storage；非阻塞调用方继续显式 service/pump 或按应用政策重试 |
| `TIMEOUT` | 只结束本次等待预算；不能释放尚未 SETTLED 的 operation/bytes |
| `CANCELLED` | channel 停止发送准入，已有 item 继续排空；仍需会合用户再销毁 |
| `CONTEXT` | 先恢复合法任务/IRQ/掩码/scheduler 上下文，不在 guard 内重试 kernel |
| `STATE` | 检查对象生命周期、close、stale ticket、receiver/index 和唯一所有者 |
| `QUARANTINED` | 保留全部 borrow，owner 继续 recovery/drain，产品决定安全输出/reset |
| `UNSUPPORTED` | 在配置/接入阶段选择真实能力；不静默启用替代 worker 或改变模式 |

平台不把这些结果自动映射成 retry、重启、写 Flash 或恢复安全输出。生产策略必须
使用产品自己的预算、告警和状态机，并保留实际失败路径的原始证据。
