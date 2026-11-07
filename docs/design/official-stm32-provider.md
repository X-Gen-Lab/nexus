---
id: design.official_stm32_provider
type: explanation
audience: maintainer
source_language: en
---

(design-official-stm32-provider)=
# Official STM32F4 drivers and Nexus contracts

Current production integration is `soc/stm32/stm32f4/`, with concrete F407VE/ZG
implementations. Reuse the locked official F4 HAL/LL and CMSIS Device sources;
keep vendor types and Handles private. Another F4 macro or SDK directory does
not establish a maintained mode, Board route or physical result.

## Responsibility boundary

| Owner | Responsibility |
| --- | --- |
| Official HAL/LL | Selected hardware operations and peripheral state handling |
| F4 C implementation | Resource admission, static binding, deadlines, borrows, callbacks, drain and lifecycle |
| F4 facts | Exact part, device macro, memory, startup, IRQ/DMA and reviewed routes |
| Board | Actual wiring, oscillators, initial levels and electrical conditions |
| Public I/O | Stable Nexus types, checked dispatch and hardware-independent contracts |
| External product | Tasks, execution progress, private regions and recovery policy |

One live hardware behavior domain has one transport owner. HAL and a private
state machine must not independently drive the same transfer, DMA or interrupt
sources. A hybrid mode needs explicit ownership boundaries, not two competing
executors. Direct access requires a specific SDK gap, erratum, atomic ordering
contract or demonstrated resource budget.

## Build-time provider contract

One schema-2 TOML selects a complete assembly. Reviewed JSON facts resolve into
immutable named IR; the generator creates exact storage, configuration, interface
faces, typed factory identities and fixed connections. CMake owns the actual
source and link graph. Do not duplicate source lists in a manifest.

Each image chooses one exact CMSIS device macro and matching startup. Compile
only selected HAL modules and required dependencies, with a consistent private
configuration and callback ABI. Do not enable every module or modify vendor
files. All other inventoried vendor sources remain retained and unselected.

Current host generation remains centralized Python. Board and SoC trees no
longer contain executable Python plugins. Moving remaining generated lifecycle
control into handwritten C and introducing Host C11 tooling are pending work;
translating strings into `fprintf()` would not fix the responsibility boundary.

## Selected F407 HAL UART transport

The current finite `dma-tx` selection combines official HAL TX with a disjoint
LL IRQ byte/event receiver under one private static owner. Handles, DMA state
and callbacks stay private. Other UART modes retain their explicit engines;
this selection does not establish whole-library or all-mode HAL replacement.

DMA completion is distinct from UART wire TC. A callback, successful abort call
or timeout does not authorize reuse of a request or buffer. Detach actual DMA,
IRQ and callback references, prove required peripheral drain, then publish
SETTLED as the provider's final request access. Failed withdrawal retains the
same storage and recovery responsibility.

Do not unconditionally call `HAL_Init()`: default SysTick setup and NVIC grouping
must respect Platform and RTOS ownership. Use the maintained time domain and
validate any kernel ISR ceiling before entering a kernel API.

## Replace a mode and verify it

Start from a complete selected mode and its tests. Preserve RX duties, instance
isolation, cancellation, deadlines, partial-start cleanup, late IRQ and failed
drain. Switch the complete behavior domain to its chosen implementation before
removing the old path; a successful TX fixture alone is insufficient.

Compare actual ELF/map and hot paths under the same toolchain and configuration.
Directory consolidation itself does not reduce dispatch or guarantee smaller
code. See [current support](../delivery/support.md),
[ownership](../explanation/ownership.md) and the
[provider guide](../guides/provider-porting.md). Physical HIL is not executed.

## Cortex-M4F short-ISR constraint

F407 ES0182 Rev19 section2.1.2 explicitly lists Arm776924 for every listed cut.
The exact chip errata, emitted instructions and selected FPU context remain
integration obligations; another CPU name does not grant the same workaround.

The failure requires an enabled FPU and lazy context saving, an interrupted
`VDIV` or `VSQRT` whose destination is s0–s15, a handler without floating-point
instructions, and exception return within fourteen cycles of the interrupted
instruction. The manufacturers' sufficient instruction-count workaround is
**more than two executed instructions in every ISR, excluding exception
return**. External stack-memory wait states are an alternative source condition,
not a premise supplied by these internal-SRAM platform profiles.

F407 startup enables CP10/CP11. Its selected FreeRTOS CM4F
port explicitly enables ASPEN/LSPEN. An integer-only current workload therefore
does not make the platform FPU-disabled or exclude floating-point application
code. Do not silently clear LSPEN in startup: the kernel re-enables it and its
context/stack ABI requires a separately reviewed decision.

Review the actual installed vector table and reachable instruction prefixes in
each emitted configuration. A thin `ldr` plus tail branch needs its callee
examined; unreachable `nop` alignment and literal-pool words are not executed
work. A default nonreturning fault loop is distinct from an empty returning
handler. Apply the same rule to external handlers, spurious/disabled-state fast
paths, aliases and compiler/LTO changes. Source line counts and nominal clock
frequency cannot prove this condition.

Selected-image software review belongs with its actual ELF, resolved input,
compiler commands and disassembly. It proves only those installed paths, not
every declared mode or a connected device result. A new or replacement external
ISR that can return within the excluded short prefix remains an open integration
condition until its emitted path or another exact workaround is reviewed.
