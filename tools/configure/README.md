# One authored assembly, one resolved configuration

Write assembly **TOML schema 2**. Reviewed SoC and Board facts remain JSON. Python
3.11 or later supplies the standard-library TOML parser. The resolver validates
facts and the complete selected resource set, constructs an immutable typed IR,
and publishes one atomic compile/link bundle. CMake targets own the software
source graph.

```sh
python tools/configure/configure.py list-bindings \
  --board stm32f407_qiming_v31 --kind uart
python tools/configure/configure.py init \
  --board stm32f407_qiming_v31 --backend baremetal --clock hse8-pll168 \
  --profile uart --binding uart1 --instance link0 \
  --output my-platform.toml
python tools/configure/configure.py list-bindings --assembly my-platform.toml
python tools/configure/configure.py check --assembly my-platform.toml
python tools/configure/configure.py explain --assembly my-platform.toml
python tools/configure/configure.py explain --assembly my-platform.toml --json
python tools/configure/configure.py diff \
  --assembly my-platform.toml --against my-platform-next.toml
python tools/configure/configure.py generate \
  --assembly my-platform.toml --output build/my-config
```

`init` accepts `empty` (the default), `gpio` or `uart`. Every profile requires an
explicit Board, backend and clock; nonempty profiles also require a reviewed
`--binding`. `--instance` defaults to `device0`. GPIO selects output; UART selects
115200 baud, IRQ events with capacity 64 and priority 5 without kernel calls.
These are visible authored values, not hardware defaults. Normal resolution
rejects a profile where its exact mode or route is unavailable. It never changes
the selection to make it pass. Creation refuses overwrite and removes its own
incomplete file after a write or validation failure.

Private Boards use `init --board-package /path/to/package` instead of `--board`.
The command reads the package relative to the current directory and writes its
path relative to the new TOML file. Moving the product tree preserves that
relationship. Init, resolution and binding queries share the same directory
checks: ordinary `..` paths are allowed, but every original symlink component is
rejected before path normalization. The package must already contain reviewed
`board.json` facts; init never invents pin assignments or clocks.

`check`, `explain`, `list-bindings` and `diff` have no generated-output side
effects. `explain` now defaults to a human-readable summary of selected public
faces, clock, resources, memory domains, declared budgets and execution duties.
Automation that consumed its previous JSON output must add `--json`; that flag
retains the exact resolved IR projection. Budget declarations and provider limits
are not measured image, stack, throughput or latency results.

`list-bindings` accepts exactly one of `--assembly`, `--board` or
`--board-package`. Board-first queries need no invented backend or clock. Optional
`--kind uart` filters by public class. `--json` reports each route's actual
provider implementation, execution shape, DMA paths, normalized per-mode
`resource_keys` and `occupied_resources`. Board-first queries include SoC and
Board reservations; assembly queries also show the selected owners. A polling
mode does not acquire an alternative DMA route. Assembly queries verify the SoC
fact bytes against the resolved input hash before adding view-only metadata. A candidate remains
`admission = "configuration-required"`: clocks, backend, options and the complete
resource set must still pass normal resolution. Missing wiring stays missing.
The selected backend and clock are `null` in a Board-first query, not defaults.

`diff` compares effective platform, route, mode, storage, endpoint, clock and
budget decisions. Instance paths identify changed fields; `$order` preserves
meaningful declaration order. JSON changes include `before_present` and
`after_present` to distinguish a missing field from explicit `null`. Source-file
renames and configuration hashes alone do not count as effective changes. Both
inputs must resolve before a comparison is printed.

JSON `impacts` explains changes to declared storage, typed factory ID ordering,
public interfaces, transfer units/bounds/alignment and execution obligations.
Reordered numeric factory IDs require consumer recompilation and must not be
persisted as ABI. Changes to DMA or cancellation policy require reviewing the
product's service and shutdown sequence. No impact estimates an ELF saving.

Every command accepts `--diagnostics-json`. Configuration rejection then writes
one JSON object to stderr and returns 1; successful output is unchanged. CLI
argument syntax errors remain argparse errors with exit 2. Diagnostics retain
`code`, `message` and the command input `source`; known file-read errors identify
the file itself. Unknown/missing fields include `context`, `unknown` and
`missing`; resource conflicts include the physical `resource` and both complete
`claims` with owner, mode and source. `contexts` retains enclosing instance
information. Other validation failures retain `invalid_configuration` and the
precise original message; no guessed line numbers or replacement values are
reported.

