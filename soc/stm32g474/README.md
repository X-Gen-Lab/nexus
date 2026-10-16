# STM32G474 provider

This provider implements an exact STM32G474RET6 software reference: Cortex-M4F
startup, one fixed 170 MHz clock transaction, independent TIM2 time, statically
selected GPIO banks, six IRQ-byte UART controllers, SPI1/SPI2 polling endpoints,
TIM3/TIM4 CH1 fixed-base PWM, independent IWDG and PA0/PA1 EXTI events. Public
typed faces share readonly
operations tables and keep mutable state private. There is no allocator, worker,
runtime registry or automatic IRQ/DMA/polling fallback.

The silicon route inventory covers the 52 package GPIO pads. The fixed profile
reserves PF0/PF1 for HSE and PG10 for NRST until an explicit reviewed option-byte
policy exists. Pad inventory does not authorize a PCB connection. ADC, I2C,
Flash programming, DMA, FDCAN, USB and Ethernet remain unsupported here.

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

## SDK and evidence

The official CMSIS Device G4 source is pinned to
`626ee412334a5ed2e5b320af5a8d77d69f03a558`, with its original Apache-2.0 LICENSE.md
and ST source notices preserved. Public product interfaces do not expose SDK
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
GPIO 焊盘清单不建立 PCB 接线；ADC、I2C、Flash 编程及 DMA 等未列模式均拒绝。

SPI 每次最多 256 字节、使用有限 deadline，最高 42.5 MHz；无法证明已接受调用的
移位器排空或 CS 无效时，保留执行者与借用存储，屏蔽中断并观察故障，观察者返回后
停驻。PWM 仅为 TIM3 CH1 PA6 AF2、TIM4 CH1 PB6 AF2，独占整个控制器，不支持
动态 base、其他 channel、互补、capture、break、IRQ 或 DMA。IWDG 激活不可逆；
准备清理失败保留重试责任。所有 HIL 均为 `not_executed`。
SPI/PWM 选择 OSPEEDR11。PWM 计数范围仅代表逻辑能力：DS12288 Table56 在 30 pF
下给出低电压区 50 MHz、高电压区 120 MHz 的条件上限；period 二的逻辑 85 MHz
不表示已证明引脚频率，供电、负载及脉宽必须由 Board 验证。

完整职责、失败路径、来源与验收条件见[双语设计正文](../../docs/design/stm32g474.md)
及[主流接入合同](../../docs/design/mainstream-mcu-adaptation.md)。主机模型和 ARM
链接不替代实板电气、时序与性能测量。
