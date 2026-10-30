# STM32H743 software reference

Cold hardware behavior is maintained in C. The family configuration package
emits static policy, exact instance storage and typed calls; `platform_cold.c`
owns selected resource admission and shared GPIO/clock withdrawal. Provider
prepare/close helpers own their mode-specific lifetime, including the fixed-run
pause/resume resource path. Empty receipts cannot stop an unacquired live owner;
failed restore retains its original state for retry. These implementation
boundaries do not extend the clock, Cache, DMA or physical support listed below.

This provider maintains exact STM32H743ZIT6 software references. The original
NUCLEO-H743ZI2 GPIO/UART slice uses
official Cortex-M7 startup, MMIO-only bounded LDO power entry, fixed HSI32 or
HSI64 with explicit Cache off/on, TIM2 1 MHz time, authorized GPIOB PB0 and
USART3 PD8/PD9 AF7 IRQ bytes, finite DMA transmission with IRQ-byte reception,
or finite DMA receive blocks with independent IRQ transmission. A separate
`stm32h743zi_hsi_reference` package declares the selected reusable peripheral
wiring below without asserting that a physical PCB or attached device exists.
Public C11 GPIO/UART faces remain immutable; each static instance owns its
private state and caller-explicit byte ring. Shared readonly ops perform hot
I/O directly. No allocator, worker, registry or DMA fallback is introduced.

## Optional V-only high-clock reference

The additional `hsi64-pll384-cache-off` profile is a cold-start configuration
for DEV_ID `0x450`, REV_ID `0x2003` (V) only. Revision `0x1003` cannot distinguish
Y from W; `0x2001` (X) and unknown revisions are rejected without fallback.
Read the [high-clock contract](../../../docs/design/h743-high-clock.md) before
selecting it. It is not runtime frequency scaling or support for every H743 cut.

Nominal CPU/HCLK/APB frequencies are 384/192/96 MHz; TIM2 and TIM3 use 192 MHz,
not the CPU clock. USART3 and I2C1 select the unmultiplied HSI64 kernel. SPI4 uses
APB2 with a reviewed 99.45 MHz upper bound for divider selection. ADC12 receives
HSI64 through per_ck; only an initialized ADC1 provider selects async /4 for
nominal 16 MHz conversions. The empty profile does not configure ADC conversions.

The reviewed 62..65 MHz HSI design envelope derives from DS12110 Rev11 V-cut
factory, voltage and full-junction-temperature limits; it is not a measurement.
The PLL budget adds the RM0433 Rev9 PLL-ready deviation. A requested 115200 baud
with BRR556 can consequently span about 111511..116906 baud. This local divider
contract does not qualify a remote receiver, line or physical baud tolerance.
The detailed contract records the source conditions and calculation.

One selected four-byte clock responsibility record owns power/Flash protection,
PLL, buses and kernel selectors. Startup proves VOS1 and Flash 2WS/WRHIGHFREQ2
before increasing clocks. Stop first joins devices and withdraws the TIM2 System
owner, then proves actual HSI selection before reducing protections. Failed
withdrawal retains its effects for retry. The high profile uses this joint owner
instead of the fixed-HSI optional Flash-timing owner. Flash still requires a
fresh erase and the original geometry, voltage, alignment and pulse-drain rules.

The existing four fixed-HSI selections do not select the high owner or clock
translation unit. Four bytes describes the selected responsibility type, not a
measured whole-image RAM delta. Default-path machine-code and linked cost remain
separate checks. Cache-on, bootloader adoption and running-clock changes are
outside this high profile.

This iteration executed 38 clock, 19 high-System, 30 kernel and 10 normally
generated factory GoogleTest cases successfully. Three normal ARM presets also
configured and linked: `fixture-io-evolution-h743-high-empty`,
`fixture-io-evolution-h743-high-uart` and
`fixture-io-evolution-h743-high-peripherals`. These are development software
results; physical HIL remains `not_executed`. See the linked contract for exact
fixtures, sources and failure boundaries.

## Reset and ownership

Official reset calls `ExitRun0Mode` before `SystemInit`, DATA/BSS initialization
and constructors. RM0433 Rev9 section 6.4.1 forbids every RAM write during
Run-star until ACTVOSRDY. The power entry uses only scratch registers and PWR
MMIO until the supply is ready; it never pushes, spills, calls another function
or touches main RAM. It
checks LDO/BYPASS readback and polls at most one million observations. A refusal
masks IRQs and retains reason 1 (ready timeout) or 2 (supply readback failure) in
r0 before a terminal loop. Poll counts do not promise measured wall-clock time.
Production C asserts the exact assembly addresses/bits against the fixed SDK.

Before returning to any C stack use, the entry reads `DBGMCU.IDCODE.REV_ID`.
For revisions below `0x2000`, ES0392 section 2.2.10 requires
`GPV.AXI_TARG7_FN_MOD.READ_ISS_OVERRIDE=1`. The entry preserves other bits,
performs DSB and verifies the write; refusal parks with reason 3. Newer revisions
skip this write. This predicate follows the exact pinned ST system template
`8f922cdc7cc6de2344e75ddd657889f4ff761790`, not an inferred silicon revision.
The ARM gate checks the complete 140-byte leaf, both conditional paths, MMIO
addresses, barriers and fault exits. These are static machine-code checks;
old-cut SRAM reliability still requires physical qualification.

在任何 C 栈访问前，启动入口读取芯片修订号。小于 `0x2000` 的旧修订按
ES0392 第 2.2.10 节设置 AXI SRAM 目标的 `READ_ISS_OVERRIDE`，保留其他位，
执行屏障并检查读回；失败以原因 3 停驻。新修订跳过写入。该判断遵循锁定的
ST 官方系统模板；完整 140 字节 ARM 指令和分支验收不能替代旧硅片的实板验证。

`SystemInit` uses only CPU/system registers after power entry permits stack
writes. It sets the explicit FPU/vector/grouping ABI without initialized globals.
The fixed-HSI runtime starts from cold HSI64, undivided buses, ready LDO, conservative
reset Flash latency 7WS and Cache off. It rejects unknown running states before
mutation. Trusted reset or a prior successful owned clean/disable must establish
known-clean caches; clear CCR bits alone cannot prove foreign dirty lines are
absent. No running-clock, bootloader, cache, scheduler or alternate supply
state is silently adopted. Runtime owns reserved TIM2/IRQ; failure preserves
cleanup responsibility for retry. HSI/LDO/Flash are boot prerequisites and are
not withdrawn when runtime stops.