If the selected provider entry or one of its parent Python packages is missing,
the diagnostic code is `provider_module_missing`, with `family`, `module`,
`source` and `missing_module`. Only the selected family is loaded. A missing
dependency inside an existing entry, or another import error, retains its actual
exception; it is not reported as an unsupported provider.

```sh
python tools/configure/configure.py provider-check \
  --assembly my-platform.toml --expect-family stm32f407 \
  --expect-part STM32F407ZGT6 --json
python tools/configure/configure.py check \
  --assembly my-platform.toml --diagnostics-json
```

`provider-check` resolves the exact input and runs the production static emitter
in a disposable directory. Optional identity assertions reject a different
family or part. The report lists selected controller faces/implementations,
reviewed inputs, explicit configuration modules and generated-file SHA256s.
Its checks are only `resolved_typed_ir` and `static_bundle_emitted`; complete
compile/link, GoogleTest/GoogleMock and applicable station validation still use
the maintained gates. This does not register a provider or create hardware facts.

Human views and binding JSON report physical scope as
`not_established_by_configuration` for MCU or `not_applicable` for Native. Consult
the exact Board and station records for physical evidence. The legacy raw IR
qualification placeholders describe the generator's scope; they cannot establish
the absence or presence of a separate station result.

```toml
schema = 2
board = "stm32f407_qiming_v31"
backend = "baremetal"
clock = "hse8-pll168"
optimization = "Os"

[memory]
main_stack_bytes = 2048
libc_heap_bytes = 0
static_ram_limit_bytes = 32768
flash_load_limit_bytes = 65536

[gpio.indicator]
binding = "led0"
mode = "output"

[uart.link0]
binding = "uart1"
mode = "irq"
baud = 115200

[uart.link0.rx]
format = "events"
capacity = 64

[uart.link0.irq]
priority = 5
calls_os = false
```

`board` names a maintained directory immediately under `boards/`. An external
package uses the explicit alternative `board_package = "path/to/package"`;
relative paths are relative to the authored file. Exactly one of these fields is
required. `clock` must match the Board oscillator and the maintained SoC clock
profile. `backend` is `native`, `baremetal`, `freertos` or explicitly selected
`threadx`; Native cannot qualify MCU execution. ThreadX has its own maintained
Native/M4 subset, strict profile and bootstrap contract; it does not inherit
FreeRTOS CPU coverage. See [ThreadX integration](../../docs/design/threadx.md). Optional `optimization` is `O2`, `Os`, `O3`, or a corresponding
`-lto` profile. Explicit `Og` and `O0` support source stepping; neither accepts an
`-lto` suffix. `native-step-debug.toml` selects `Og`; the ordinary Native debug
assembly retains its production-oriented `Os` choice. Optional `abi` asserts the selected CPU facts. Optional `components`
selects explicitly mapped CMake targets.

Tables select only the required instances. Each kind table contains instance
IDs, a Board `binding`, a maintained `mode` and its bounded options. UART `irq`
authoring normalizes to the maintained `irq-byte-event` provider mode. UART RX
byte storage and timestamped error-event storage are different profiles. EXTI
and SPI IRQ options use a nested `irq` table where the selected mode uses an IRQ.
Unknown fields, mismatched kind/binding, unsupported modes and unreviewed routes
are rejected. No mode or resource is silently substituted.

| Kind | Options beyond binding and mode |
| --- | --- |
| GPIO | Board pin mask and safe initial level |
| UART | `baud`; RX `format`, ring `capacity`; IRQ `priority`/optional `calls_os` |
| SPI | Controller `max_hz`; explicit device CS, mode and speed |
| I2C | Reviewed 100 kHz bus; explicit nonreserved seven-bit addresses |
| Flash | Exact physical geometry; product reservations stay external |
| Watchdog | Construction leaves the irreversible watchdog disabled |
| EXTI | `edge`, `event_capacity`, nested IRQ priority |
| PWM | `period_ticks`, `duty_ticks`, exact-divider `tick_hz` |
| ADC | `channels`, `sample_times`, `reference_mv`; GD32 `timeout_ms`; DMA IRQ priority |

