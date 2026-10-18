# STM32H743 software reference

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
Read the [high-clock contract](../../docs/design/h743-high-clock.md) before
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
inferred. Additional H7 DMA engines, RX continuous/double-buffer modes and
physical cache coherence remain separate acceptance scopes.

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
[FIFO selection and lifecycle contract](../../docs/design/uart-fifo.md).
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
[`peripherals.toml`](../../tests/contracts/h743_assembly/peripherals.toml).
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
above. Assemblies without Flash acquire no RAM workspace. One generated
workspace holds private vectors, stack, staged input and protection state. The
CPU-only assembly bridge installs private MSP; the RAM transaction owns VTOR
publication/readback, the MPU guard and physical both-bank drain. There is no
fallback to the former ordinary Flash-resident pulse path. These mechanisms and
their still-open production-ELF/software exits are described in the
[RAM maintenance design](../../docs/design/h743-flash-ram.md).

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

The selected private LL SPI/ADC headers and original BSD-3-Clause notice are
paired with STM32CubeH7v1.13.0 by the reviewed source lock. Public faces retain
Nexus types. Official LL field encodings and pure TSIZE access are reused while
Nexus remains the single transfer, time, IRQ and shutdown owner.

## 中文范围说明

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
完整条件、公式、调用顺序和验证边界以[高频合同](../../docs/design/h743-high-clock.md)为准。

本轮接入精确STM32H743ZIT6软件参考的SPI4、I2C1、ADC1、TIM3 CH1、EXTI0/1、
IWDG1与双Bank物理Flash。原NUCLEO GPIO/USART3参考保持独立；新增引脚组合位于
明确的软件参考Board包，不能据此推断实板接线、电压或外设已验证。typed factory只查找
静态face，热路径直接调用共享只读ops与独立context，不分配对象，不启动隐藏线程。

可选D2 SRAM1必须通过独立容量预算选择。复位阶段确认供电与时钟后，按链接边界复制
DATA并清零BSS，NOINIT由调用者初始化；DMA只准入已初始化的AXI/SRAM1，并在借用前
检查SRAM1时钟。Cache打开时仍要求独占完整32字节行。已有有限RX正常模式以owner
service发布和重启，背压期间不会覆盖已借出的block，重启间隙仍然存在，不能宣称无缝连续。

ADC提供PA3/INP15的16位单次采样，包含稳压等待、偏移与线性校准，以及不同硅片cut的
官方LL boost编码；旧cut只保留实际BOOST位，屏蔽编码中保留且读回为零的位。校准完成必须观察到全部六个线性校准握手标志，控制寄存器写入
保留这些标志，停止和重启不隐式同时切换；每次退出深度掉电均重新校准。ADC的
Board supply_mv必须明确为2400..3300mV，参考电压必须至少2.0V且不得超过VDDA；16位数据格式
不代表已测量有效精度。

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
继承Flash限制；SPI另有上文的独立V／trim合同。未选Flash的装配也不取得RAM工作区。不再回退到原Flash驻留的脉冲路径；CPU bridge
仅负责栈和CPU状态，RAM事务负责VTOR、MPU和实际排空。生产ELF闭包及完整软件
退出尚未完成，IO-24仍为in_progress。维护窗口屏蔽可配置中断，不承诺严格中断
延迟或硬实时deadline；MPU不停止其他bus master，外部调用者必须撤回并join。
完整边界见[RAM维护设计](../../docs/design/h743-flash-ram.md)。所有物理HIL仍为
not_executed。
