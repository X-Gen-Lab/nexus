# SoC extension verification

This record continues the [earlier SoC baseline](soc-refinement-verification.md).
The [55-item ledger](../design/soc-refinement-execution.csv) retains its original
exit conditions. Implemented software, executed software evidence and physical
acceptance are separate states. Hardware is not connected; every physical result
remains `not_executed`.

Final source validation and candidate sealing are in progress. Partial passes
do not establish a complete passing delivery or qualify a later source revision.

## Implemented scope

| Layer | Added behavior | Exact boundary |
|---|---|---|
| Arch | Caller-owned Cortex-M7 cache transition control | Cold privileged entry, verified cache geometry, explicit ownership and bounded cleanup; other cores reject this particular control operation |
| Arch | Cortex-M33 NonSecure range permission observation | Real CMSE check; observation does not pin storage, copy data or authorize arbitrary peripherals |
| G474 | Static EXTI event providers | Two selected software-fixture lines, bounded rings, pending-bit isolation, overflow order and retained cleanup; PCB routing and electrical behavior remain unqualified |
| H743 | Fixed HSI32/HSI64 startup with cache selection | Four cold profiles; independent divider, timer and cache ownership; this is not runtime frequency switching |
| H743 | USART3 finite TX using DMA1/DMAMUX | AXI SRAM only; explicit buffer borrow, wire completion, cancellation and quarantine; RX remains IRQ byte mode |
| U575 | Exact Secure/NonSecure software reference | Fixed Flash/RAM attribution, restricted NonSecure MPU, actual CMSE gateway, synchronous bounded broker and real paired images |
| SDK | Required private implementation closure | Deleting a required source/header and recalculating the manifest still fails validation |
| Automation | Complete dependency initialization and compiler context | SDK dependencies come from declared locks; static analysis must preserve the actual translation unit's defines and forced includes |

The public device architecture remains static typed factory plus immutable
polymorphic interfaces. Factory lookup does not allocate, start hardware or take
a lock. New modes extend selected private providers and configuration facts;
they do not add a runtime registry, implicit worker or mandatory OS layer.

H743 DMA borrows request and payload storage until acquire-observed SETTLED.
Cache-enabled transfers require the caller to own complete aligned cache lines;
the provider does not round maintenance outward into neighboring allocations or
allocate a bounce buffer. Cancellation and timeout do not revoke the borrow.
Failure to withdraw hardware references retains storage in quarantine.

The U575 reference admits a fixed TZEN=1 boot profile and an immutable
unprivileged NonSecure thread without NonSecure interrupts, an RTOS or concurrent
DMA. It clears and verifies future NonSecure RAM before changing attribution,
checks final protection state, and enters through the actual compiler CMSE
sequence. Its synchronous gateway copies bounded input and checks output before
publication. This does not implement general asynchronous cross-domain loans,
arbitrary userspace tasks, product authentication or secure update policy. See
the [complete contract](../design/u575-trustzone-contract.md).

## Evidence rules

Normal commit hooks execute the complete GoogleTest/GoogleMock, configuration
and tool suites for behavioral changes. Formal qualification additionally binds
a clean canonical commit and tree, actual compiler inputs, raw command output,
configuration and linked artifacts. Failed attempts remain failed evidence;
changing the source requires new source-bound execution.

The verification work exposed missing generated-header input in the H743 ARM
startup test, a fixed virtual-environment path in CPU configuration tests, an
uncontrolled scheduling assumption in a serial-noise test, and lost forced-include
context in static analysis. Sanitizer jobs also exposed register-mask width
conversions in host models and signal-dependent termination observers. Their
regressions retain the actual compiler warnings and production nonreturning
loops; test observers must work without a signal-delivery point inside those
loops. Regression evidence and final execution status belong with the delivery
index. No suppression or skipped test can substitute for closing these findings.

Offline candidates cover the eight maintained public H743/U575 combinations of
empty/IRQ-UART assemblies and baremetal/FreeRTOS. Each requires two fresh,
network-disabled builds and comparison of actual ELF/BIN bytes, with both maps
and raw logs retained. The H743 cache/DMA test fixtures and U575 paired-image
reference have their own ARM evidence; they are outside those eight single-image
candidate configurations. An unsigned software candidate is not a product
release or physical qualification.

## Remaining acceptance

SOC-28 through SOC-32 and SOC-36 are exact partial references. Their original
hardware, peripheral and isolation exits remain open. SOC-33 bootloader/running
state handoff, SOC-34 dynamic frequency changes and SOC-35 deep STOP with
continuous time are not implemented. SOC-37 through SOC-55 provide station,
admission, capture and result-validation tooling; physical execution remains
pending equipment.

Fixed clock profiles do not establish DVFS. Ordinary WFI or tickless idle does
not establish deep STOP. Host models and linked ARM images do not establish
electrical behavior, DMA/cache coherence on silicon, TrustZone isolation,
interrupt latency, measured power or WCET. Applications, PCB-specific policies
and recovery decisions remain in external projects.