Implemented MCU modes are selected explicitly:

| Controller | Modes and exact sources |
| --- | --- |
| STM USART1 / GD USART0 | IRQ bytes/events, `dma-tx`, `irq-blocks`, `rx-dma-idle`, `dma-duplex`, `rx-dma-continuous`, `dma-duplex-continuous` |
| STM USART2 | Finite `dma-tx` on its reviewed route |
| GD USART1 | IRQ bytes/events, `irq-blocks`, finite `dma-tx` |
| STM SPI1 / SPI2 | SPI1 `short-poll`; both controllers finite `dma` |
| GD SPI0 / SPI4 | `short-poll` and finite `dma-full-duplex` |
| STM I2C1 / I2C2 | I2C1 `poll-messages`; both controllers `irq-messages` / `dma-messages` |
| GD I2C0 / I2C1 | `poll-messages`, `irq-messages`, `dma-messages` |
| STM ADC1 / GD ADC0 | `single-shot`, `low-rate-scan`, finite `trigger-dma`, separate `trigger-dma-continuous` |
| STM CAN1/2 / GD CAN0/1 | `classic-irq`, shared filter domain |
| STM OTG FS / GD USBFS | `device-fs`, PIO endpoints and explicit USB48 clock owner |
| STM ETH / GD ENET | `raw-rmii`, MDIO and descriptor/RX loans |
| STM TIM4 / GD TIMER3 | `edge-capture` |
| STM TIM1 / GD TIMER0 | `complementary-pwm`, deadtime and break |
| G474 GPIOA / LPUART1 | Narrow GPIO and `irq-bytes` baseline; no DMA fallback |

Exact routes, source and software fixture coverage are maintained in the
[derived capability matrix](../../docs/delivery/capabilities.md). A SoC mode
alone does not install a PCB connector, transceiver or external device.

UART `irq-blocks` requires `rx.format = "blocks"`; `rx.capacity` is rejected
because `nx_uart_port_rx_start` supplies caller-owned stream storage. TX DMA with
IRQ blocks is rejected: explicit duplex modes select a different provider.
Finite RX/ADC modes include rearm gaps. Continuous modes use distinct
caller-loan/replacement contracts, stop on backpressure and never overwrite a
consumer. The generator reserves no block pool. Mode-specific IRQ and DMA
resources remain mandatory.

DMA conflict checks use the physical controller and stream/channel, independent
of the request selector. Selected fixed providers validate the complete exact
TX/RX DMA tuple against their production implementation; syntactically valid
alternative streams/selectors and missing trigger timer claims are rejected.
`timer:TIM3` and `controller:TIM3` identify the same physical timer, including
Board reservations and PWM ownership. Runtime timebase reservations belong to
the SoC: STM TIM2 and GD TIMER1 plus their IRQ remain reserved for external
Boards too; a Board may restate that reservation without releasing it.
STM DMA2 and GD DMA1 clocks are shared platform-owned
resources; shutdown retains them until every selected request and block loan
drains. `mode_resources` in reviewed route facts reserves TIM3/TIMER2 only for
`trigger-dma` and `trigger-dma-continuous`, rejecting simultaneous PWM use. CPU-only CCM/TCM is excluded from
immutable DMA domains. Silicon route support does not qualify PCB wiring.

The exact G474 `shared-fixed-pwm` selection keeps two ordinary `[pwm]` instances
and their existing fields. The complete TIM3 CH1/PA6 and CH2/PA7 AF2 pair must
agree on tick and period. The resolver derives one immutable physical owner,
compatible base claims and exclusive lane/pad claims; generation initializes
and closes that owner once while preserving both original PWM factory IDs.
Missing members, a different base or another whole-TIM3 personality reject.
Ordinary `fixed-pwm` remains exclusive. A running peer is joined at its current
phase, without a coherent group-start promise. The
[canonical software input](../../cmake/package/verification/io_evolution/g474-pwm-shared.toml)
does not qualify PCB routing, electrical timing or HIL.

SPI and I2C controller identity is separate from each child endpoint:

<!-- nexus-toml-fragment: endpoint tables extend a complete Board assembly. -->

