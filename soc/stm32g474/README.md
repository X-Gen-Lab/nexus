# STM32G474 provider

This provider implements an exact STM32G474RET6 software reference: Cortex-M4F
startup, one fixed 170 MHz clock transaction, independent TIM2 time, statically
selected GPIO banks, six IRQ-byte UART controllers, SPI1/SPI2 polling endpoints,
TIM3/TIM4 CH1 fixed-base PWM, independent IWDG and PA0/PA1 EXTI events. The
acquisition software fixture adds ADC1 PA0/IN1 shots, finite TIM6-triggered normal
DMA blocks, I2C1 PB8/PB9 finite polling and full physical dual-bank Flash. Public
typed faces share readonly
operations tables and keep mutable state private. There is no allocator, worker,
runtime registry or automatic IRQ/DMA/polling fallback.

The silicon route inventory covers the 52 package GPIO pads. The fixed profile
reserves PF0/PF1 for HSE and PG10 for NRST until an explicit reviewed option-byte
policy exists. Pad inventory does not authorize a PCB connection. USB and
Ethernet remain unsupported here. ADC and I2C routes are software fixtures with
no populated analog circuit, pull-ups or external device asserted.

FDCAN1 now has exact STM32G474RET6 ISO-FD normal and internal-loopback software
selections at 500 kbit/s / 2 Mbit/s, fixed 212-word physical message RAM and three
hardware-priority TX slots. See [the CAN FD ownership contract](../../docs/design/can-fd.md);
transceiver wiring, physical timing and HIL remain unverified.

The source-reviewed reference Board is NUCLEO-G474RE, MB1367 C04/C05 documentation.
The physical PCB revision, fitted crystal and solder bridges have not been
inspected. No physical test has executed. The initial reference selects the
24 MHz X3 crystal, PA5/LD2 and LPUART1 PA2/PA3 AF12 VCP routes. Applications must
confirm the Board profile and own LED/console policy. See
[the detailed contract](../../docs/design/stm32g474.md).

## Private ownership

`system.c` supplies the pre-RAM CPU-only `SystemInit`, checked clock startup and
rollback, reserved TIM2 handler and the single time domain. The generated cold
assembly owns clock gates, pin admission, startup ordering and partial rollback.
It validates every selected pin before the first change and retains one
`uint16_t` original state per selected pin with validity/gate ownership until
complete Platform release. Driver GPIO stop establishes its declared idle;
Platform release subsequently restores the original input/analog lease. A
failed restore readback retains only unfinished leases for ordinary cleanup
retry. This storage is separate from any constructor's temporary local snapshot.
Before system stop, every peripheral and OS consumer must already have joined.
Clock changes and deep-sleep continuity are excluded from this fixed profile.
Reserved TIM2 admission checks trigger, slave and output controls both before
clock effects and after enabling its gate. Startup holds TRGO on disabled CEN
while issuing UG, then restores reset selection before starting the counter.
Refused reset release or trigger restoration retains the timer for cleanup retry.

The GPIO owner supplies an exact port/mask, output direction and initial/inactive
levels. Latch readback precedes output mode publication. Partial initialization
keeps `retained` set until safe rollback succeeds. The hot BSRR write is a direct
hardware command, with no polling or hidden lock. Toggle is a read/modify/write
sequence and requires external serialization with all users of the same pins.

Each UART owner supplies separate static state, ring, baud and IRQ policy.
LPUART1 uses PA2/PA3 AF12, IRQ91 and PCLK170/PRESC16. USART1 PA9/PA10 AF7,
USART2 PA2/PA3 AF7, USART3 PB10/PB11 AF7, UART4 PC10/PC11 AF5 and UART5
PC12/PD2 AF5 use their own vectors, APB gates and PCLK170/PRESC1/OVER16.
All select 8N1, FIFO-disabled bytes and priority5. Only mutually nonconflicting
routes can be selected. The additional routes are silicon software fixtures,
not a statement about VCP bridges or inspected connector access.

TX admission borrows request/payload until actual TC and source withdrawal
precede SETTLED. Failed wire drain preserves QUARANTINED storage. Cold stop joins
captured notifications and proves UE/TEACK/REACK/NVIC withdrawal before releasing
ring/wake ownership. The IRQ moves at most one byte in each direction and records
hardware facts. Notification is a hint; predicates remain authoritative. No RX
event/block or DMA mode is advertised.