TIM2 cold admission checks CR1, CR2, SMCR, DIER, CCER and both CCMR registers:
an inactive CEN alone cannot exclude foreign trigger or pin/capture ownership.
Because a disabled peripheral gate can hide retained registers, startup first
takes only the reversible gate and repeats this observation before reset, UG,
divider or cache changes. A failed second admission restores only that gate;
it never clears the foreign timer, reset, flags or vector. Refused gate rollback
retains a distinct gate responsibility for retry without acquiring the timer.

The fixed-HSI clock profile selects `hsi32-fixed-cache-off`,
`hsi32-fixed-cache-on`, `hsi64-fixed-cache-off` or `hsi64-fixed-cache-on`.
HSI32 changes HSIDIV only after cold admission and confirms HSIRDY, HSIDIV and
HSIDIVF before publishing frequency or enabling TIM2. UART uses the same selected
APB frequency; TIM2 uses PSC31 or PSC63. Stop withdraws the timer before restoring
the owned divider. Refused divider or source writes retain cleanup responsibility.
This is a fixed cold configuration, not runtime frequency scaling.

Cache-on owns both reviewed M7 caches through a caller-owned Arch control object.
Providers must drain before cache withdrawal; a failed provider stop prevents
cache/timebase shutdown. Failed cache or selector restoration retains the control
object and prevents divider restoration. Cache-off removes the control field and
cold cache calls at compile time from the same resolved profile. The observed
ARM system object remains 48 bytes with Cache off and becomes 56 bytes with it on;
field placement lets the 12-byte control reuse existing padding. These are static
object sizes, not measured cache latency or physical coherency evidence.

Prescaler update starts with an observed W0C clear of stale UIF. Only a new UIF
after UG, followed by another observed clear, can publish the time domain.
Reset MMS exports UG on TRGO, so the owner first observes MMS=1 with CEN=0.
After UG/UIF are consumed it restores and observes MMS=0 before starting CEN.
A refused restoration retains only the explicitly owned quiet-MMS responsibility
alongside the timer; stop cannot release its gate until that state is cleared.
Overflow IRQs count only acknowledged flags. ACK failure
invalidates the live epoch, records `timer_error`, retains TIM2 responsibility
and attempts finite DIER/CEN/NVIC withdrawal. The lost deadline domain is always
terminal, including successful source isolation: the CPU masks IRQs, calls the
existing bounded fault observer and parks if it returns. No invalid zero time
resumes a deadline consumer or releases an I/O loan. Normal stop/restart is not
recovery from this terminal path; products own separately reviewed non-returning
recovery. A clock
snapshot checks epoch validity under the same saved mask as both counter words.
Zero alone does not prove readiness, and old deadlines cannot be reused after
invalidation. This supplies neither a generic public health manager nor a
product recovery policy.

Generated assembly enables all selected GPIO banks and validates every selected
pin as unlocked input/analog before capturing any fields/latches. A foreign pin
behind a disabled gate is rejected without pin writes or an unauthorized lease.
The assembly captures only selected GPIO fields/latches and added clock gates.
Partial initialization includes the failing instance in reverse cleanup.
GPIO preloads and observes safe levels before publishing output mode. Ordinary
IRQ-byte UART uses
8N1, PRESC1 and the selected PCLK, no FIFO, actual TEACK/REACK and IRQ readback.
In IRQ-byte mode its bounded ISR moves at most one TX and one RX byte.
Requests settle only after actual wire
TC and withdrawal of retained sources. Failed drain preserves QUARANTINED
borrowed storage. Stop joins notification publishers, withdraws UE/ACK/IRQ and
returns ring/wake responsibility only on success. External consumers must join
before platform stop.

## Finite UART DMA

`dma-tx` selects USART3 TX through DMA1 Stream0, DMAMUX1 Channel0 request46;
its vector is independent of the USART3 vector. RX remains an explicit bounded
IRQ-byte ring. This TX selection does not enable receive DMA, circular mode,
a stream pool or a fallback.
Cold admission rejects an already owned lane, request mux or vector. Generated
assembly records only clock gates it added and retains them while cleanup fails.

Only selected initialized AXI SRAM and explicitly budgeted D2 SRAM1 are admitted
for TX buffers. TCM, Flash,
unpowered/unselected D2/D3 memory and arithmetic overflow are rejected before
borrowing. Cache-off accepts 1..65535 bytes. Cache-on requires an aligned address
and a positive multiple of 32 bytes, up to 65504; the caller exclusively owns
every complete line. No outward rounding or hidden bounce buffer occurs.

Admission borrows request and storage before cache cleaning. A PREPARING state
lets cleaning run with the caller's normal interrupt mask while cancellation
cannot settle or reclaim the loan. The provider rechecks cancellation/deadline
before starting DMA. Cache policy is stable throughout the platform lifetime.
DMA completion is separate from UART wire TC. DMAT, stream EN, transfer sources
and retained references must withdraw before SETTLED; a refusal quarantines the
loan for retry. Late DMA errors take precedence over success. Failed transfers
report zero bytes rather than infer wire progress from NDTR.

`rx-dma-idle` instead selects USART3 RX through DMA1 Stream1, DMAMUX1 Channel1
request45 and its exact vector. TX reuses the ordinary IRQ provider. The assembly
creates no byte ring: the caller initializes and supplies an `nx_stream_t` and
its exact block array through `rx_start`. Every block is validated before the
first loan against selected initialized AXI/SRAM1 DMA-write regions, provider
state, hardware registers, region descriptors, stream/slot metadata, wake and
active TX storage. Cache-off accepts 1..65535 bytes; Cache-on requires aligned,
exclusive whole 32-byte lines, up to 65504 bytes. No range rounding occurs.

Normal-mode DMA writes one producer loan. Full completion or USART IDLE causes
bounded source withdrawal. RM0433 section 15.3.16 permits TCIF with nonzero NDTR
after software interruption: only after EN and DMAR read back disabled does the
provider use the stable remaining count for a valid IDLE prefix. The USART IRQ
never reads RDR while this DMA selection owns reception. DMA/UART errors discard
the affected loan after proven drain and record one loss boundary rather than
publish uncertain bytes. Acknowledgements clear only the selected stream flags.

