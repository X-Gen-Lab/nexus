# STM32G474 reference assembly proof

These fixtures consume the maintained `stm32g474re-hse24-reference` Board package
and the actual generated factory, startup, SoC providers and official CMSIS
dependency. `empty.toml` selects no peripheral. `io.toml` selects GPIOA PA5 and
LPUART1 PA2/PA3 with a 64-byte reception ring. Both select the reviewed
`hse24-pll170` profile, independent TIM2 timebase and explicit memory budgets.

The four authored inputs explicitly select `baremetal` or `freertos` and use
separate build directories. The fixture consumes the generated selection to
retain selected I/O operations in `g474_assembly.elf`, without a second selection
switch. The FreeRTOS fixture retains the actual static
task, scheduler, SVC, PendSV and SysTick implementation using caller-owned
8-byte-aligned task storage. A returning scheduler enters a local terminal loop;
it does not claim that no task ran or attempt pre-start rollback.

`provider-retention.json` observes the actual generated operations tables in the
selected ELF. `g474_assembly.resources.json` checks the memory budget. The empty
images must omit the LPUART provider, face, ring and vector implementation. A
weak unused vector in the official startup table may remain aliased to the
default handler. The selected images must retain the generated LPUART1 vector
and the independent TIM2 vector. FreeRTOS must retain its real kernel vectors and
the validated 170 MHz SysTick source, not a stub callback.

The host `nexus_g474_factory_test` compiles the actual generated bindings and
actual G474 clock/GPIO/UART implementation. Its force-included model substitutes
external CPU permission, MMIO addresses, read-only ready bits and NVIC
acknowledgements. It does not replace the startup transaction, constructors,
factory, request state machine or rollback code. These tests cover pure lookup,
real initialization, byte reception, final-wire TX completion, partial failure,
retained cleanup responsibility, unrelated pin preservation and restart.

All results establish software behavior and linking only. The reference package
does not prove a populated 24 MHz crystal, solder-bridge routing, MCU marking,
UART connector or electrical timing on a particular PCB. Physical execution,
waveforms, interrupt latency, DMA, power, stack peaks and HIL remain unexecuted.
This G474 vertical does not add ThreadX support.
