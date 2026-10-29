---
id: design.official_stm32_provider
type: explanation
audience: maintainer
source_language: en
---

(design-official-stm32-provider)=
# Official drivers and provider iteration

Use this contract when changing a SoC provider or integrating an official driver.
Nexus keeps static assembly, typed factories and public C11 I/O contracts. Official
HAL/LL code is private implementation input. This page defines the integration
boundary and work-package exits; it does not qualify every external library,
maintained mode, source revision or physical board.

![Current platform responsibilities and external product boundary](../assets/diagrams/platform-architecture.svg)

FIG-01 remains the current responsibility diagram. Reusing official code changes
the private SoC implementation, not the public dependency direction. FIG-03 in
[the architecture explanation](../explanation/platform-architecture.md) shows the
existing build-time generation path. Neither diagram implies a new runtime
registry, HAL manager, default worker or mandatory OS layer.

## Implementation boundary

The reviewed F4 dependency is STM32 HAL Driver v1.8.5 at
`b6f0ed3829f3829eb358a2e7417d80bba1a42db7`. Its CMSIS Device pairing comes from
STM32CubeF4 v1.28.3. The machine authority is
[the official-driver lock](../../dependencies/stm32f4-hal.lock.json), checked
against the selected Git dependency and imported SDK bytes. The lock records the
reviewed LL headers and license; it is not a declaration that all HAL modules are
compiled, supported or validated.

CMSIS Device pairing is separate from the shared Arm CMSIS-Core source. The
Core migration pins official v6.4.0 for the seven maintained STM32/GD32 family
entries while keeping their Device/startup, HAL/LL and imported vendor bytes.
The internal `nexus_cmsis_core` target supplies CPU headers; it does not change
public Nexus types, Arch context/ownership contracts or the selected peripheral
state machine. [Vendor versions](../../vendors/VERSIONS.md) distinguishes the
release, Core version macros, literal pack metadata and license identities.
Actual include closure, firmware links and installed-SDK consumption require
current-source evidence; a common Core pin does not transfer F4 qualification to
G4, H7, U5 or GD32.

The selected production approach uses official LL operations inside existing F407
provider state machines. It retains request admission, absolute deadlines, wire
completion, cancellation, drain and quarantine. A full HAL Handle/callback
transport is a separate evaluation and must not be described as an implemented
platform-wide HAL migration. G4, H7 and U5 do not inherit F4 support or dependency
qualification. GD32 remains a separate provider.

| Owner | Responsibility |
| --- | --- |
| Official HAL/LL/CMSIS | Reviewed hardware operations and selected vendor algorithms |
| Family provider | Exact peripheral behavior, private state, IRQ and error bridge |
| Common I/O | Admission, public results, borrowed storage and final publication contracts |
| Generated Platform | Static context/face construction and explicit C-call ordering for resource acquisition, startup and reverse cleanup |
| External product | Execution progress, workers, budgets, partitions and recovery policy |

Only one state machine may own a live peripheral operation. An adapter must not
run a HAL transfer engine and a second independent register engine over the same
interrupt flags or DMA channel. Mixed LL/register operations require explicit
ordering and ownership. Vendor types, mutable handles and headers stay out of
public interfaces. Factory lookup returns an existing face without initializing
hardware or allocating storage.

## Build-time provider contract

Each constructor returns the frozen `Construction` record from
[provider common contracts](../../tools/configure/providers/common.py). It names
`state_type`, `definitions`, `initialize`, `stop`, `retain_on_failed_init`,
`exports` and `pin_snapshots`. The common emitter consumes these decisions rather
than reconstructing state types or failed-init responsibility from a family name.
`initialize` is an immutable `DirectCall`; `stop` is a tuple of these calls.
Arguments are explicit static `Symbol` or `Address` values, uint32 constants,
booleans, NULL, or nested direct calls such as absolute-deadline construction.
Free-form expressions, casts, assignments and function-pointer/member-call
expressions reject.
`definitions`, `exports` and `face_definitions` remain C source strings for
declarations, static configuration, storage, faces and fixed forwarding. Concrete
C adapters own hardware algorithms. The real C compiler establishes symbol
and parameter types. This is a narrow Python generation contract, not a complete
C AST; these Python records introduce no additional runtime metadata or dispatch
into the MCU image.