With Cache on, complete owned lines are invalidated before DMA and again after
drain, before producer publication. Both operations run in owner task context
outside the IRQ mask; PREPARING/PROCESSING retain the producer loan throughout
maintenance. A refused EN/DMAR/source withdrawal or cache operation keeps the
loan and marks the private state quarantined for explicit retry. Neither a TC
flag nor cancellation revokes the loan. Stop retains stream and platform clock,
cache and time ownership while a producer, READY block or consumer loan remains.
After all loans end, a successful RX stop permits a new explicitly initialized
stream without restarting the controller.

The owner calls service to publish and arm the next free block. Restart gaps
are possible, and borrowed blocks are never overwritten. This is neither
circular DMA nor continuous no-gap reception; no byte-exact loss count is
inferred. This finite selection does not inherit the separate double-buffer
mode below. Additional H7 DMA engines and physical cache coherence remain
separate acceptance scopes.

## Optional continuous DMA receive

`h743-uart-continuous.toml` and `h743-uart-continuous-cache-on.toml` select
`rx-dma-continuous`: exact STM32H743ZIT6 V, fixed HSI64/default trim,
baremetal, USART3 PD8/PD9, DMA1 Stream1/request45 and priority5 IRQs that do
not call an OS. TX remains independent finite IRQ transmission. This mode
does not provide continuous TX DMA or a general DMA duplex provider.

The caller supplies at least three equal, nonoverlapping initialized AXI SRAM
blocks. Cache-off admits 32..65535 bytes per block; Cache-on requires aligned,
exclusive whole32-byte lines, 32..65504 bytes. Two posted producer loans remain
FILLING behind M0AR/M1AR. A completed target is replaced with a FREE third loan
without disabling DBM/CIRC. Service proves the inactive address detached before
post-DMA invalidation and FIFO publication. The IRQ only latches completion and
masks TC until service; it never performs long cache work or publishes payload.
The [posted-loan diagram](../../../docs/assets/diagrams/continuous-stream.svg)
illustrates the shared principle using its explicitly labeled F407 reference;
H743 additionally owns the cache and correlated deadline checks described here.

A sticky TC and one-bit CT cannot count missed wraps. The provider therefore
checks an absolute replacement deadline before and after target replacement.
USART PRESC1/OVER16/8N1 and TIM2 PSC63 use the same HSI clock: actual BRR,
not a nominal baud or host clock, establishes the ratio. Eight BRR clocks per
byte conservatively replace the ideal ten-bit frame assumption. The bound
subtracts20 bytes for the16-byte FIFO, RDR, shift and direct-beat boundary,
rounded upward, plus two timer ticks; FIFO bytes already counted by NDTR are
intentionally deducted again. TIM2 internal/upcounting mode, ARR, DIER,
overflow vector, clock/reset and UART/cache configuration are checked.
This is a fail-closed software admission bound, not a measured service latency.

No FREE replacement stops production and records one loss boundary without
overwriting READY/held buffers. A late or uncertain bank history discards the
unproved producer bytes. IDLE closes the current producer and publishes only a
proved drained prefix. Both require an explicit caller restart after successful
RX stop, consumer join and stream reinitialization; there is no automatic rearm.
Actual EN readback, rather than the disable write or an old TC, establishes
memory drain under RM0433 Rev9 section15.3.19. Cache operations run outside the
IRQ mask; preparation/processing guards retain every loan through recursive
control calls and failed maintenance. Observed clock, timer, reset or cache
violations permanently taint the retained lifetime even if registers return.
Software models, ordinary ARM links and SDK consumption establish separate
development evidence; physical throughput, cache coherence, latency and HIL
remain `not_executed`.

## Selected continuous RX and finite TX DMA duplex

The separate `dma-duplex-continuous` mode is authored by
[`h743-uart-duplex-continuous.toml`](../../../tools/configure/assemblies/h743-uart-duplex-continuous.toml)
and its
[`cache-on` counterpart](../../../tools/configure/assemblies/h743-uart-duplex-continuous-cache-on.toml).
It selects the exact V/HSI64/default-trim/baremetal reference above, USART3
PD8/PD9 AF7 and priority5 with `calls_os = false`. The existing
`rx-dma-continuous` mode keeps its independent IRQ transmitter.

| Selected lane | Engine | DMAMUX1 | Request | Vector |
| --- | --- | --- | --- | --- |
| Finite TX | DMA1 Stream0 | Channel0 | 46 | DMA1 Stream0 IRQ |
| Continuous RX | DMA1 Stream1 | Channel1 | 45 | DMA1 Stream1 IRQ |
| USART wire/IDLE/error | USART3 | — | — | USART3 IRQ |

One immutable public UART face owns one UART context, the existing RX owner
and an appended TX DMA extension. RX-only storage stays unchanged. Both
personalities share the RX loan algorithm; the existing `dma-tx` selection and this
duplex selection share the finite TX DMA algorithm. Temporary private call
views add no registry, persistent dispatch object, buffer pool or worker.
The typed factory returns only the existing static face without hardware work.

Both directions use selected initialized AXI SRAM at
`0x24000000..0x24080000`. TX borrows 1..65535 bytes with cache off; cache-on TX
borrows aligned complete exclusive32-byte lines, 32..65504 bytes. All TX payload
lines are disjoint from the entire live RX pool, stream, slots, request, owner
and wake storage. RX retains the two-target/spare, held-block protection,
replacement deadline and explicit loss-stop rules above. Products supply every
buffer and consumer; no copy, split-transfer or memory-domain fallback is used.

TX clean and RX maintenance run outside ISR and the saved interrupt mask.
Cancellation during maintenance retains its request. DMA terminal count alone
does not release TX: source/engine/IRQ withdrawal and actual final wire TC must
also hold. RX stop leaves the independent TX responsibility active. Platform
stop joins RX loans and TX memory/wire, then restores only the owned lane
registers/FIFO settings and UART divider to their cold values before releasing
gates and caches. Successful stop permits another normal Platform start.
A refused restoration retains the remaining owner and returns a failure for
retry. No shared DMA-domain reset is used.

Observed clock, timer, reset or cache loss permanently taints this retained
lifetime and isolates all three vectors. Later I/O register access, producer
publication and loan return stop; restoring register values does not authorize
recovery. Request and producer storage remain quarantined. Native/model,
normal ARM resources, SDK consumption and physical station results have separate
evidence. Physical duplex throughput, cache coherency and timing remain
`not_executed`; this selection does not extend to PLL384, RTOS backends or D2 RAM.