```toml
[spi.bus0]
binding = "spi_flash"
mode = "short-poll"
max_hz = 1000000

[spi_device.nor0]
controller = "bus0"
# This reviewed GD32 route contains the fixed PF6 active-low CS.
mode = 0
max_hz = 1000000
```

A route without a built-in CS requires an explicit reviewed `cs_binding`. I2C
uses `[i2c_device.sensor]`, `controller = "bus0"`, and `address = 0x76`. Endpoint
`driver` defaults to `spi-endpoint` or `i2c-endpoint`; `bmp280` requires its
maintained SPI component. Drivers do not create component instances or workers.
An endpoint has no queue capacity. Optional Native-only `model_bytes` allocates
an exact independent register-model buffer, between 1 and 256 bytes. Native I2C
requires it; Native SPI without it explicitly uses the loopback model.

`assembly.schema.json` is generated from the same `authored.py` field model used
for normalization. It assists editor completion with units and lifecycle duties.
Descriptions add no independent acceptance rules. The resolver additionally
checks hardware-specific routes, ranges, IRQ priorities, source identities and
resources. Schema completion does not establish a chip or PCB capability.

The ordered `CONTROLLER_CONTRACTS` declaration in `contracts.py` owns common
category fields and header facts. Category enumeration, graph-envelope field
admission and common IRQ normalization derive from those existing declarations;
mode-specific options remain explicit. Typed option constructors, provider mode
contracts and cross-resource arbitration retain their own responsibilities.
This reduces repeated category lists without adding another configuration
authority or firmware registry. CMake continues to own compiled sources.

`authored.py` parses the sole platform input, TOML schema 2, into frozen
`AuthoredAssembly`: unresolved user intent, not admitted hardware. The production
entry `resolve_ir()` validates exact facts, selections and conflicts before
returning `ConfigurationIR`. Its frozen named controller, endpoint, CPU/kernel,
memory and resource records are the sole stored decisions. They do not implement
the dictionary interface or retain a second `values` mapping. Nested IRQ options
are records; ADC channel/sample sequences are tuples. Reviewed JSON hardware facts
and keyed indexes remain explicitly immutable leaf mappings; temporary resolver
drafts remain local validation data. This is not a claim that every JSON key has
its own nominal Python class or that C factory ID aliases are nominal strong types.

Private typed source epoch 2 and serialized projection schema 1 are distinct.
`to_dict()` derives `resolved.json` for audit, diagnostics and independent artifact
checks; serialization does not become another configuration input. `source_fields`
records projection-field presence, not a duplicate set of decision values.
There is no mutable configuration cache or generated software dependency graph.
Legacy JSON platform assemblies are rejected, including former production
fixture entry points. Maintained test fixtures author TOML through test-only
helpers. Historical reports keep their original bytes and read-only verification;
they cannot reenable a legacy platform input.

`providers/` explicitly admits Native, STM32F103/F407/G474/H743/U575 and
GD32F303/F470. Each retains its documented exact part, route and mode scope.
Production family packages live in `soc/<vendor>/<family>/configure/`; Native
uses `soc/native/configure/`. The explicit map, `FamilyDescriptor` and shared
leaf contracts remain in `tools/configure/providers/`; provider tests remain
in `tools/configure/tests/providers/<family>/`. Common parsing, resource
arbitration, typed IR and atomic bundle publication remain in this tool directory.

