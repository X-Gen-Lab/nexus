# STM32H743 software reference

This provider maintains a narrow STM32H743ZIT6/NUCLEO-H743ZI2 vertical slice:
official Cortex-M7 startup, MMIO-only bounded LDO power entry, fixed HSI32 or
HSI64 with explicit Cache off/on, TIM2 1 MHz time, authorized GPIOB PB0 and
USART3 PD8/PD9 AF7 IRQ bytes or finite DMA transmission with IRQ-byte reception.
Public C11 GPIO/UART faces remain immutable; each static instance owns its
private state and caller-explicit byte ring. Shared readonly ops perform hot
I/O directly. No allocator, worker, registry or DMA fallback is introduced.

## Reset and ownership

Official reset calls `ExitRun0Mode` before `SystemInit`, DATA/BSS initialization
and constructors. RM0433 Rev9 section 6.4.1 forbids every RAM write during
Run-star until ACTVOSRDY. The power entry uses only scratch registers and PWR
MMIO; it never pushes, spills, calls another function or touches main RAM. It
checks LDO/BYPASS readback and polls at most one million observations. A refusal
masks IRQs and retains reason 1 (ready timeout) or 2 (supply readback failure) in
r0 before a terminal loop. Poll counts do not promise measured wall-clock time.
Production C asserts the exact assembly addresses/bits against the fixed SDK.

`SystemInit` uses only CPU/system registers after power entry permits stack
writes. It sets the explicit FPU/vector/grouping ABI without initialized globals.
Runtime start accepts only cold HSI64, undivided buses, ready LDO, conservative
reset Flash latency 7WS and Cache off. It rejects unknown running states before
mutation. No running-clock, bootloader, cache, scheduler or alternate supply
state is silently adopted. Runtime owns reserved TIM2/IRQ; failure preserves
cleanup responsibility for retry. HSI/LDO/Flash are boot prerequisites and are
not withdrawn when runtime stops.

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
IRQ-byte ring. There is no receive DMA, circular mode, stream pool or fallback.
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