## Optional bounded hardware FIFO bytes

The separate `irq-fifo-bytes` selection uses USART3 PD8/PD9 AF7, 8N1, priority 5
and the exact `hsi64-fixed-cache-off` 64 MHz profile. It enables the hardware 16-byte RX
and TX FIFOs; RX can additionally hold one RDR byte. Explicit threshold and
per-entry RX/TX byte budgets are independent. An IDLE/threshold handler drains
at most each budget, uses a checked RX-not-empty fallback for a short remaining
tail, and preserves per-head error ordering. TX space/FIFO-empty never replaces
wire TC. Cancel/drain and whole stop retain storage until source, vector,
publisher and UE/ACK withdrawal is proved.

The selected state adds only a const-profile link to the existing UART prefix;
ordinary UART state/operations and FIFO-disabled DMA modes remain unchanged.
H743 high-clock profiles, DMA, non-8N1 and flow-control combinations are not
admitted by this mode. The shared compile-time algorithm reuses pinned official
LL USART leaves without a second hardware owner. See the
[FIFO selection and lifecycle contract](../../../docs/design/uart-fifo.md).
Its 45 real-provider and 9 normal-TOML/generated-factory model cases passed,
along with the selected normal ARM build, resource and retention checks.
Physical HIL is `not_executed`;
bounded register accesses are not measured latency or a throughput guarantee.

中文：可选 `irq-fifo-bytes` 仅接入上述 USART3、HSI64 缓存关闭配置和8N1，
硬件 FIFO 每方向16字节，RX 另有1字节 RDR。阈值与每次中断 RX/TX 字节预算独立；
短尾通过经读回确认的 RX 非空中断继续排空，错误按当前队首处理。FIFO 空或有空间
不等于线上 TC；取消和停止仍须证明所有来源、向量、发布者及 UE/ACK 已撤回，才能
归还存储。普通模式状态与接口不增长，不自动扩展到高频或 DMA。45项外部寄存器
模型和9项正常生成工厂用例，以及选定 ARM 链接/资源/保留检查已通过；这些结果
仍不代表实板测量，物理 HIL 为 `not_executed`。

## Memory and evidence

AXI SRAM512KiB is the default linker domain. Optional D2 SRAM1 is selected by
an explicit `[memory.regions.sram1]` byte budget. Reset enables and observes its
SRAM1 clock after the MMIO power entry permits RAM, then copies selected DATA
and clears selected BSS from checked linker bounds before primary DATA/BSS or
constructors. NOINIT remains caller-initialized. The boot-owned SRAM1 clock is
not withdrawn at Platform stop; every consumer must join before any external
power policy changes it. DMA admission rechecks this clock before borrowing.
`NX_MEMORY_SRAM1_DATA`, `NX_MEMORY_SRAM1_BSS` and `NX_MEMORY_SRAM1_NOINIT` place
only variable storage; selection does not authorize instruction execution.
DMA1 providers admit selected initialized AXI/SRAM1 only, with the same exact
exclusive-line policy when Cache is enabled. Other domains remain unselected. Physical ITCM/DTCM, D2/D3 and
backup SRAM facts preserve per-engine DMA1/DMA2/BDMA/MDMA reachability. MDMA can
reach TCM; no generic DMA boolean erases that distinction. Reachability does not
authorize placement, cached buffer ownership, power retention or execution.
Additional domains, DMA routes, dynamic PLL, deep STOP and bootloader
handoff have separate implementation and acceptance exits.

Official Apache-2.0 CMSIS Device H7 v1.10.7 is pinned to
`8f922cdc7cc6de2344e75ddd657889f4ff761790`; vendor types stay private. DS12110 Rev11,
RM0433 Rev9 and UM2407 Rev6 establish source facts. PC2_C/PC3_C remain conditional
physical pads, not exposed routes. Device group/density checks do not certify
package, PCB or silicon revision. Applicable ES0392 errata require actual silicon
revision and physical evidence.

GoogleTest/GoogleMock execute real production C against external register/IRQ
boundaries. Actual ARM ELF/disassembly checks establish code generation,
resources and the pre-RAM closure. Neither establishes physical power behavior,
oscillator accuracy, UART traffic or safety. All physical acceptance remains
`not_executed`; clean candidate, offline reproducibility and HIL are separate.


## Selected reusable peripherals

The software-only peripheral assembly is
[`peripherals.toml`](../../../tests/contracts/h743_assembly/peripherals.toml).
The32/64MHz HSI, Cache-off variants link the same seven new selected classes
with baremetal or FreeRTOS. UART and GPIO remain independently selectable.
Applications, Flash reservations, safe output policy and external devices remain
outside this platform repository. Modes not listed are rejected at configuration.

| Controller | Selected mode and exact boundary |
|---|---|
| SPI4 | PE12/13/14 AF5, explicit active-low PE11 CS endpoint,8-bit full-duplex polling1..256bytes, finite absolute deadline,<=16MHz requested SCK; default APB2 kernel selector must be observed |
| I2C1 | PB8/PB9 AF4 open drain, standard-mode ceiling100kHz, explicit7-bit endpoint, repeated START and bounded message chunks; final STOPF/BUSY drain or explicit reset; external pull-ups required |
| ADC1 | PA3 INP15,16-bit single conversion, sample code7(810.5cycles), synchronous HCLK/4, single-ended offset+linearity calibration; no trigger/DMA/injected acquisition |
| TIM3 CH1 | PA6 AF2, fixed PSC/ARR and caller-staged CCR1 preload;0/100percent supported, explicit Board inactive latch restored after timer withdrawal |
| EXTI0/1 | PA0/PA1 raw edge facts in caller-sized rings; bounded selected W1C, coalescing/loss visible; no debounce guarantee |
| IWDG1 | Explicit irreversible start/feed, no window mode, observed debug-freeze and latched reset facts; LSI timing envelope remains unknown(0/0), enabled-state queries return UNSUPPORTED while preserving enabled responsibility |
| FLASH | Full2MiB,16physical128KiB sectors, two1MiB banks,32-byte ECC flashwords, explicit erase-aligned consumer regions; no product partition |

### Public finite SPI4 DMA assemblies