Each family's `configure/contract.py` owns its mode map, CPU/enum ABI, numerical limits,
fixed DMA/trigger routes, IRQ sharing and built-in CS rules. `__init__.py`
reexports the explicit contract and constructors; sibling implementations import
`contract.py` directly rather than the partially initialized package. `bindings.py`
owns static construction descriptions and fixed IRQ wiring; `emission.py` supplies
static system/shared-clock bindings and explicit call ordering. Concrete C11
adapters own hardware admission, register access/readback, preparation, stop,
rollback and recovery. Each constructor returns the frozen
`Construction` contract with explicit `state_type`, `definitions`, `initialize`,
`stop`, `retain_on_failed_init`, `exports` and `pin_snapshots`. The common emitter
consumes those named fields instead of inferring private C types or cleanup
responsibility from family names. `initialize` is a `DirectCall`; `stop` is a tuple
of direct calls. Their arguments admit only explicit `Symbol`/`Address` references,
uint32 constants, booleans, NULL and nested direct calls. They reject free-form
expressions, casts, assignments and function-pointer/member-call expressions.
The actual C compiler still validates symbol declarations and parameter types.
This is a narrow call
representation, not a complete C AST: `definitions`, `exports` and
`face_definitions` contain declarations, constant configuration, storage and
interface faces. Generated thin IRQ entries and Platform start/stop preserve
explicit typed call ordering; they do not implement peripheral or clock register
algorithms. The record exists only during generation;
no runtime registration or additional dispatch is emitted. The public resolver owns schema, reviewed facts,
input snapshots and resource conflicts. The public binding emitter owns Nexus
faces and common lifecycle progress. There is no package discovery or runtime
provider registration, and unknown families or unimplemented modes fail closed.

Family packages also own endpoint capability policy, exact backend/clock coupling,
cache-derived mode fields, whole-selection exclusions and optional compile-time
constants. The resolver invokes these explicit contracts while retaining input
identity, schema validation, transaction assembly and resource arbitration.
G474's ADC review and shared-PWM owner are explicit family helpers; their exact
timer choices do not live in the generic resolver. `FamilyDescriptor` lists
configuration helpers directly, while CMake alone owns compiled sources. Public
mode traits describe execution shape and never admit a family by themselves.
This refactor adds no firmware registration, allocation or dispatch layer.

Dependency direction stays explicit: `providers/common.py` contains leaf input
checks and frozen contracts; cross-domain option/clock/IRQ validation lives in
`selection.py`. `ir.py` owns frozen decisions and their one-way audit projection. DMA region
rendering lives with memory policy in `memory.py`; provider bindings do not
import the top-level emitter. Preflight receives the selected H743 DMA admission
callback from its emitter instead of importing that emitter back. Derived
support catalogue orchestration lives in `matrix.support_catalogue`; selected
contract descriptions never import the resolver. These are build-time boundaries
and do not alter the static typed factory or public C ABI.

Adding a family requires a real capability and constructor contract in that map,
explicit CMake reconfiguration dependencies, source-SDK required files and tests.
CPU architecture, FPU, float ABI and enum ABI reach CMake from the validated
resolution. Arch and build options accept only maintained combinations; a new
non-Native family does not implicitly receive Cortex-M4 flags. These configuration
modules do not select software source files: CMake targets retain that authority.

`cpu.py` is the shared strict CPU profile resolver. Reviewed CPU facts declare
architecture, DSP, FPU, float ABI, DWT cycle counter, MPU format, instruction
and data cache line sizes, execution security state, accessible SAU and MVE
variant. Every optional capability must be present as a fact; a CPU name does
not enable optional hardware. IRQ facts declare priority width and actual
external-vector count. The maintained SoC providers verify the exact chip values
before resolution. Native retains a four-bit model range and zero external
vectors without claiming a physical NVIC or DWT. Authored `[abi]` asserts only
architecture, FPU and float ABI; an assembly cannot override chip capabilities.

The resolver accepts CPU software profiles for Cortex-M0, M0+, M3, M4, M7,
M23, M33, M55 and M85. It produces one immutable ABI, compiler flag list,
Arch capability set and IRQ/kernel-port decision. Hardware capability, software
component support and physical qualification remain separate: the full assembly
providers support Native, STM32F407, GD32F470 and the narrow STM32G474
GPIO/LPUART baseline. This does not establish all-peripheral or physical
qualification for G474.

The required `cpu.dsp` fact is a strict boolean. M33 can include or omit DSP;
`false` emits `-mcpu=cortex-m33+nodsp`, while `true` enables its reviewed DSP
instruction set. Native, M0/M0+/M3/M23 require `false`; M4/M7/M55/M85 require
`true` because their DSP extension is always present. The generated
`NEXUS_CPU_HAS_DSP` macro has the same value. Missing or contradictory facts are
rejected. An assembly cannot infer DSP from compiler defaults or override it.