SPI1 PA5/PA6/PA7 and SPI2 PB13/PB14/PB15 select AF5, G4 DS=7, byte FIFO threshold
and actual 8-bit DR accesses. Each active-low endpoint owns an explicit GPIO CS,
mode0..3 and rate. At APB170, legal divisors yield 664063 Hz requested minimum
through 42.5 MHz maximum; APB/2=85 MHz exceeds DS12288 Table89 and is rejected.
Polling transfers are limited to 256 bytes and a finite absolute deadline, with
a bounded poll budget as a second termination bound. Both FIFO levels and BSY
must drain. Failures reset only the selected APB controller and require explicit
recovery. If an accepted polling call cannot prove reset/idle or inactive CS,
it retains the executor, buffers and controller/CS owners, masks IRQs and calls
the fault observer; it parks if the observer returns. Idle stop/recovery failures
retain retryable leases. There are no asynchronous submission or DMA methods.

PWM selects TIM3 CH1 PA6 AF2 or TIM4 CH1 PB6 AF2, each with exclusive ownership
of its whole timer. It is edge-aligned, upcounting and active-high, with a fixed
170 MHz/integer-divisor tick, divisor1..65536, period1..65535 and duty0..period.
CCR preload stages duty for an update event; period/base changes are rejected.
SPI/PWM use OSPEEDR11. Counter limits are logical: DS12288 Table56 at 30 pF gives
50 MHz pad toggling at 1.62..2.7 V and 120 MHz at 2.7..3.6 V. Period two at
170 MHz is 85 MHz only as counter arithmetic; actual supply/load/pulse
conditions require Board qualification and are not inferred from this profile.
Startup uses quiet UG and an inactive GPIO level. Stop proves output/counter
withdrawal and Board idle; failed private close retains cleanup responsibility.
Other channels, shared bases, complementary output, capture, break, IRQ and DMA
are outside this profile.

IWDG lookup and Platform startup never enable or feed the watchdog. Enable uses
the RM0440 non-window sequence and reports DS12288 Table45 full-temperature LSI
bounds of 29.5--34 kHz. Activation is irreversible, including later setup failure.
An automatically active watchdog with a pending register update reports unknown
timeout bounds. Failed preactivation LSI/debug cleanup retains private leases for
Platform stop retry. An active watchdog prevents complete Platform shutdown;
health, feed and reset/handoff policy belong to the external application.

EXTI `edge-event` selects PA0/EXTI0 (IRQ6) or PA1/EXTI1 (IRQ7), with independent
vectors and priorities. These are exact silicon routes in a software fixture;
they do not add NUCLEO button wiring or claim inspected connector access. The
caller explicitly supplies raw rising/falling/both edges and 1..4096 event
slots. A generated factory owns exactly that storage; no maximum pool exists.
Read has one task consumer. Each IRQ acknowledges its own W1C pending bit and
latches at most one timestamped, coalesced fact. Both-edge direction comes from
the sampled GPIO level, not a complete edge history. Full storage preserves
older facts, then exposes one loss boundary before any later accepted facts.
New edges can reassert pending immediately after acknowledgement; the handler
does not poll or misclassify that reassertion as a hardware fault.

Cold EXTI admission rejects live interrupt/event/trigger/pending state and the
selected live NVIC vector before global effects. Platform acquires the shared
SYSCFG gate once, preserves any preexisting gate, and holds GPIO leases through
complete stop. Each line preserves/restores its own four-bit mux field. Stop
attempts independent line-source and vector withdrawal; a refused readback keeps
event storage, wake target and mux recovery responsibility. An active wake
publisher must join before stop can release it. Kernel notifications validate
the actual vector priority against the generated policy. External bias, debounce,
edge-rate budget and safe input wiring remain Board/product responsibilities.

## Selected acquisition and physical Flash

The acquisition fixture selects PA0/ADC1_IN1, single-ended 12-bit raw counts,
SYSCLK/128 = 1.328125 MHz, bulb sampling and sample code0 (2.5 cycles). It owns
the ADC12 common clock and explicit SYSCLK mux, rejects other ADC owners, waits
22 us for the regulator and 5 us after calibration. These delays preserve the
20 us and four-ADC-cycle minimums after 1 MHz time quantization. ES0430
2.7.8--10 requires keeping the mux selected and discarding the first conversion
after idle or software STOP. Each enable and conversion clears, barriers and
reads back its own completion flags; stale ADRDY/EOC cannot become new data.
Each shot performs two conversions separated by a
3 us wait and publishes only the second. The masked prime/arm phase has one
absolute 100 us deadline. Sampling is disabled before returning or waiting for
another block. Failed drain retains the calling frame or producer loan.
Declared reference voltage is nominal; raw counts do not establish accuracy.