The canonical `tools/configure/assemblies/h743-spi-dma.toml`,
`h743-spi-dma-uart-tx.toml` and `h743-spi-dma-uart-rx.toml` select Cache off;
`h743-spi-dma-cache-on.toml` selects Cache on. All use the same STM32H743ZIT6
software reference: baremetal, fixed HSI64/default trim, DEV_ID `0x450`,
REV_ID `0x2003` (V), SPI4 mode 0 and 8-bit frames.
PE12/PE13/PE14 use AF5; PE11 is the explicit active-low GPIO chip select.
These software Board routes do not establish PCB wiring or electrical timing.

Each request borrows non-overlapping TX/RX byte spans in selected initialized
AXI memory. Cache-off requests admit 1..65535 bytes; Cache-on requests require
32-byte-aligned addresses and lengths of 32..65504 bytes divisible by32. Every
TX/RX line is exclusively borrowed, including padding; the provider never rounds
into adjacent storage. Accepted requests clean TX and invalidate RX before DMA,
then invalidate RX again after memory, wire, IRQ and address withdrawal. Cache
maintenance runs with the incoming interrupt mask restored, while a preparation
guard prevents recursive service from releasing the loan. A refused final
invalidate keeps the request quarantined until a successful retry. Cache state
must remain fixed through successful stop; an observed change permanently
taints the retained owner. RX uses DMA1 Stream2, DMAMUX1
request83 and IRQ13; TX uses Stream3, request84 and IRQ14. SPI4 uses IRQ84.
The three handlers use priority5 with `calls_os = false`. The optional USART3
TX Stream0 or RX Stream1 selections use independent resources; the existing
shared DMA1 gate remains owned until all selected users release it.

The public typed factory returns the existing SPI bus and endpoint faces. The
same public C11 consumer is selected by the existing H743 CMake project and the
installed SDK plan; it does not expose private constructors or MMIO. Preparation,
submit, cancel, service and stop retain the existing borrow/drain contract:
DMA completion alone does not release the request. A refused withdrawal or
clock/state mismatch retains the caller storage and recovery responsibility.

This selection is finite full-duplex SPI, not a continuous UART duplex mode.
It does not admit high-clock profiles, other backends, 16-bit frames,
fixed dummy buffers or multisegment transfers. Configuration, model execution,
ARM linking, installed SDK consumption and physical HIL are separate evidence;
publishing these assemblies does not prove those executions. HIL remains
`not_executed`.

### SPI4 polling

SPI4 polling requires actual DEV_ID `0x450`, REV_ID `0x2003` (V) and
HSITRIM `0x40`, including fixed-HSI profiles. Non-V cold admission returns
`UNSUPPORTED` before reading the V-only HSICFGR layout; foreign V trim returns
`STATE`. Retained/active cold responsibility keeps its existing `BUSY` priority.
A later incompatible clock makes transfer/recover/stop return `STATE` while
retained cleanup remains owned. This SPI contract does not narrow generic
fixed-System or UART support.

Divider selection uses the reviewed source upper bounds, not nominal rates:
65 MHz for fixed HSI64, 32.5 MHz for fixed HSI32 and the existing 99.45 MHz
APB2 bound for PLL384. The V factory-trim/voltage/junction envelope comes from
DS12110 Rev11 Table 134; these are design bounds, not measured SCK. Thus a
16 MHz ceiling on HSI64 selects /8, and a 250 kHz ceiling is unsupported on
HSI64 but allowed on HSI32. The fixed-profile quiet guard retains the nominal
same-source timer/SPI ratio; it does not estimate this wait from the source
upper bound. A register match checks an existing exclusive lease, not authority
to share or take over hardware.

SPI v3 uses TSIZE, byte-width TXDR/RXDR and explicit CSTART. With nonzero
TSIZE, TXC follows EOT; it is not an independent observation of wire completion.
Stopping observes CSTART clear after EOT or an explicit CSUSP/SUSP handshake,
then waits at least one SCK period plus two timer quanta independently of the
caller deadline. The CSUSP read-modify-write masks the rs CSTART command;
otherwise writing back a sampled one could restart the master. Normal EOT can
win that race: CSTART clear with nonzero TSIZE and both EOT and TXC set accepts
an actual stop even without SUSP. After RX withdrawal and SPE disable, it verifies CS high. If PE12
still uses AF5, it latches the current CPOL and verifies its GPIO-output handoff; an
already-owned GPIO idle latch is preserved. Only then does it assert the
exclusive peripheral reset (ES0392 section2.22.6). The next transfer configures
CPOL while CS is high before restoring PE12 AF5. No MOSI
idle level or physical glitch-free behavior is inferred from these model checks.
CFG1 retains CRCSIZE7 even with CRC disabled (section2.22.5). A failed active
withdrawal parks with the caller frame and buffer loan retained, even if CS has
already gone high. RX reads count completed frames; unsent TX FIFO bytes flushed
after suspension do not. Generated assembly owns the GPIOE clock and selected
pins; a private manual constructor must hold the same exclusive lease.
I2C reuses the reviewed private V2 engine after independent H743 register/bit
layout assertions;
it never acquires a vendor HAL handle, tick, worker, IRQ or DMA owner. The fixed
TIMINGR values are0x70f22727(HSI64) and0x30f22727(HSI32), with analog filter on,
DNF0 and a standard-mode electrical envelope of rise<=1000ns/fall<=300ns.
They bound the nominal requested rate; real bus timing remains unmeasured.
I2C123SEL must remain the admitted APB1 selector; a foreign selector rejects
before private pin changes. ES0392 sections2.19.4/2.19.5 select master-only
spurious BERR clearing and NBYTES1 throughout receptions requiring RELOAD.
ARLO, NACK and overrun remain errors; TIMEOUTR must be cold-zero.

ADC boost selection preserves the official LL cut1/cut2 encoding difference.
The old-cut encoding is masked to its actual single BOOST bit, excluding LL
transport bits that are reserved/read-zero in old hardware. The selected8/16MHz
ADC clocks therefore leave old-cut boost off; newer cuts select their own range.
A nominal25us regulator guard and a two-tick guard after calibration precede
ADEN; the latter covers one timer quantum of phase and four ADC clock cycles.
Readback of ADRDY precedes interface publication. Calibration failure keeps
the ADC12 common-domain lease. ADC2 must be cold, and only ADC1's selected regular
conversion is admitted. Stop withdraws conversion/enable before restoring common
clock fields. Full calibration must expose all six LINCALRDYW flags; control
writes preserve these handshakes across enable, stop and restart. No factor
read/restore handshake is performed. Each deep-powerdown exit recalibrates.
The Board must explicitly declare actual VDDA in2400..3300mV and a reference
2000mV or higher and no greater than VDDA; this admits the conservative16-bit
mode across unqualified older cuts (ES0392 section2.9.10). Counts and nominal reference voltage are not
calibrated volts or a guaranteed effective resolution.

