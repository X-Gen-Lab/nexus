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

## Current source organization

The source tree groups MCU families by vendor and CPU implementations by
instruction-set architecture. These directories describe ownership at build
time; they add no runtime dispatch or lookup layer. Public include paths,
target names and authored family identifiers remain stable.

The [source-adjacent contracts](source-map.md) own the directory map and layer
README content. Native models implement the same public I/O contracts under the
Native SoC package. Shared register-protocol algorithms live under
`drivers/<kind>/<ip>/` only when the participating wrappers independently
establish their silicon semantics. Selected SoC targets compile these private
sources directly; the directory adds no runtime layer to FIG-01. Cross-vendor
SoC collaboration remains in `soc/common/`, while STM32 identity access remains
under `soc/stm32/common/`.

Each family's `configure/` package owns its capability policy, constructors and
lifecycle emission; Native uses `soc/native/configure/`. The explicit provider
map and shared leaf contracts remain in `tools/configure/providers/`. Common
parsing, resource arbitration, typed IR and atomic publication stay in
`tools/configure/`. Configuration tests stay under
`tools/configure/tests/providers/<family>/`. These are build-time Python
boundaries, independent of the C execution path.

Common components and selected STM32 extensions have distinct source ownership.
Components are explicit targets; a chip-specific target still requires its
exact SoC, mode and backend, and its presence does not extend the public support
matrix.

## Platform and CPU-only Runtime

| Consumption entry | Provided assembly | External responsibility |
| --- | --- | --- |
| `Nexus::Platform` | Exact SoC/Board facts, selected providers, static factory and explicit lifecycle | Product sources, workers, budgets, storage regions and recovery |
| `Nexus::Runtime` | Core, Arch, public I/O faces and the explicitly selected OS adapter | Startup, vectors, memory/clock facts, Board wiring and concrete device providers |

One build directory admits one resolved assembly. Use separate build
directories for different boards or CPU/security images; a product build
coordinator or CI matrix owns their orchestration. Runtime and Platform reuse
the same production mechanisms but are separate consumption modes.

The common OS boundary is the wait/wake port. Task, queue, join and storage APIs
remain backend-specific, including the real kernel control-block types.
Products switching kernels must adapt that lifecycle explicitly; no universal
object pool or hidden scheduling task is supplied.

## Build-time decisions and runtime work

TOML schema 2 is the only authored platform input. The parser freezes user intent
as `AuthoredAssembly`; `resolve_ir()` combines it with reviewed SoC/Board facts
and validates modes, resources, clocks, IRQs, ABI, DMA reachability and budgets.
It returns `ConfigurationIR` directly. Named frozen records hold each decision
once; they do not expose a Mapping facade or a duplicate `values` store. Hardware
fact leaves and keyed indexes still use immutable mappings, and local resolver
validation drafts may use dictionaries. These are explicit data boundaries,
not a promise to assign a separate nominal type to every JSON field.

Factory and emitters consume the same typed result. `to_dict()` derives the
schema-1 `resolved.json` audit/artifact projection; it is never read back as an
authored platform input. Private typed source epoch 2 is separate from that
serialized format and from the public C factory epoch. Old JSON assemblies are
rejected. Historical evidence keeps its original bytes and read-only verification.
CMake owns the source dependency graph; resolution does not discover software
from device names or generate product workers.

Firmware execution is C11 plus necessary architecture assembly. Python runs on
the host to validate inputs and generate static C/H; the MCU has no Python
interpreter. Hardware algorithms belong in C: register access, admission, waits,
resource acquisition, rollback, stop and recovery. Generation connects immutable
configuration, exact instances, factory getters and fixed IRQ handlers. Constant
addresses and thin typed call sequences can remain generated without adding a
runtime interpreter or another dispatch layer.

The reviewed Native and seven MCU-family cold paths keep preflight, resource
acquisition, preparation, failed-start cleanup and stop behavior in C. Shared
enhanced-SPI admission calls read-only C checks before acquiring resources or
marking failed-init cleanup responsibility. H743 fixed-run pause/resume uses C
resource and runtime lease owners. Unreachable hardware generators and unused
default MMIO macros have been removed; pure pruning and intentional output
deletions retain separate comparisons. The
[delivery-completion ledger](../design/platform-delivery-completion.csv) records
implementation, exact development executions and final integration separately.