Finite DMA additionally owns TIM6, DMA1 channel1 and DMAMUX1 channel0 request5.
Each block is primed before arming. Exact rates 2.5--50 kHz must divide the 1 MHz
timer tick. RM0440 bulb sampling ends on the next trigger, so SMPR alone does not
bound its aperture. DS12288 Table66 limits it to 2.5--640.5 ADC cycles: at the
selected clock, the 20--400 us periods minus 12.5 conversion cycles give
14.0625--518.75 sampling cycles. After UG, CNT is set to period minus ten; the
first trigger follows ten timer ticks after CEN, with the same 100 us deadline
checked before and after enabling CEN. The common SYSCLK source fixes these
ratios. Host models cannot establish analog timing or physical clock accuracy.

Halfword normal DMA never overwrites a consumer loan. IRQ stops the trigger;
task service observes DMA/ADC withdrawal and disabled ADEN before publishing.
The next service explicitly rearms an available block; backpressure leaves the
sampler disabled. READY/BORROWED blocks still require consumer release. Close
restores and reads back all owned timer, DMA and mux configuration before
releasing the lease; refused restoration retains it for retry. The mode has
trigger gaps and no hardware aperture timestamp. ES0430 2.3.1 requires specific
TC/HT/TE acknowledgement, never global GIF clear on an active lane.

The selected linker and DMA domains use 96 KiB SRAM1/2. CCM32 KiB is counted
once at 0x10000000; silicon also permits DMA through its 0x20018000 alias
(RM0440 p89). Neither CCM address is selected by this software policy.

I2C1 selects PB8/PB9 AF4 open drain with external pull-ups, HSI16 kernel,
TIMINGR `0x30420f13`, analog filter and DNF0. The reviewed electrical envelope is
standard mode, rise at most 1000 ns and fall at most 300 ns. The finite master
supports one to eight 7-bit messages, each one to 256 bytes, repeated START and
one final STOP. A 256-byte message uses 255+1 RELOAD chunks. STOPF and cleared
BUSY precede return. NACK/arbitration/error aborts reset the local controller,
then require explicit recovery; this does not recover a physically stuck bus.
ES0430 2.15.2 master-only spurious BERR is acknowledged without abort. No IRQ,
DMA, slave mode, hidden retry or GPIO clock pulses are selected.

The Flash face exposes all 512 KiB, two banks of 128 pages, 2 KiB erase pages
and 8-byte ECC programming units. DBANK=1, BFB2=0 and observed FB_MODE=0 are
required; the SYSCFG clock remains owned and observed before reading the live
mapping. Remapped banks and foreign Flash control modes are rejected before
unlock, data or erase effects. Page128 selects bank2/page0. There is no product
reservation. The fixed software reference assumes 3300 mV; the Board must
confirm programming voltage. Geometry explicitly advertises fresh-erase
provenance: each written ECC line must come from a real erase, which byte
readback cannot prove. Each all-ones input unit skips the physical pulse so it
does not consume an erased ECC line. Program and erase wait for any live pulse
after an elapsed deadline and relock after idle. Each pulse clears and reads
back its own W1C status, checks the prepared control mode, and requires a fresh
EOP after BSY withdrawal; a preceding page cannot provide its completion.
RM0440 Category3 Table16 and FLASH_SR.Bit0 require EOPIE for EOP to latch.
EOPIE/ERRIE are explicitly enabled only for the owned pulse while exclusively
claimed FLASH_IRQn4 remains disabled in NVIC. Cold/live enabled, pending, active
IRQ or ECCIE owners are rejected. Cleanup observes source withdrawal, clears
status, then clears NVIC pending and observes disabled, not-pending and
not-active before release without changing its priority. Failed cleanup retains
a retryable lease.
ES0430 2.2.2 requires disabling
data cache before pulses and resetting it before restoring the original policy.
Products own explicit regions, watchdog, execution stalls and power-loss
recovery. Option writes, fast programming and mass erase are excluded.

Failed relock or cache restoration retains a private cleanup lease and rejects
further reads/writes. Platform stop retries the actual restoration before
releasing that state.

## Finite coupled PWM and ADC reference

The separate `pwm_adc` / `finite-regular-dma` software assembly selects one
TIM1/ADC1 owner and one `nx_pwm_adc_t` face. It keeps TIM1 CH1/2/3 PA8/PA9/PA10
AF6 duties constant for a finite request, uses internal CH4 PWM2/TRGO2 at a
50-tick phase in a 100-tick period, and samples PA0/ADC1_IN1 through DMA1
channel1/DMAMUX1 request5. The counter is 1 MHz from the 170 MHz kernel; deadtime
is separately 170 kernel ticks. COMP1 PA1/PA4 supplies the selected noninverted
break input. This software-only Board does not establish analog wiring, a power
stage or physical safety qualification.