Flash writes require an explicit Board binding `supply_mv` in2700..3600 and the
Cache-off profile, unswapped banks, no active foreign mode and erased complete
flashwords. Input bytes are copied to eight local words; caller input alignment1
is independent of destination/program length alignment32. Each flashword is
subject to a caller-proved fresh erase before programming, because ECC bits
also participate. A visible all-FF scan only rejects visible prior writes; it
cannot prove virgin ECC cells across reset or foreign writers. An all-FF input
flashword is a no-op without controller writes or programming pulse. Bank flags,
program modes and locks
are checked. Pulse error is returned only after BSY/QW/WBNE withdraw. A stuck
pulse or refused final control/lock write retains the active caller and enters a
nonreturning fault path; timeout never aborts a pulse or releases storage early.

The current development path requires selected-only cold RAM preparation and
V revision `0x2003` admission before physical programming or erase. This applies
to every selected Flash profile, including fixed HSI32/64 and HSI64/PLL384;
unknown cuts are rejected. UART and devices without their own cut contract
do not inherit the Flash restriction; SPI has the separate V/trim contract
above. Assemblies without Flash acquire no RAM workspace. The generated
Platform owns one existing fixed workspace and its embedded guard; foreign
workspace construction rejects before effects. Public SDK consumers use the
immutable `nx_factory_flash` face and ordinary Platform start/stop, without
supplying a workspace or including private types. The assembly bridge installs
private MSP and protects incoming MPU captures in r8-r11; the C transaction owns
RAM VTOR publication, the MPU guard and actual both-bank drain. Its final
assembly gate checks restored hardware and linker-known original VTOR before
exact caller restoration. The selected link requires zero unclassified explicit
reads/stores and at most one exact eight-store programming leaf per transaction.
The incoming MSP and active Thread stack each require 260 owned bytes below the
bridge-entry SP, in addition to ordinary C frames; the private-stack bound does
not prove those incoming extents. There is no ordinary Flash pulse fallback.
The [RAM maintenance design](../../../docs/design/h743-flash-ram.md) gives the
C/ABI, external-owner and per-profile evidence obligations.

This synchronous maintenance mode masks configurable interrupts and provides no
tight interrupt-latency or wall-time deadline ceiling. Deadline expiry after
trigger only latches a result; it cannot cancel the pulse or authorize an early
return. The32-bit TIM2 timebase can recover one pending wrap, and consumers must
still respect its documented service interval. CPU MPU protection does not stop
other bus masters: the external maintenance caller must withdraw and join them.
Cache-on self-programming, bank swapping, option/OTP programming, ECC exception
recovery and product boot/update policy remain separate scopes. The reference's
3.3V is an authored software assumption; no voltage or analog path has been
measured. Source integration and a successful link do not establish completed
IO-24, software-candidate or physical qualification.

The selected private LL SPI/ADC/USART/I2C headers and original BSD-3-Clause
notice are paired with STM32CubeH7v1.13.0 by the reviewed source lock. Reuse
includes field encodings, TSIZE queries, selected USART operations, SPI polling
and DMA-cleanup byte ports, and I2C receive/transmit through a private compile-time
port. Public faces retain Nexus types; existing transfer, time, IRQ and shutdown
owners are unchanged. Retained command and lifecycle exceptions follow the
[official-driver contract](../../../docs/design/official-stm32-provider.md).

## 中文范围说明

私有官方 LL 依赖现涵盖 SPI/ADC/USART/I2C。SPI 轮询与 DMA 清理的字节访问、
I2C 收发叶操作使用选定实例；既有传输、时间、IRQ 和停止 owner 不变。
具体操作例外及资格边界见[官方驱动合同](../../../docs/design/official-stm32-provider.md)。

新增显式 `dma-duplex-continuous`，使用
`h743-uart-duplex-continuous.toml` 和 `h743-uart-duplex-continuous-cache-on.toml`。
精确范围是 STM32H743ZIT6 V、固定 HSI64／默认用户 trim（HSITRIM=0x40）、裸机、USART3 PD8／PD9
AF7、优先级5且 `calls_os = false`。原 `rx-dma-continuous` 保留 IRQ TX。
TX 使用 DMA1 Stream0／DMAMUX1 Channel0／request46；连续 RX 使用 Stream1／
Channel1／request45；USART3 另观察线上 TC、IDLE 和错误。三个向量属于同一个
静态 UART owner 和不可变公共 face，RX-only 状态不增加 TX 字段。RX 和有限 TX
算法分别共用，临时私有调用视图不保存注册表、持久转发对象、块池或 worker。

双向 payload 限于已选择初始化的 AXI SRAM `0x24000000..0x24080000`。
TX 缓存关闭时允许1..65535字节；开启时需要完整独占32字节行，地址对齐、长度
32..65504且为32的倍数。TX 与整个 RX pool、stream、slots、request、owner 和
wake 存储互不重叠，不能用 Flash 常量冒充 DMA payload。RX 沿用下文两个 target
和替换块、持有块不覆写、有界替换时限及丢失后停止合同。所有存储和 consumer
由产品提供；不会隐式复制、分段传输或退回其他内存域。

TX clean 和 RX 缓存维护在恢复中断掩码后执行，维护期间取消仍保留借用。
DMA TC 不能替代真实线上 TC；源、引擎和 IRQ 排空后才能归还 TX request。
RX stop 保留独立 TX 责任；Platform stop 排空两向及消费者，再恢复自己持有的
lane 寄存器／FIFO 设置和 UART 分频，最后释放时钟与缓存。成功 stop 后可正常
再次 start；恢复写入失败保留剩余 owner 供重试，不复位共享 DMA 域。
时钟、timer、复位或 cache 失效会永久 taint 本次保留生命周期并隔离三个向量，
停止后续 I/O MMIO、producer 发布和归还借用。恢复寄存器不能自动解除隔离。
模型、普通 ARM／SDK 消费与实板证据分开；双向吞吐、缓存一致性和时序实板状态
仍为 `not_executed`，不扩展 PLL384、RTOS 或 D2 RAM 范围。