The family owns numerical limits, supported modes, exact routes and construction
semantics. The resolver owns strict fields, immutable inputs and resource
conflicts. CMake owns source selection and target dependencies. Preserve these
separate authorities: generation must not create a second source build graph,
and a syntactically valid configuration must not imply unimplemented hardware.

## Selected immutable I2C V2 configuration

The selected G474, H743 and U575 polling I2C V2 providers separate the private
`nx_i2c_v2_config_t` from `nx_i2c_v2_state_t`. The generated readonly configuration
holds register/reset addresses, the reset mask, timing, finite polling budget
and the explicitly selected BERR and one-byte RX reload quirks. Zero-initialized
runtime storage keeps a configuration pointer and `initialized`, `retained`,
`active` and `fault` state. Public faces, shared ops and factory lookup stay the
same. This split is not a measured Flash or RAM saving.

The private `nx_i2c_v2_bind` runs under the serialized privileged task owner with
interrupts unmasked.
It validates the configuration and rejects overlap with runtime or hardware
storage. An initialized, retained or active owner rejects rebinding without
mutation; successful binding itself has no hardware effects. The immutable
configuration must live as long as its runtime owner; successful stop releases
hardware ownership but preserves the binding. Do not reset a live owner merely
to replace its configuration.

Concrete G474/H743 `i2c_prepare` entries bind first, then prepare pins and
initialize the engine. U575's separate mutable clock owner rejects a retained
clock before binding, and acquires the selected clock before engine initialization.
Its `i2c_close` stops the engine first; it releases the clock only after that stop
succeeds. Failed drain retains the relevant runtime and clock responsibility
for explicit cleanup or retry.

The family emitter now supplies static configuration and calls these concrete C
entries. The existing `Construction` record and generated Platform still order
C calls for resource phases, controller starts and reverse cleanup. Other
maintained peripheral modes use their own family static bindings and concrete C
preparation; this does not move every platform lifecycle into a new common
abstraction.

See the [private shared contract](../../drivers/i2c/i2c_v2/i2c_v2.h) and the
[U575 integration](../../soc/stm32/stm32u575/drivers/i2c.c) for exact parameters
and failure ordering. The [refactor ledger](soc-ip-layout-execution.csv) separates
focused software evidence, final linked cost comparison and physical execution.

## Selected immutable UART DMA configuration

The selected F103 and GD303 UART DMA modes use `nx_uart_v1_dma_config_t` for
register/channel addresses, DMA memory domains, IRQ withdrawal and wake-validation
callbacks, byte-ring or duplex receive storage, capacity, baud, IRQ policy and
channel shifts. The selected profile identifies the active storage union member.
`nx_uart_v1_dma_state_t` retains the configuration pointer, deadlines, active
request and wake references, ring cursors, publication state and lifecycle flags.
Duplex embeds its own mutable receive extension; byte mode does not reserve one.
Configuration and its referenced resources have the runtime owner's lifetime.
Successful stop preserves the binding, and zero-initialized runtime binds only
during cold admission.

Each family's concrete `uart_dma_prepare` and `uart_duplex_prepare` requires a
serialized privileged task with interrupts unmasked. After null/profile checks,
it checks the existing cold owner before preparing exact pins and calling the
initializer. Retained hardware, active requests, wake publishers and duplex RX
loans prevent rebinding; the candidate receive extension is checked even before
a configuration is bound. Failed withdrawal retains the same configuration and
outstanding loans for ordinary stop or retry. The shared readonly ops, public
faces and direct IRQ symbols remain unchanged; no hot-path dispatch layer is
added.

The family's frozen `LegacyTransportProfile` supplies exact wiring to the
existing emitter without family-name fallback. For these UART modes the selected
`Construction.initialize` calls concrete C preparation; generation supplies
static configuration, faces and IRQ forwarding. Concrete C adapters now own AHB
DMA gates, pin resources and retained cleanup. The generated Platform keeps
explicit phase ordering and typed C calls. Legacy I2C V1 and the other maintained
peripheral cold paths follow the same C hardware-algorithm boundary.

See the [UART private contract](../../drivers/uart/uart_v1_channel_dma/uart_v1_channel_dma.h)
for the exact storage, callback and lifetime requirements. Host tests exercise
the actual SoC wrapper, shared engine and GPIO model; generated factory validation
and final ARM resource measurements remain separate evidence scopes.