A cold receipt proves which resources this caller actually acquired. Rejection
with an empty receipt cannot stop or overwrite an existing owner. Partial
acquisition and failed restoration retain the original snapshot and unfinished
effects for retry; a public peripheral stop does not discard the enclosing
Platform lease. These checks run on the real C helpers in the register models.
Factory lookup and ordinary I/O retain their existing direct typed paths.

Generated static C and maintained C use the same firmware toolchain. Moving an
algorithm changes its maintenance and test ownership; it does not promise a
universal resource reduction. The recorded F407 I2C owner-guard comparison saves
RAM while increasing IRQ/DMA Flash. Linked costs and physical timing retain
their own source, configuration and measurement requirements.

![Actual compile-target dependency relationships](../assets/diagrams/build-targets.svg)

FIG-02 is the checked compile graph. Vendor and provider includes remain private;
public consumers receive Nexus types and selected compile/link policy.

![Authored inputs, atomic generation and firmware artifacts](../assets/diagrams/configuration-generation.svg)

FIG-03 shows current generation. Unknown and conflicting selections fail; no
Kconfig/cache/old-header fallback repairs them. Runtime factory lookup returns an
existing face. After explicit successful platform start, ordinary operations call
its shared ops table with its instance context. IRQ dispatch is fixed to provider
instances and does not repeat factory lookup for every byte.

Each of the eight maintained providers owns data-only `descriptor.py` metadata
at its SoC package root, outside `configure/`. The central frozen
`FamilyDescriptor` and explicit family-to-module map remain in
`tools/configure/providers/descriptor.py`; the provider loader delegates that
lookup without a second eager registry. A single `descriptor.maintained()` lookup
loads only selected metadata, without configuration behavior; `providers.maintained()`
uses that record to import the selected configure package. Explicit enumeration,
source snapshots and SDK inventory may read all metadata. No descriptor becomes
a second software source graph.

The constructor contract names private state, definitions, read-only admission,
initialization, stop actions and failed-init responsibility explicitly. Admission,
initializer and stop lists use immutable `DirectCall` values with explicit static
references or constants; no free-form C expression or function-pointer/member-call
expression can enter those fields. Definitions remain generated static C source
and fixed connections; hardware algorithms are maintained in C. The C compiler
establishes actual symbol and parameter types. Model tests and linked-image
checks remain necessary: this narrow representation is not a complete typed C
AST. Common generation does not infer these decisions from family names. Selected
STM32F4 official LL operations remain inside private SoC state machines. Selected
F407 UART `dma-tx` uses private HAL finite TX with disjoint LL IRQ RX under one
owner; other UART modes retain their existing engines. The static shared-ops
factory and public I/O contracts are unchanged. This mode-specific integration
does not qualify another family or replace current-source evidence. See
[official driver integration](../design/official-stm32-provider.md).

The legacy F103/GD303 selections share private algorithms only after exact vendor
register, bit, clock and memory checks. A TX-only channel-DMA instance owns a
compact TX prefix and an IRQ-byte ring; only duplex adds the finite RX-block
extension. Selected methods and direct IRQ entries let ordinary section GC remove
unused block code. No runtime forwarding layer or implicit mode fallback is added.

G474's selected ADC bulb mode, H743's Cache/domain policies and U575's TZEN0
peripherals remain exact SoC contracts. Optional H743 D2 SRAM1 is budgeted and
initialized before DMA admission; hardware reachability alone does not authorize
an arbitrary address. Finite RX/ADC blocks require explicit owner service and
rearm gaps. The separate TZEN1 GPIO job copies at most eight commands into Secure
storage and retains no NonSecure pointer. It is not general asynchronous I/O.

## Lifecycle and failure responsibility

![Current initialization, failure withdrawal and retained cleanup responsibility](../assets/diagrams/platform-lifecycle.svg)

FIG-05 shows explicit startup and reverse cleanup. A static face can exist before
start and after stop; its pointer is not a capability to execute cold hardware.
Partial failure can retain irreversible watchdog effects or busy borrows. Products
must inspect primary and cleanup outcomes, continue the real executor, and retain
quarantined resources. Platform start never starts a scheduler.

Physical Flash reports exact geometry and `program_flags`. An ECC fresh-erase
requirement belongs to the caller's provenance contract: all-FF readback cannot
prove unused ECC state, and a failed pulse requires another erase. All-FF source
units issue no pulse. A started pulse must settle before return; an expired
deadline prevents later pulses and never cancels hardware already programming.
Products own writable regions, voltage, execution stalls, watchdog and power loss.

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