The owner disables ADC while idle and discards two software primes before each
finite halfword DMA window. ADC limited requests with DMACFG0 prevent buffer
overwrite after TC, but do not stop PWM. Task service proves CEN0 before changing
the trigger route, then ADC stop/disable, DMA/address detachment and both vector
joins before SETTLED. Captured and late TE/OVR/break errors remain authoritative;
failed drain retains the request and outer leases. Public stop closes admission;
final Platform close additionally restores priorities, comparator, pads and
clocks. No independently startable ADC or group face aliases the owner. Exact
format, finite deadline observation, failure and software-test boundaries are in
[the coupled acquisition contract](../../docs/design/pwm-adc.md). HIL remains
`not_executed`.

## SDK and evidence

The official CMSIS Device G4 source is pinned to
`626ee412334a5ed2e5b320af5a8d77d69f03a558`, with its original Apache-2.0 LICENSE.md
and ST source notices preserved. ADC/DMAMUX inline primitives use the private
STM32CubeG4 v1.6.3 paired driver `f5929f431f9effe45fbe18f5337e4753ced9ac92`
with its unchanged BSD-3-Clause notice and exact selected-file lock. Public
product interfaces do not expose SDK
register types. The SDK is private to selected implementation targets.

Developer model logs, strict C11 ARM commands, actual official startup ELF
section/symbol outputs and resource comparisons are kept as execution evidence.
They establish software behavior and linking only. Clean configured platform
builds, reproducibility, OS composition and physical HIL have separate exits.
Unsupported peripheral modes fail configuration rather than selecting polling,
other clocks, alternate controllers or unreviewed PCB routes.

## 中文范围说明

本 provider 仅接入精确的 STM32G474RET6 软件参考：固定 170 MHz 时钟、独立 TIM2
时间域、封装有效 GPIO、六个 IRQ 字节 UART、SPI1/2 有限 polling、TIM3/4 CH1
固定 PWM、独立 IWDG 和 PA0/PA1 EXTI。PF0/PF1 保留给 HSE，PG10 保留给 NRST。
GPIO 焊盘清单不建立 PCB 接线。新增独立软件夹具接入 ADC1 PA0/IN1 单次采样、
TIM6 触发的有限普通 DMA block、I2C1 PB8/PB9 polling 及完整物理 Flash；
未列模式仍拒绝，USB/以太网未接入。

FDCAN1 新增精确 STM32G474RET6 的 ISO-FD 正常/内部环回软件装配，
500 kbit/s / 2 Mbit/s、固定 212 word 物理消息 RAM 与 3 个硬件优先级发送槽。
详见 [CAN FD 所有权合同](../../docs/design/can-fd.md)；收发器接线、物理时序与
HIL 仍未验证。

ADC 为独立单端 12 位、SYSCLK/128=1.328125 MHz、code0（2.5 周期）bulb 采样。
稳压器延迟 22 us，校准后延迟 5 us；考虑 1 MHz 计时量化后，仍满足 20 us 和
四个 ADC 周期的最小等待。每次丢弃首次转换，两次软件转换间等待 3 us，只返回
第二次。预热至 CEN 前后使用同一个 100 us 绝对 deadline。返回、背压或发布 block
前必须禁用 ADEN，下次启用重新预热，不能让 bulb 采样无限延续。

有限 DMA 使用 DMA1 channel1、DMAMUX request5、TIM6，速率仅允许精确整除
1 MHz 的 2.5--50 kHz。bulb 孔径由前次 EOC 到下次触发决定；DS12288 Table66
要求 2.5--640.5 ADC 周期，当前 20--400 us 触发间隔扣除 12.5 周期转换后得到
14.0625--518.75 周期。UG 后 CNT=period−10，首次触发位于 CEN 后十个 tick。
IRQ 停止触发，任务观察 DMA/ADC 撤离及 ADEN 清零后发布；下一次 service 才重装，
存在间隙。close 读回冷态配置，失败保留责任并允许重试；消费者仍负责释放借用。
仅选择 96 KiB SRAM1/2；32 KiB CCM 在 0x10000000 统计一次，芯片也允许通过
0x20018000 别名进行 DMA（RM0440 p89），当前软件策略未开放这些 CCM 地址。