## Selected legacy cold integration

F103 and GD303 keep concrete ADC, PWM and Flash `prepare` entries in their
respective `drivers/devices.c`. These privileged task entries require interrupts
unmasked and reject the relevant live, retained or faulted owner before replacing
its resource binding. The exact C adapter owns register addresses, fixed pin and
gate relationships, system-clock state and chip-specific cold checks. Family
generation still validates the selected Board/mode and supplies its chosen values.
The existing shared `nx_legacy_*_close` paths retain stop and drain responsibility.

The ADC and PWM owners now reference their own readonly hardware descriptions,
`nx_legacy_adc_config_t` and `nx_legacy_pwm_config_t`, supplied by each exact SoC.
Register addresses, gates and fixed pad facts move out of mutable owner storage.
ADC reference voltage and PWM tick/period/duty/inactive level remain selected
owner state; they are not promoted into shared hardware facts. Configuration
must outlive the owner and cannot overlap its state or API output storage.
This does not turn these drivers into the I2C V2 type; Flash keeps its separate
private contract. Actual linked costs depend on the exact selected image.

Selected U575 USART1 `dma-tx` and `dma-chain-tx` use static cold configuration and
concrete C preparation. Their C adapters own admission, binding and pin preparation
before existing finite/chain initialization. Selected F407 SPI polling/DMA and paired-controller DMA, plus
GD470 SPI0/SPI4 polling and finite full-duplex DMA, move prepare/close behavior
into concrete C while preserving the same Platform snapshot slots and retained
cleanup responsibility. GD470 also moves its conditional EXTI close into C.
The hot transfer engines, public faces and direct IRQ ownership are unchanged.

The maintained Native, F103, F407, G474, H743, U575, GD303 and GD470 selections
now keep hardware algorithms in C11, including selected GPIO, IRQ UART, ADC, PWM,
EXTI, preflight, shared resources and retained cleanup. Host Python supplies static
configuration, storage, faces and fixed typed calls; the generated Platform still
owns explicit startup and reverse-cleanup ordering. Review the actual selected
C path rather than inferring support from a call field or directory. The
{ref}`sealed cold-lifecycle baseline <sealed-cold-lifecycle-baseline>`
records the exact final source and software evidence; physical timing remains
unexecuted.

## Conditions for full HAL transport

Before accepting a Handle-based mode, establish a progressing time source for
every HAL wait it can reach. Review bootstrap clock waits, `HAL_GetTick`, tick
reinitialization and interrupt-priority grouping. Do not let default HAL startup
silently acquire SysTick or change OS policy. Public operations consume one
absolute monotonic deadline, including queue and preparation time.

Establish request identity and borrowed references before enabling hardware or
calling an API that can invoke a callback synchronously. Callback bridges publish
bounded facts and optional wake hints; they do not execute arbitrary product
callbacks. Check the complete HAL call stack before publishing settlement or
allowing reentrant submission. A HAL lock is not a task mutex or a controller
ownership policy.

![Reference finite-DMA completion and failure responsibilities](../assets/diagrams/dma-completion.svg)

FIG-07 is the existing F407 finite-DMA reference. DMA completion, wire completion,
HAL internal readiness and Nexus settlement remain different facts. Preserve the
[public ownership contract](../explanation/ownership.md): abort or timeout starts
withdrawal; it does not return storage. Detach every required hardware, IRQ and
queue reference before the provider's final release-publication of `SETTLED`.
Failed drain retains `QUARANTINED` storage and an explicit recovery obligation.
Circular DMA must never overwrite a consumer-owned block to emulate progress.

## Caller-owned lifecycle diagnostics

Generated headers expose `nx_platform_diagnostic_t` and the optional
`nx_platform_start_diagnosed` / `nx_platform_stop_diagnosed` entries. The original
start/stop entry points remain available. A null diagnostic pointer uses the
original behavior; a nonnull pointer supplies caller-owned cold-path output and
is never retained. It adds no static registry, labels or default instance RAM.
Cold code/stack cost still belongs in the linked-image comparison.