新增 `hsi64-pll384-cache-off` 仅准入 DEV_ID `0x450`、REV_ID `0x2003`（V）。
`0x1003` 无法区分 Y/W，`0x2001`（X）及未知修订均拒绝，不自动回退。
该冷启动配置的名义 CPU/HCLK/APB 为 384/192/96 MHz，TIM2/TIM3 为 192 MHz；
USART3/I2C1 使用未倍频的 HSI64，SPI4 按 APB2 的 99.45 MHz 保守上界选分频。
ADC12 输入为 HSI64，只有初始化 ADC1 后才设置 async /4，得到名义 16 MHz。
62..65 MHz 是按官方出厂、电压及完整结温范围推导的设计包络，不是实测值；
115200 baud 的本地分频不能证明整条通信链路的容差。四字节选配责任记录统一
管理时钟、电压、Flash 时序和 kernel mux；所有设备及 System 停止后，才撤销
时钟责任，读回失败保留以便重试。现有四种固定 HSI 配置不选入该 owner。
本轮 38+19+30+10 个模型测试与三个正常 ARM 配置已通过；实板 HIL 仍为
`not_executed`。Cache-on、所有修订支持、运行时调频和整机资格不在本次结论内。
完整条件、公式、调用顺序和验证边界以[高频合同](../../../docs/design/h743-high-clock.md)为准。

本轮接入精确STM32H743ZIT6软件参考的SPI4、I2C1、ADC1、TIM3 CH1、EXTI0/1、
IWDG1与双Bank物理Flash。原NUCLEO GPIO/USART3参考保持独立；新增引脚组合位于
明确的软件参考Board包，不能据此推断实板接线、电压或外设已验证。typed factory只查找
静态face，热路径直接调用共享只读ops与独立context，不分配对象，不启动隐藏线程。

可选D2 SRAM1必须通过独立容量预算选择。复位阶段确认供电与时钟后，按链接边界复制
DATA并清零BSS，NOINIT由调用者初始化；DMA只准入已初始化的AXI/SRAM1，并在借用前
检查SRAM1时钟。Cache打开时仍要求独占完整32字节行。已有有限RX正常模式以owner
service发布和重启，背压期间不会覆盖已借出的block，重启间隙仍然存在，不能宣称无缝连续。

独立的 `rx-dma-continuous` 使用 `h743-uart-continuous.toml` 与
`h743-uart-continuous-cache-on.toml`，只准入 STM32H743ZIT6 V、固定 HSI64／
默认用户 trim（HSITRIM=0x40）、裸机、USART3 PD8／PD9、DMA1 Stream1／request45 及优先级5的非 OS
IRQ。TX 仍是独立有限 IRQ 发送，不是连续 TX DMA 或通用 DMA 双向实现。
调用者提供至少三个已初始化 AXI SRAM 中等长、互不重叠的 block；缓存关闭时每块
32..65535 字节，缓存开启时为完整独占32字节行，地址对齐、长度32..65504且为32
的倍数。两个 posted producer loan 始终以 FILLING 留在 M0AR／M1AR，service
用空闲第三块替换非活动地址，DBM／CIRC 保持运行。证明旧地址已脱离两个硬件
target 后，才进行 post-DMA invalidate 并按 FIFO 发布。IRQ 只记录完成事实并
屏蔽 TC，等待 service；不在 IRQ 中进行长缓存维护或发布 payload。
[posted loan 图](../../../docs/assets/diagrams/continuous-stream.svg) 明确以 F407 为
参考说明共用原则，H743 另加本节的缓存和同源时限约束。

TC 粘滞位和单比特 CT 不能计算漏掉的循环次数，因此目标替换前后都检查绝对时限。
USART PRESC1／OVER16／8N1 与 TIM2 PSC63 来自同一 HSI；实际 BRR 建立比例，
不使用名义 baud 或主机时间。按每字节至少8个 BRR 时钟保守估计，不假设理想10位
帧间隔；再扣除20字节和2个 timer tick。20字节上界覆盖16字节 FIFO、RDR、shift
及 direct beat 边界并向上取整；已经计入 NDTR 的 FIFO 部分仍重复扣除以保守。
TIM2 内部向上计数模式、ARR、DIER、溢出 IRQ、时钟／复位和 UART／缓存配置均
检查。这是无法证明即停止的软件准入界限，不是已测 service 延迟。

没有 FREE 替换块时停止生产并记录一个丢失边界，不覆盖 READY 或消费者持有的块；
迟服务或无法证明的 bank 历史丢弃未证明的 producer 数据。IDLE 关闭本次 producer，
只发布实际排空的有效前缀。成功 RX stop、消费者全部归还并重新初始化 stream 后，
由调用者显式重启，不自动重臂。RM0433 Rev9 15.3.19 的实际 EN 读回为0建立内存
排空依据，写 EN=0 的请求和旧 TC 都不能替代。缓存维护恢复调用者中断掩码执行，
准备／处理门控保证递归控制和维护失败期间仍保留全部借用。观察到时钟、timer、
复位或缓存破坏后，该保留生命周期永久 tainted，恢复寄存器不能解锁。模型、普通
ARM 链接和 SDK 消费分别建立开发证据，实际吞吐、缓存一致性、延迟与 HIL 仍为
`not_executed`。

ADC提供PA3/INP15的16位单次采样，包含稳压等待、偏移与线性校准，以及不同硅片cut的
官方LL boost编码；旧cut只保留实际BOOST位，屏蔽编码中保留且读回为零的位。校准完成必须观察到全部六个线性校准握手标志，控制寄存器写入
保留这些标志，停止和重启不隐式同时切换；每次退出深度掉电均重新校准。ADC的
Board supply_mv必须明确为2400..3300mV，参考电压必须至少2.0V且不得超过VDDA；16位数据格式
不代表已测量有效精度。

公开规范装配位于 `tools/configure/assemblies`：
`h743-spi-dma.toml`、`h743-spi-dma-uart-tx.toml` 和
`h743-spi-dma-uart-rx.toml` 选择缓存关闭，`h743-spi-dma-cache-on.toml`
选择缓存开启。它们仅选择 STM32H743ZIT6、裸机、固定 HSI64／默认用户 trim（HSITRIM=0x40）、
DEV_ID 0x450、V 修订 0x2003、SPI4 mode 0 和 8 位帧。
PE12／13／14 使用 AF5，PE11 是显式低有效 GPIO 片选；软件参考路由不证明
PCB 接线或电气时序。