独立 `pwm_adc` / `finite-regular-dma` 软件装配用一个 `nx_pwm_adc_t` 接口统一拥有
TIM1/ADC1。TIM1 CH1/2/3 的 PA8/PA9/PA10 AF6 占空比在有限请求期间保持不变；
170 MHz 内核分频得到 1 MHz 计数，period100、内部 CH4 PWM2/TRGO2 phase50，
deadtime 独立使用 170 个内核时钟 tick。PA0/ADC1_IN1 通过 DMA1 channel1、
DMAMUX1 request5 进行半字普通 DMA；COMP1 PA1/PA4 提供选定的非反相 break。
该软件 Board 不建立实际模拟接线、功率级或安全资格。

每次启用 ADC 后先丢弃两次软件转换，空闲时禁用 ADC。DMACFG0 的有限请求避免
TC 后覆写缓冲区，但不会停止 PWM。任务 service 先证明 CEN0，再改变触发路由，
随后观察 ADC 停止/禁用、DMA 与地址撤离以及两个向量退出，才发布 SETTLED。
排空中已捕获和迟到的 TE/OVR/break 不得被完成状态覆盖；失败时保留请求及外层
租约。公共 stop 关闭新准入，最终 Platform close 还需恢复优先级、比较器、引脚
和时钟。不会另暴露可独立启动的 ADC/group 接口。精确格式、有限 deadline 的观察
规则、失败及软件测试范围见[联合采样合同](../../docs/design/pwm-adc.md)；HIL 仍为
`not_executed`。

I2C1 为 PB8/PB9 AF4 开漏、HSI16、100 kHz 标准模式的固定 timing，外部上拉与
上升/下降时间由 Board 验证。每次最多八条消息，每条 1--256 字节；支持 repeated
START，最终 STOPF 与 BUSY 清零后才返回。错误要求显式 recover，不会脉冲 GPIO
或自动重试。Flash 为 DBANK1、512 KiB、两 bank、256 个 2 KiB page 和 8 字节
ECC 编程单位；要求 DBANK1、BFB2=0、FB_MODE=0，先持有并观察 SYSCFG 时钟再读
映射，操作时重新校验；外来 Flash CR 模式在任何写入前拒绝。无产品预留，3300 mV
参考假设必须确认。脉冲结束前不因 deadline
返回；每个脉冲先清理并读回自己的 W1C 状态和控制模式，BSY 撤离后必须观察新 EOP，
不能继承上页的完成标志。Category3 的 EOP 仅在 EOPIE=1 才锁存；脉冲显式启用
EOPIE/ERRIE，独占 FLASH_IRQn4，但始终不启用 NVIC。外来 enabled/pending/active
或 ECCIE 均拒绝；清理先观察中断源撤离，再清状态、清除 NVIC pending，
确认向量已禁用、无 pending 且无 active 后再释放，保持原优先级；失败保留可重试责任。操作前关闭 Flash 数据 cache，操作后复位并恢复。geometry 显式声明新擦除要求；全 FF 输入单位不产生物理脉冲，字节 FF 检查不能证明
ECC 从未写入，产品负责真实擦除来源、
Flash 区域、看门狗、执行停顿及掉电策略。所有新增模式的 HIL 仍未执行。
重新加锁或 cache 恢复失败时保留私有清理责任，拒绝后续读写；Platform stop
观察到实际恢复成功之后才释放该状态。

SPI 每次最多 256 字节、使用有限 deadline，最高 42.5 MHz；无法证明已接受调用的
移位器排空或 CS 无效时，保留执行者与借用存储，屏蔽中断并观察故障，观察者返回后
停驻。普通 `fixed-pwm` 模式仅为 TIM3 CH1 PA6 AF2、TIM4 CH1 PB6 AF2，独占整个控制器，不支持
动态 base、其他 channel、互补、capture、break、IRQ 或 DMA。IWDG 激活不可逆；
准备清理失败保留重试责任。所有 HIL 均为 `not_executed`。
SPI/PWM 选择 OSPEEDR11。PWM 计数范围仅代表逻辑能力：DS12288 Table56 在 30 pF
下给出低电压区 50 MHz、高电压区 120 MHz 的条件上限；period 二的逻辑 85 MHz
不表示已证明引脚频率，供电、负载及脉宽必须由 Board 验证。

完整职责、失败路径、来源与验收条件见[双语设计正文](../../docs/design/stm32g474.md)
及[主流接入合同](../../docs/design/mainstream-mcu-adaptation.md)。主机模型和 ARM
链接不替代实板电气、时序与性能测量。