`phase` and `cleanup_phase` distinguish `NONE`, `CONTEXT`, `SYSTEM`, `RESOURCES`
and `CONTROLLER`. A controller phase carries its zero-based index in
`resolved.json.controllers`; it is not a per-kind factory ID. `SIZE_MAX` means
no controller. Successful calls clear the diagnostic. A failed start records the
primary location and, if cleanup fails, a separate cleanup location. Existing
primary result, cleanup result and remaining effects remain authoritative;
diagnostics neither perform recovery nor establish that a retained borrow ended.

## Work-package exits and existing authorities

The following table assigns exits to the iteration work packages. It is a design
checklist, not a second support or completion database. Current support continues
to come from [the derived capability matrix](../delivery/capabilities.md), exact
provider facts and reviewed fixtures. Existing ledgers retain original IDs,
statuses and evidence; do not relabel historical results as new-source execution.

| Work package | Exit and maintained authority |
| --- | --- |
| Configuration and provider contract | Explicit construction and failed-init responsibility; deterministic generation and rejection tests; [integration contract](integration-contracts.md) |
| Official driver reuse | Locked source/license/pairing, private build and SDK closure, mode-specific real-driver tests; [source compatibility](source-compatibility.md) |
| Hardware/resource description | Exact pins, IRQ, DMA, clocks and memory; no invented Board wiring or silent fallback; [SoC ledger](soc-refinement.md) |
| Lifecycle and diagnostics | Every partial effect either withdrawn or retained; caller-owned cold diagnostics never revoke a borrow; [lifecycle matrix](io-lifecycle-matrix.md) |
| I/O ownership | Real acceptance, cancel, late IRQ, overflow, instance isolation and drain regressions; [I/O recovery contract](io-observation-recovery.md) |
| Arch, OS and components | Exact backend/CPU scope, unchanged masks, explicit execution and wait/observer shutdown; [OS contract](os-contracts.md) |
| Performance and resources | Same configuration/toolchain/workload comparison, actual ELF/RAM/stack reports; [execution budgets](soc-execution-budgets.md) |
| Test plan and HIL | Reviewed applicability plus fresh executed results; software and physical status separate; [qualification](../delivery/qualification.md) |
| External consumption and documentation | Relocated Source SDK consumer, external examples and complete reviewed English/Chinese pages; [build ledger](build-system-execution.csv) and [documentation ledger](documentation-system-execution.csv) |

The read-only work-package view joins existing provider, capability, SDK-required
source and reviewed lifecycle-test authorities. It retains exact scopes and shows
unmapped properties instead of inventing aliases. It does not execute tests,
verify a complete installed SDK or establish software/physical qualification.
See [maintenance tools](../../tools/maintenance/README.md) for the actual command
and output limits.

## Comparison and replacement

Compare official and existing implementations with the same chip, clock, memory,
route, mode, capacity, OS, compiler, optimization and workload. Record Flash LOAD
and footprint separately, per-domain static RAM, caller-owned storage, stack
reservation versus measured peak, IRQ/service behavior and cleanup failures.
Operation counts and host timings are not physical cycles, latency or WCET.

Retain an old implementation only while it provides a needed comparison or an
unreplaced maintained mode. Select one normal production implementation after its
contract and costs are accepted; remove unused switches and duplicate state.
A dedicated LL/register path is justified by a demonstrated semantic gap or
measured budget, not by a general assertion that vendor code is slow.

## Validation and delivery boundary

New behavior follows RED, GREEN and refactor with the normal commit/CI gates.
Host C tests use GoogleTest/GoogleMock; tool tests use stdlib unittest. Mock an
external port when checking the adapter contract, then separately execute real
official code with the adapter. ARM links establish selected image/resource
properties. Real equipment is required for IRQ timing, DMA/Cache coherence,
electrical behavior and long-load conclusions.

A software candidate still requires clean source, exact dependency/configuration
identity, independent SDK consumption and two fresh network-disabled builds of
the same artifacts. No iteration checklist or derived work-package report grants
that status. Existing release tags/assets and original evidence remain immutable.
Without connected boards, HIL stays `not_executed`; do not call a software-only
iteration a product release or safety qualification.

Bootloader handoff, dynamic reclocking, deep STOP, broader H743 DMA and generic
U575 cross-domain I/O retain their separate unimplemented or partial exits in the
[SoC refinement ledger](soc-refinement.md). Official drivers do not automatically
supply the platform contracts for these features.