| CPU execution profile | FreeRTOS context port | Mask contract |
| --- | --- | --- |
| M0 / M0+ | `GCC/ARM_CM0` | PRIMASK |
| M3 / M4 without FPU | `GCC/ARM_CM3` | BASEPRI |
| M4 with FPU | `GCC/ARM_CM4F` | BASEPRI |
| M7 without FPU | owned `nexus/ARM_CM7_integer` erratum overlay | BASEPRI |
| M7 with FPU | `GCC/ARM_CM7/r0p1` | BASEPRI |
| M23 | `GCC/ARM_CM23_NTZ/non_secure` | PRIMASK |
| M33 / M55 / M85 | corresponding `GCC/ARM_CM*_NTZ/non_secure` | BASEPRI |

The v8 NTZ ports execute one declared security state and do not introduce Secure
gateway calls. Secure execution emits `-mcmse`; NonSecure and single-state
builds do not. FPU/MVE save contracts follow exact feature facts. Integer MVE
without an FPU uses softfp ABI because GNU soft ABI disables MVE. An absent
MVE/FPU on M55/M85 emits explicit instruction exclusions. CPU flags reach compile
and link from this resolver, with no second CMake CPU/FPU table.

Mainline FreeRTOS uses a maintained logical syscall ceiling of 5, or 10 for
eight implemented priority bits to meet the port's even-priority requirement.
Baseline FreeRTOS uses PRIMASK and a logical ceiling of zero. Priority width,
actual external-vector count, mask kind and syscall range are independent facts.
M0/M0+/M23 have exactly two implemented priority bits and four priority levels;
Mainline profiles permit three to eight bits, narrowed by the actual SoC fact.
The processor limits are 32 external IRQs for M0/M0+, 240 for M3/M4/M7/M23,
and 480 for M33/M55/M85. System exceptions are excluded: 496 total exceptions
on M33/M55/M85 is not 496 external interrupts. CMSIS register-array capacity
does not establish the processor limit. The actual count remains a required
SoC fact and cannot exceed that processor limit; the limit is never a default
claim that a chip implements every vector.
Baremetal and Native have no kernel syscall ceiling. Invalid widths, counts,
optional-feature combinations and unsupported backends fail closed.

External SoC packages consume `cpu.resolve` and its immutable `CpuProfileIR`.
The public CPU/Core/Arch/OS runtime entry consumes a separate authored runtime
TOML through `runtime.py`, using the same resolver and `emit_profile`. It emits
no device factory, pin route, startup, SoC driver or memory layout. The machine
CLI `cpu.py --facts cpu.json --backend baremetal --output build/cpu-config`
accepts a strict fact package containing `schema_version = 1`, CPU/IRQ records
and a positive uint32 `clock_hz`. Its owned output is published atomically;
source parents, symlink paths and unowned output are protected. The CLI does not
replace the single authored TOML entry used by CMake.

Generated private storage has separate role namespaces: `s_nx_port_*`,
`s_nx_endpoint_*`, `s_nx_cs_*`, receive/model buffers and platform lifecycle state.
Public `static const` faces use `s_nx_face_*` and `s_nx_device_face_*`, containing
one shared ops pointer and one private context pointer. Native generated instances
never alias singleton model fixture storage.

The factory is a **pure static typed getter**. `nexus_factory.h` declares
assembly-local `uint16_t` ID types, named constants such as `NX_UART_ID_LINK0` and
`nx_factory_uart(nx_uart_id_t)`. A valid ID returns its immutable face; an invalid
or count ID returns NULL. SPI/I2C controller getters return controller faces;
`nx_factory_spi_device` and `nx_factory_i2c_device` return explicit child faces.
Lookup performs no initialization, allocation, locks, string search or lifecycle
changes. Factory IDs are assembly-local source interfaces, not a persistent or
binary protocol ABI. Selected instances remain part of the selected platform.

Consumers may include the generated `nexus_factory_strong.h` to use nominal
references: `nx_gpio_ref_t`, `NX_GPIO_REF_LED_A` and `nx_factory_gpio_ref`.
C11 and C++17 reject a UART reference passed to a GPIO getter, including raw
scalar IDs. These inline wrappers call the existing static getters; they add
no registry, hardware initialization or instance storage. The scalar getters
remain available without including the optional wrapper header.

