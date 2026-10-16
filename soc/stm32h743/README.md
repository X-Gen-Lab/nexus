# STM32H743 software reference

This provider maintains a narrow STM32H743ZIT6/NUCLEO-H743ZI2 vertical slice:
official Cortex-M7 startup, MMIO-only bounded LDO power entry, fixed HSI32 or
HSI64 with explicit Cache off/on, TIM2 1 MHz time, authorized GPIOB PB0 and
USART3 PD8/PD9 AF7 IRQ bytes, finite DMA transmission with IRQ-byte reception,
or finite DMA receive blocks with independent IRQ transmission.
Public C11 GPIO/UART faces remain immutable; each static instance owns its
private state and caller-explicit byte ring. Shared readonly ops perform hot
I/O directly. No allocator, worker, registry or DMA fallback is introduced.

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
Runtime start accepts only cold HSI64, undivided buses, ready LDO, conservative
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

The authored clock profile selects `hsi32-fixed-cache-off`,
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
GPIO preloads and observes safe levels before publishing output mode. UART uses
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

Only the selected AXI SRAM linker region is admitted for TX buffers. TCM, Flash,
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
first loan against the selected initialized AXI SRAM DMA-write region, provider
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

## Memory and evidence

AXI SRAM 512 KiB is the only initial linker domain. Physical ITCM/DTCM, D2/D3 and
backup SRAM facts preserve per-engine DMA1/DMA2/BDMA/MDMA reachability. MDMA can
reach TCM; no generic DMA boolean erases that distinction. Reachability does not
authorize placement, cached buffer ownership, power retention or execution.
Optional domains, additional DMA routes, dynamic PLL, deep STOP and bootloader
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
