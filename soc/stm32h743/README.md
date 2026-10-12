# STM32H743 software reference

This provider maintains a narrow STM32H743ZIT6/NUCLEO-H743ZI2 vertical slice:
official Cortex-M7 startup, MMIO-only bounded LDO power entry, fixed HSI64 with
Cache off, TIM2 1 MHz time, authorized GPIOB PB0 and USART3 PD8/PD9 AF7 IRQ bytes.
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
8N1, PRESC1/PCLK64, no FIFO/DMA, actual TEACK/REACK and IRQ readback. Its bounded
ISR moves at most one TX and one RX byte. Requests settle only after actual wire
TC and withdrawal of retained sources. Failed drain preserves QUARANTINED
borrowed storage. Stop joins notification publishers, withdraws UE/ACK/IRQ and
returns ring/wake responsibility only on success. External consumers must join
before platform stop.

## Memory and evidence

AXI SRAM 512 KiB is the only initial linker domain. Physical ITCM/DTCM, D2/D3 and
backup SRAM facts preserve per-engine DMA1/DMA2/BDMA/MDMA reachability. MDMA can
reach TCM; no generic DMA boolean erases that distinction. Reachability does not
authorize placement, cached buffer ownership, power retention or execution.
Optional domains, DMA drivers, cache maintenance, dynamic PLL, deep STOP and
bootloader handoff have separate implementation and acceptance exits.

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