The optional `[factory]` table accepts `capabilities = true`. It emits selected
read-only `nx_factory_io_capability_t` records and pure
`nx_factory_<class>_capability` getters. Descriptors describe the actual selected
mode, transfer units, unit size, alignment and message bound. A zero upper bound
means **not described**, not unlimited. Native records carry
`NX_FACTORY_CAP_MODEL` and do not assert physical IRQ or DMA capability. Invalid
IDs return NULL. Descriptors stay separate from interface faces and hot I/O
dispatch; the default assembly does not generate them.

The bundle contains `resolved.json`, `nexus_config.h`, `nexus_bindings.h`,
`nexus_factory.h`, `nexus_factory_strong.h`, `bindings.c`, `selection.cmake`, `memory.ld`,
`resource-budget.json`, declared input navigation and an ownership sentinel.
`configuration_code_paths.json` registers the maintained Python generation
inputs with CMake. `generator-identity.json` separately binds their source bytes;
the configuration digest describes resolved decisions, rather than a compiler,
toolchain, environment or generator version. Configuration code is snapshotted
before resolution and checked after resolution and emission. When the family is
not yet known, the initial snapshot covers every explicitly maintained family;
the published identity retains the selected family closure from that snapshot.
Source SDK verification remains complete and independent of these sidecars.

Every authored and fact input is hash-bound. CMake reads the resolver's exact
input paths for reconfiguration, including external packages; it never reparses
TOML. The resolved digest is embedded in MCU ELF. MSP and heap reservations have
explicit symbols and intervals; the stack is counted once.

A nonblocking OS file lock admits one writer per output. Inputs, source fact
packages and symlink paths are protected before any withdrawal; an ownership
marker never authorizes deleting Board, SoC or other input facts. After these
safety checks, the previous owned bundle moves to a private sibling before
resolution. The public output is absent until atomic publication. Failed
generation removes private output; an interrupted process leaves a bounded
owned pending/previous pair for recovery under the same lock. Stable lock files
remain in the build directory so two writers cannot lock different inodes.
Successful generation preserves each byte-identical file's timestamp. Changing
only a budget therefore preserves unrelated factory headers. No old output is
used as a configuration or validation cache.

Generation failures leave no usable partial output. Duplicate TOML tables/JSON
keys, bool-as-number, overflow, unsafe C identifiers, exact-part/ABI/pin conflicts,
invalid addresses, unknown drivers and changed input bytes fail closed. DMA
channels are request selectors: different channels on one physical stream still
conflict. Shared STM32 EXTI vectors have one bounded static dispatcher and require
identical priorities. OS-calling IRQs must respect the FreeRTOS syscall ceiling.

`nx_platform_start` constructs selected hardware explicitly and creates no
scheduler or product worker. Obtaining a factory face before startup is safe;
using its device requires successful startup. Stop requires quiesced direct
writers and drains selected interfaces in reverse order. BUSY/error retains
contexts, clocks and cleanup progress. An enabled watchdog remains an explicit
irreversible effect. Factory pointers never revoke storage ownership on cancel,
timeout, shutdown or restart.

Run `python -B -m unittest discover -s tools/configure -p 'test_*.py'` for resolver
behavior. GoogleTest/GoogleMock exercise compiled generated factories and provider
lifecycle behavior. ARM link fixtures under `tests/contracts/{stm32,gd32}_assembly`
qualify selected software routes; physical hardware status remains unexecuted
until an attached station performs HIL.

`assemblies/catalogue.json` explicitly lists maintained public and Native
references. `python -B tools/configure/matrix.py --check` checks authoring,
resolver, factory, provider modes, component target selection and every listed
assembly. `--output path.json` emits the derived software support matrix with
exact selections and `physical_status = not_executed`. Adding an assembly
requires a catalogue entry; a private integration fixture does not become a
public Board route or physical qualification through this check.

Generated Platform headers also provide optional caller-owned lifecycle
diagnostics through `nx_platform_start_diagnosed` and
`nx_platform_stop_diagnosed`. A null output preserves original start/stop
behavior. Phases identify context, system, resources or a controller; controller
indices refer to the zero-based `resolved.json.controllers` order, not per-kind
factory IDs. No output pointer is retained. Original primary/cleanup results
and remaining effects remain authoritative. See
[the lifecycle diagnostic contract](../../docs/design/official-stm32-provider.md).
