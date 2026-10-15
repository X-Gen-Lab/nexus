---
id: docs-explanation-platform-architecture
type: explanation
audience: consumer
source_language: en
---

(docs-explanation-platform-architecture)=
# How the platform is assembled and executed

Nexus is a statically assembled C11 MCU platform. It separates reusable mechanisms
from external product policy. One authored TOML selects exact hardware and optional
adapters; immutable I/O faces expose the chosen private providers. The application
owns execution and recovery. These are current design boundaries, not whole-platform
physical qualification.

![Current platform layers and external product boundary](../assets/diagrams/platform-architecture.svg)

FIG-01 expresses current layer responsibilities. Arrows describe labeled dependencies
or access; it is not an initialization sequence.

| Layer | State and responsibility |
| --- | --- |
| Core | Results, monotonic deadline, request publication and stable epochs |
| Arch | Compile-time CPU primitives; incoming masks, barriers, optional Cache/MPU/SAU |
| SoC | Exact chip clocks, memory, startup, IRQ/DMA and controller implementation |
| Board | Reviewed PCB routes, oscillator/safe-level facts and explicit unknowns |
| I/O | Typed public faces and operation contracts; no vendor layout |
| Optional OS/owner | Explicit per-object wait, task or shared-resource execution storage |
| Optional component | Bounded algorithms through narrow ports |
| External product | Main, workers, roles, private PCB, budgets, partitions and recovery |

## Build-time decisions and runtime work

The resolver combines authored TOML with reviewed SoC/Board facts into immutable
typed IR and an atomic bundle. It validates modes, resources, clocks, IRQs, ABI,
DMA reachability and budgets. CMake owns the source dependency graph; the resolver
does not discover software from device names or generate product workers.

![Actual compile-target dependency relationships](../assets/diagrams/build-targets.svg)

FIG-02 is the checked compile graph. Vendor and provider includes remain private;
public consumers receive Nexus types and selected compile/link policy.

![Authored inputs, atomic generation and firmware artifacts](../assets/diagrams/configuration-generation.svg)

FIG-03 shows current generation. Unknown and conflicting selections fail; no
Kconfig/cache/old-header fallback repairs them. Runtime factory lookup returns an
existing face. After explicit successful platform start, ordinary operations call
its shared ops table with its instance context. IRQ dispatch is fixed to provider
instances and does not repeat factory lookup for every byte.

The constructor contract names private state, definitions, initialization, stop
actions and failed-init responsibility explicitly. Common generation does not
infer these from family names. Selected STM32F4 official LL operations remain
inside private SoC state machines; factory and public I/O contracts are unchanged.
Full HAL Handle/callback transport requires separate mode-specific evaluation.
See [official driver integration](../design/official-stm32-provider.md).

## Lifecycle and failure responsibility

![Current initialization, failure withdrawal and retained cleanup responsibility](../assets/diagrams/platform-lifecycle.svg)

FIG-05 shows explicit startup and reverse cleanup. A static face can exist before
start and after stop; its pointer is not a capability to execute cold hardware.
Partial failure can retain irreversible watchdog effects or busy borrows. Products
must inspect primary and cleanup outcomes, continue the real executor, and retain
quarantined resources. Platform start never starts a scheduler.

Optional generated diagnosed start/stop entries write phase and controller
location to caller-owned cold-path storage. They retain no diagnostic pointer
and do not replace the primary/cleanup results or remaining-effect obligations.
See [the diagnostic contract](../design/official-stm32-provider.md) for index
meaning and failure responsibilities.

## Costs and tradeoffs

A 32-bit face contains two pointers: eight bytes of readonly storage. Provider/mode
ops are shared; exact mutable state and selected buffers remain separate. Indirection
lets one public interface serve independent concrete providers and GoogleMock
ports. Its cost is one public dispatch and code/readonly table retention; actual
Flash/RAM and timing are measured on the selected linked image.

Static assembly rejects conflicts early and prunes unselected drivers. It deliberately
forgoes hotplug discovery, runtime string registries, arbitrary dynamic clock changes
and automatic recovery policy. Optional shared owners add explicitly budgeted slots
and synchronization; direct single-executor paths need no default OS lock or worker.

Read [factory](factory.md), [ownership](ownership.md), the
[integration contract](../design/integration-contracts.md) and
[exact support](../reference/platforms/support.md). Architecture diagrams explain
mechanisms; tests, image checks and HIL establish their separate execution scopes.
