# CPU、原子操作与运行时装配契约

本契约适用于 M0、M0+、M3、M4、M7、M23、M33、M55、M85 的软件机制，以及
Native 模型。实现入口为 [Arch](../../arch/README.md)，具体配置例子和整平台边界
见 [Cortex-M 支持说明](../delivery/cortex-m.md)。CPU 软件支持、具体芯片集成和
物理验收分别建立证据；增加 CPU profile 不增加 SoC、Board、外设或 HIL 资格。

## 唯一事实与依赖方向

`tools/configure/cpu.py` 校验显式 CPU/IRQ 事实，返回不可变 `CpuProfileIR`。
同一解析器服务完整 SoC 装配与外部 CPU runtime 装配，派生编译选项、原子操作
后端、IRQ mask 类型和维护的 FreeRTOS port。编译器宏用于验证所选指令集，
不能替代具体芯片的 DSP、可选 FPU、MVE、cache、MPU、DWT 或 SAU 事实。

完整平台由精确 SoC facts、Board 接线和一份外部 TOML 生成静态 typed factory。
CPU runtime 只读取一份 `runtime.toml` 和它指向的 `cpu.json`，不生成厂商驱动、
链接脚本、向量表、启动、pin route、设备对象或 factory。两种装配都禁止用
CMake cache、运行时寄存器探测或第二份默认配置覆盖已验证事实。

Arch 不依赖 SoC、I/O、内核或厂商 SDK。Core 的 acquire/release publication
直接使用编译器 load/store，不依赖 Arch；OS 与日志的 RMW 使用 Arch 原子端口，
可选 OS 使用 Arch 的合法上下文与掩码机制。产品提供任务、栈、TCB、缓冲区、内存
布局、保护策略和恢复责任。CPU runtime 的 `clock_hz` 是配置事实，不设置时钟，
也不生成可用于 request deadline 的单调时钟源。

## CPU 局部互斥与上下文

`nx_arch_irq_save/restore` 保存并恢复输入 PRIMASK，token 只在同 CPU/线程内
严格逆序使用一次。BASEPRI/FAULTMASK 在存在时保持原值。仅允许特权 Thread
或可配置 IRQ 使用该互斥域；NMI/HardFault 不被 PRIMASK 屏蔽，不得并发访问
其中的元数据。token 不是内核 critical-section token，也不是跨核/跨安全域锁。
被屏蔽段只更新有界元数据，不调用内核、回调，不等待、分配或复制 payload。

`nx_arch_irq_masks` 是查询快照，不是 restore token。Baseline M0/M0+/M23
不读取不存在的 BASEPRI/FAULTMASK。IPSR 身份、特权和安全状态分别查询：异常
号不等于 SoC IRQ ID，也不证明可以调用 FreeRTOS ISR API。Handler mode 的
特权不能由被中断线程的 CONTROL.nPRIV 推断。

DMB、DSB、ISB 保留各自的排序语义，不证明 DMA 排空、外设 idle、缓存一致性或
request SETTLED。DWT 只读显式允许且已启用的特权周期计数器，不启用、不复位；
不可用时返回失败且保留输出。频率变化、暂停和 32-bit 回绕使它不能替代单调
deadline 时钟。真实 IRQ latency、特权切换与周期上界仍需目标芯片测量。

具体核的外部 IRQ 上限：M0/M0+ 为 32；M3/M4/M7/M23 为 240；
M33/M55/M85 为 480。SoC 必须给出实际有效 vector 范围，不能以体系结构最大值
代替芯片事实。M0/M0+/M23 固定 2 个优先级位，Mainline 按事实声明 3～8 位。
系统异常不计入外部 IRQ；CMSIS 寄存器数组容量不是处理器能力上限。
M33 的 DSP 是可选实现，必须声明 `dsp`；关闭时生成 `+nodsp`，不能使用
编译器默认的 DSP 指令能力替代芯片事实。M4/M7/M55/M85 固定具备 DSP，
M0/M0+/M3/M23 没有该扩展；矛盾事实直接拒绝。