每个请求借用已选且已初始化 AXI 内存中互不重叠的 TX／RX 字节区间，长度为
缓存关闭时为 1..65535 字节；缓存开启时，地址必须按 32 字节对齐，长度为
32..65504 字节且必须是 32 的倍数。TX／RX 的完整缓存行均被独占借用，包括
填充字节；驱动不会向相邻存储取整。请求接受后，在 DMA 前 clean TX、invalidate
RX，在内存、线上、IRQ 和地址全部撤回后再次 invalidate RX。缓存维护在恢复
调用者中断掩码后执行，准备阶段的门控禁止递归 service 提前归还借用。最终
invalidate 失败时请求继续隔离，实际重试成功后才可 SETTLED。缓存状态必须保持
至成功停止；一旦观察到变化，保留责任的 owner 永久 tainted。
RX 使用 DMA1 Stream2／DMAMUX1 request83／IRQ13，TX 使用
Stream3／request84／IRQ14，SPI4 使用 IRQ84；三条 IRQ 均为优先级 5、
`calls_os = false`。可选 USART3 TX Stream0 或 RX Stream1 使用独立资源，
所有选定使用者退出前仍保留共享 DMA1 时钟门责任。

静态 factory 返回已有 SPI 总线和端点接口；既有 H743 CMake 工程与安装 SDK
计划复用同一公共 C11 consumer，不暴露私有构造器或 MMIO。DMA 完成不等于
归还请求；撤回失败或时钟／状态不符时继续保留调用者存储和恢复责任。这里是
有限全双工 SPI，不是连续 UART 双向模式；不扩大为高频、其他后端、
16 位帧、固定 dummy 缓冲区或多段传输。配置、模型执行、ARM 链接、安装 SDK
消费与实板 HIL 各自需要独立证据，公开装配不代替这些执行，HIL 仍为
`not_executed`。

SPI4 轮询在固定 HSI 配置下同样只准入 DEV_ID 0x450、REV_ID 0x2003（V）
和 HSITRIM 0x40。先判实际修订，再读取 V 专用 HSICFGR；冷态非 V 返回
UNSUPPORTED，V 的外来 trim 返回 STATE，已有责任仍优先 BUSY。运行中时钟
不匹配时 transfer／recover／stop 返回 STATE，保留既有清理责任；这不收紧
通用固定 System 或 UART 支持。分频采用 65／32.5 MHz 固定 HSI 上界，PLL384
仍用 99.45 MHz APB2 上界；它们来自 DS12110 Rev11 V 修订的出厂 trim、电压和
结温条件，并非实测 SCK。HSI64 下 16 MHz 上限选 /8，250 kHz 上限不支持；
HSI32 下允许 250 kHz。quiet guard 继续用同源名义 timer／SPI 比，不能把频率
上界作为该等待的时基。

SPI 保留 CRCSIZE7。写 CSUSP 时屏蔽 rs 类型 CSTART 的采样值，避免误启动；
CSTART 清零、TSIZE 非零且 EOT 和 TXC 均置位，可证明正常结束，不强求竞态中
未出现的 SUSP。独立等待一个 SCK 周期加两个时基量化间隔后，排空 RX、关闭 SPE，实际
读回 CS 高，再把 AF5 SCK 交接到 CPOL 的 GPIO 电平并复位外设。已有且独占的
GPIO idle latch 保持不变；下一笔在 CS 高时配置 CPOL，再恢复 AF5。活动撤源
失败会保留调用帧及缓冲区借用并停驻，CS 已高也不撤销责任。RX 实际读出才计为
完成帧，TX FIFO 中未发送的字节不计入；timeout 原因一旦锁存不随迟到 RX 改写。
这些模型读回不证明实板无毛刺、MOSI 空闲电平或 HIL；静态 GPIOE／引脚独占租约
仍由装配方证明。I2C按勘误
对需要RELOAD的长接收采用单字节重装，忽略规定的master伪BERR，仍保留NACK、
仲裁丢失和溢出错误。Flash提供完整2MiB几何：每Bank1MiB、共16个128KiB sector、
32字节ECC flashword；目标地址和长度按32对齐，输入buffer可按1字节对齐。电压来自
明确的Board supply_mv，3.3V仅是软件参考假定；调用者必须证明目标自上次编程
后已成功擦除，全FF读回不能证明ECC单元尚未编程；全FF输入跳过物理脉冲。Cache-on、Bank swap、OTP与option
byte写入均未开放。正在执行的Flash脉冲不能被timeout撤销；必须等硬件排空后才返回，
排空或最终锁定失败将保留调用帧并进入终止故障路径。当前开发路径要求选定的
V修订0x2003、冷态RAM复制与独立工作区；该准入适用于包括固定HSI32/64和PLL384
在内的所有选定Flash配置，未知cut拒绝。UART和未声明自身cut合同的其他外设不
继承Flash限制；SPI另有上文的独立V／trim合同。未选Flash的装配不取得RAM工作区。
生成的Platform拥有唯一既有固定工作区及内嵌guard；外来工作区构造在效果发生前拒绝。
公共SDK consumer通过不可变nx_factory_flash face和普通Platform start/stop使用它，
无需提供工作区或包含私有类型。汇编bridge安装私有MSP，并在r8-r11中保护传入MPU
捕获值；C事务负责RAM VTOR发布、MPU guard和双Bank实际排空，最终汇编gate在
精确恢复调用者前检查已恢复硬件及链接器已知原始VTOR。选定链接要求显式read/store
无未分类项，且每个事务至多调用一次精确八次store的编程leaf。bridge入口处传入MSP
和活动Thread栈各需在SP下方另有260字节拥有的存储，不计普通C栈帧；私有栈界限
不能证明这些传入范围。不回退到普通Flash脉冲路径。完整IO-24退出及逐配置证据
仍以原台账为准；维护窗口屏蔽可配置中断，不承诺严格中断
延迟或硬实时deadline；MPU不停止其他bus master，外部调用者必须撤回并join。
完整边界见[RAM维护设计](../../../docs/design/h743-flash-ram.md)。所有物理HIL仍为
not_executed。
