# STM32G474 initial provider

This provider implements a deliberately narrow STM32G474RET6 baseline: the
Cortex-M4F startup, one fixed 170 MHz clock transaction, independent TIM2 time,
authorized GPIOA pins, LPUART1 IRQ bytes and exact PA0/PA1 EXTI events. It uses
the public typed GPIO/UART/EXTI faces with caller-owned private contexts and
bounded storage. Instances share readonly
operations tables; the IRQ moves at most one byte in each direction and records
hardware facts. There is no allocator, worker, runtime registry or DMA fallback.

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
Clock changes and deep-sleep continuity are excluded from this first profile.

The GPIO owner supplies an exact port/mask, output direction and initial/inactive
levels. Latch readback precedes output mode publication. Partial initialization
keeps `retained` set until safe rollback succeeds. The hot BSRR write is a direct
hardware command, with no polling or hidden lock. Toggle is a read/modify/write
sequence and requires external serialization with all users of the same pins.

The LPUART owner supplies a separate static state, ring, baud and IRQ policy. Only
LPUART1 IRQ 91, 8N1, FIFO-disabled bytes and PCLK 170/PRESC 16 are implemented. TX
request admission borrows request/payload until actual TC and source withdrawal
precede SETTLED. Failed wire drain preserves QUARANTINED storage. Cold stop also
joins captured notification callbacks and observes UE/TEACK/REACK/NVIC withdrawal
before returning ring/wake ownership. Notification is a hint; predicates remain
authoritative. No RX event/block or DMA mode is advertised.

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