这些边界核对了 ARM 的 [M23 TRM](https://documentation-service.arm.com/static/63dbc7dc53e02459952dba53)、
[M33 NVIC](https://documentation-service.arm.com/documentation/100230/0100/Introduction/Component-blocks/Nested-Vectored-Interrupt-Controller)、
[M55 NVIC](https://documentation-service.arm.com/documentation/101051/0101/Nested-Vectored-Interrupt-Controller-/NVIC-features)
与 [M85 NVIC](https://documentation-service.arm.com/documentation/101924/0101/Nested-Vectored-Interrupt-Controller-/NVIC-features)。
M7/M55/M85 的 cache line 为 32 bytes，仍须显式声明芯片是否实现对应 cache。

## 32-bit publication 与 RMW

`nexus/arch/atomic.h` 只操作仍然存活、自然对齐的 `uint32_t`。初始化发生在
发布前；所有并发访问使用这些 helper 或兼容的编译器原子操作，不能混入普通
读写。它不用于 MMIO，不保证 DMA 或其他 bus master 的原子性。

Acquire load/release store 在所有维护的 CPU 上使用编译器原子指令，不屏蔽
IRQ。成功 compare/exchange 使用 acq_rel、失败使用 acquire；失败会更新调用者
私有 `expected`，不会伪造 spurious failure。fetch add/sub 返回原值并按 32-bit
模数运算。发布状态与被发布 payload 的生命周期仍由 owner 负责。

M0/M0+ 的 RMW 使用固定范围的 saved-PRIMASK 读改写，再恢复输入 mask。
仅同 CPU 的特权 Thread/可配置 IRQ 可并发参与；NMI、HardFault、非特权线程、
另一个核、DMA 或另一个安全域不能同时访问该存储。这是 CPU 局部机制，不宣称
lock-free 或 bus atomic，也不静默链接 libatomic。其 masked 范围没有循环、
回调、分配、阻塞和 payload copy；有界指令序列不等于已测得最坏 cycles。

其他核使用编译器 lock-free word RMW。独占指令可能重试，lock-free 不等于固定
周期上界。M23 属于这一后端，不能依据它没有 BASEPRI 就归为 M0 的 RMW 后端。
停用对象前必须停止所有访问者；release/acquire 不自动回收 request、通知或队列。

## Cache 与指令同步

`cache.h` 区分 data clean、invalidate、clean-invalidate 和 instruction sync。
能力来自精确 CPU facts，维护的 cache line 为 32 bytes；零表示没有该能力。
每次操作要求非零、完整 line 对齐的范围，地址/长度不得溢出 32-bit CPU 地址
空间，不向外取整。调用者独占每一整条 line，并排除其他 CPU、DMA 和安全域的
访问，保持 cache 配置稳定。

只允许特权 Thread/可配置 IRQ；操作不自动屏蔽 IRQ、不启用 cache。存在但关闭
的 cache 返回 UNSUPPORTED。instruction sync 在任何维护写入前检查全部相关
cache，调用者还须阻止该范围的执行。函数不 flush 无关 dirty line、不验证代码，
也不进行全局 branch-predictor 维护。

Invalidate 可以丢弃 dirty data，调用者明确承担这一后果。DMA receive 的前置和
排空后的维护由具体 provider/产品协调；cache 操作本身不证明 DMA idle，也不
结束 request/buffer 借用。工作量随 complete line 数量增加，不能笼统称为 O(1)。

## MPU 编码与单区域编程

`mpu.h` 保留独立 v7 size/subregion 格式与 v8 base/limit/MAIR 格式。纯 encoder
在任意 profile 上可用，拒绝非法或保留编码、错位和不完整边界，不截断/取整，
失败保留输出。输入和输出不能重叠。编码不证明 SoC 可达性、DMA coherency 或
产品权限正确性。M0+ 的可选 MPU 属于 ARMv6 PMSA，使用兼容 RBAR/RASR 格式；
这不改变其 ARMv6-M 指令架构。M0 不提供 MPU 编程能力。

实际 region programming 要求本安全域内特权 Thread、输入 PRIMASK=1、MPU
已经 disabled，并由调用者独占 selector 和对应状态。实现先校验 profile、字段、
实际 TYPE region 数量与上下文，再写所选区域；保留 CTRL 和输入 RNR，不自动
启用、不 clear 所有 region，不访问另一个世界的 alias。Handler 或 enabled MPU
调用被拒绝，不通过自动关 MPU 改变保护策略。

v8 MAIR 更新只改变指定 byte，保留相邻属性。调用者拥有使用该 attribute 的
所有 region 与有效内存策略；实现不做 cache 维护。MPU 的后续 enable、重配、
上下文切换和 FreeRTOS 用户 task 保护集成均属于外部设计，不能由 encoder 或
单 region programming 声称已经实现。

## 安全状态与 SAU

`security.h` 查询此 image 的显式 single/Secure/NonSecure 状态。查询不授权
跨世界访问，也不探测或推断真实 SoC IDAU policy。SAU encoder 验证精确的
32-byte base/inclusive-limit 与 NSC 标志，失败保留输出。

只有明确 Secure 编译与事实声明了 SAU 的 image 可以进行 SAU MMIO。写/清单个
region 要求 Secure 特权 Thread、输入 PRIMASK=1、SAU disabled 和独占 ownership；
实际 TYPE region 数量在写入前校验。保留 CTRL、ALLNS、输入 RNR 和其他区域，
不 enable SAU，不使用跨世界寄存器。NonSecure/single image 不写 SAU MMIO。

IDAU、重叠规则、Secure boot、NSC veneers、双世界 gateway、共享资源协议、密钥
和安全升级由产品/具体 SoC 接入承担。Secure-only 或 standalone NonSecure
FreeRTOS kernel 编译不代表已支持成对世界 context、Secure task gateway 或用户
MPU task 隔离。这些项目没有本轮物理验收证据。

## 验证与可支持性

GoogleTest/GoogleMock 在窄指令/MMIO 边界替换硬件，运行生产 C 算法，覆盖非法
字段/上下文在状态变更前拒绝、selector/mask 保存恢复和各可选能力组合。Native
运行真实递归线程互斥、C11 fence 与 publication/生命周期行为；Native cache/MPU/
SAU MMIO 返回 absent-hardware 结果，纯 encoder 仍可使用。模型不执行真实 NVIC。

锁定 ARM 编译器门禁分别验证九核的宏、ABI、对象、指令和未解析符号，再编译
所选 Core/Arch/OS kernel 组合。FPU/MVE/security 的可选组合按事实单独执行，不
从一种组合继承到所有组合。每次记录真实命令、返回码、工具身份与输出 hash。

本轮执行证据由当前源码重新生成；正式 clean-source SDK、离线双构建和 candidate
封存还须遵循 [qualification](../delivery/qualification.md)。旧 HEAD 的通过数、
candidate 或硬件状态不能继承。当前整平台仍限已集成的 F407/F470 三板，没有
新增真实芯片/Board，也没有实板 HIL。软件模型或成功编译不提升产品 release。
