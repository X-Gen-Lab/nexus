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

## Vendor-first target and current implementation

Official hardware libraries are the default input for further provider work.
Reuse reviewed HAL transport when it satisfies the complete selected mode;
otherwise bind suitable LL or SPL primitives to the required bounded mechanism.
Nexus retains admission, borrowed storage, deadlines, result publication and
resource responsibility. Direct register helpers remain justified exceptions for
an exact SDK gap, erratum, atomic operation or demonstrated budget.

The selected F407 UART `dma-tx` provider uses official HAL finite TX with a
disjoint LL IRQ receiver under one private owner. Other F407 UART modes retain
their existing engines; G4, H7 and U5 retain their reviewed LL integrations.
GD32F470 compiles selected official SPL C units. Source selection, model tests,
ARM links and physical execution remain separate evidence. An include, primitive
call or finite-TX fixture alone does not establish a complete transport or family
qualification.

The [existing delivery-completion ledger](platform-delivery-completion.csv)
appends V-A01 through V-L08, V-P01 through V-P06 and V-X01 through V-X04 while
preserving the older rows and frozen results. Each new row starts with its real
implementation, software and physical status; a plan has no execution evidence.
The [derived ledger view](../status/ledgers/platform-delivery-completion.md)
uses the same renderer. No second support or completion database is introduced.

| Boundary | Required responsibility |
| --- | --- |
| Official transport | Own the selected peripheral transfer, IRQ/DMA and vendor state machine |
| LL/SPL primitive | Encode reviewed hardware operations without claiming a complete transport |
| Nexus adapter | Bridge admission, borrow, absolute deadline, errors, callbacks, drain and final settlement |
| Exact SoC | Bind SDK pairing, clock/reset, IRQ/DMA route, memory/Cache and silicon restrictions |
| Board/product | Supply reviewed wiring, electrical state, execution, storage, partitions and recovery policy |
| Static assembly | Construct existing contexts/faces and fixed IRQ connections; select one implementation before execution |

A private Handle can live in static provider storage. A public face still calls
its selected immutable ops with that instance context. No universal HAL manager,
name registry, default worker or extra forwarding-only layer is required. One
live controller behavior domain has one hardware transport owner. Selecting HAL
transport replaces the corresponding independent register transport; Handle
readiness and Nexus request lifetime remain different contracts.

### Complete maintained scope

The iteration covers the existing exact selections for STM32F103, STM32F407,
STM32G474, STM32H743, STM32U575, GD32F303 and GD32F470. The three launch Boards are
Qiming F407ZGT6, Sky F407VET6 and Liangshan GD32F470ZGT6. Current Sky bindings
cover LED, UART, Flash and IWDG; additional electrical routes need review before
physical execution.

The 71-entry assembly catalogue is one input, not the complete closure. Derive
navigation from the existing capability/provider/source catalogues, provider
fixtures, installed-SDK consumers, selected components and external examples.
The maintained `tools/maintenance/work_packages.py` joins existing authorities;
it does not execute tests or grant support. A mode needs its exact part/density,
Board/revision, route, backend/kernel policy, clock, format, IRQ/DMA,
memory/Cache/security/power profile and SDK pin reviewed together. CPU-only Arch
coverage and the independent ThreadX subset do not expand this SoC matrix.

The frozen source review at `89485943` preserves all 217 declared identities,
including its 33 unselected identities and 19 Native MODEL rows. Separate source
annexes now review those 33 declarations, seven components and seven system
owners. The fresh `5e82bab6` matrix selects 29 identities in 11 ARM images and
rejects four reserved G474 selections; it does not execute those images or change
the original inventory. Authorized private examples still need a separate annex.

F103/GD303 errata, F407/GD470 stream DMA, G474 DMAMUX and trigger coupling,
H743 Cache/domains/wire completion, and U575 GPDMA/retention/TrustZone retain
separate checks. A finite HAL UART result cannot qualify FIFO, continuous,
chained, duplex or cross-domain operation.

### Fixed clock admission

The reviewed software-trim gates run in cold preflight. An unexpected adjustment
in a gated profile is rejected before the startup transaction writes hardware or
acquires resources. Factory calibration remains independent and unchanged;
admission does not retune an oscillator. These checks add no work to the normal
I/O call path and establish no measured clock accuracy or physical timing
qualification.

F103 HSI8 and GD303 IRC8 require adjustment 16 in `RCC_CR.HSITRIM` and
`RCU_CTL.IRC8MADJ`, respectively. H743's exact-revision-V HSI admission requires
`RCC_HSICFGR.HSITRIM=64`; older fixed nominal profiles retain their existing
revision-specific boundary. U575's selected MSIS4 range requires
`RCC_ICSCR2.MSITRIM1=16`; its separate I2C1 HSI16 gate continues to require
`RCC_ICSCR3.HSITRIM=16`. The U575 selection follows the pinned official
HAL/CMSIS range-to-trim mapping and reset definitions.

F407 HSE8/PLL168, GD470 HXTAL25/PLL200 and G474 HSE24/PLL170 retain their
external primary clocks. Their internal-clock admission checks are
`RCC_CR.HSITRIM=16`, `RCU_CTL.IRC16MADJ=16` and `RCC_ICSCR.HSITRIM=64`,
respectively. They constrain the nominal bootstrap and fallback clocks; G474
also uses HSI16 for the selected I2C1 kernel. The G474 default of 64 follows
RM0440 Rev9, section 7.4.2, and the pinned HAL definition; the LL comment
stating 16 is stale.

### Production promotion gates

The selected F407 `dma-tx` implementation preserves the complete mode, including
IRQ RX bytes/events, frame/error/loss ordering, timestamps, wake and storage.
HAL owns finite TX DMA, the UART TC completion sequence and transmit abort;
the private provider owns their integration with the disjoint LL receiver and
Nexus lifetime. The older serialized finite-TX reference remains evaluation
evidence for its narrower contract, not the production mode's qualification.
Other modes require their own complete evaluation before replacement.

The serialized reference's `hal_depth` IRQ guard is not an MCU preemption design.
Production must establish allowed HAL nesting, short guards, pending-fact
handling and real IRQ service ordering. Do not mask all interrupts around vendor
calls that may wait. An abort callback can run synchronously in task context;
bridges record bounded facts and use notification permitted in the actual
context. Product callbacks and final settlement do not run arbitrarily inside
vendor callback stacks.

A Handle-only callback does not carry a request epoch. Before Handle reuse,
prove withdrawal of the previous hardware, IRQ and callback references; stamping
the current epoch onto a late callback is not proof of association. Establish
admission before a vendor call can retain references, and treat a startup error
after admission as accepted work requiring explicit withdrawal.

Review every reachable vendor wait, including bootstrap/runtime progression,
millisecond quantization, 32-bit tick wrap and low-power suspension. Initial UART
work keeps the existing system-clock and OS startup choices. Selected HAL modules
do not authorize default SysTick acquisition or interrupt-policy changes.
UART/SPI `uint16_t Size` parameters must match the selected public `size_t`
contract without truncation, hidden splitting or implicit bounce storage.

SPI evaluation preserves the whole CS interval, dummy/discard, fixed DMA
addresses and counts. GPIO needs one combined set/reset command; selected U5
GPIO commands now use private `HAL_GPIO_WriteMultipleStatePin` with the admitted
GPIO source closure. Separate set/reset calls cannot substitute. DMA disable
still needs actual readback and withdrawal proof before returning borrowed
storage.

Inspect each completion stack: F4 SPI DMA may wait for TXE/BSY; H7/U5 EOT hardware
close must preserve the selected quiet interval before disabling the shifter.
U575 HAL also contains its Device-gated persistent-DMAT workaround. These API
fits do not prove callback, deadline or loan withdrawal; review ordinary LL/SPL
setters separately from justified command/snapshot exceptions.

### Ordered implementation and acceptance

| Phase | Ledger work | Exit |
| --- | --- | --- |
| G0 | V-A: exact scope and contract | Every maintained selection has a bounded migration decision and baseline |
| G1 | V-B/V-C/V-D: dependencies and integration | Private SDK build, static contexts, resource responsibility, time and IRQ contracts are established |
| G2 | V-E and first GD32 work | Complete F407 UART behavior and selected GD32 official primitives execute their real regressions |
| G3 | V-F/V-G/V-H/V-I | Each applicable peripheral/family has a reviewed implementation and comparable cost; unused replaced production paths are removed |
| G4 | V-J/V-K | Test contexts, relocated SDK consumers, external examples and both documentation languages agree |
| G5 | V-L | Clean source P, examples E, dependencies, configuration and original artifacts are associated without inheriting old qualification |
| G6 | V-P/V-X | Connected physical execution and actual external environment/publication exits are observed separately |

Keep developer tests focused, execute the ordinary complete-index gate for each
functional increment, and run the complete qualification plan for the frozen
source. New behavior and fixes require actual RED then GREEN. Pure equivalent
refactors use the applicable regressions and output checks without fabricated
RED failures. Firmware remains C11; GoogleTest/GoogleMock remains host-only.
Preserve original attempts and compare complete providers, not only Handle sizes.
Without connected equipment, physical rows remain `not_executed`.

## Implementation boundary

The reviewed F4 dependency is STM32 HAL Driver v1.8.5 at
`b6f0ed3829f3829eb358a2e7417d80bba1a42db7`. Its CMSIS Device pairing comes from
STM32CubeF4 v1.28.3. The machine authority is
[the official-driver lock](../../dependencies/stm32f4-hal.lock.json), checked
against the selected Git dependency and imported SDK bytes. The selected API is
`stm32f407-ll-inline-eth-definitions-uart-dma-hal-tx`. Its 25-file closure retains
the earlier LL/ETH definitions and license, and adds the reviewed HAL headers
plus UART, DMA and RCC C units. GPIO/Flash declarations are required by RCC
compilation; their C implementations, PWR and global `stm32f4xx_hal.c` are not
selected. This lock is not a declaration that all HAL modules are supported or
validated.

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

The earlier development overlay added H743 `hal_driver_h7` to the family descriptor
and SoC source-lock projection, matching its existing pinned LL build input.
Private capture, advanced-PWM and UART-FIFO templates retain specialization
contracts; pin-state has optional implementation/default-macro scoping, while
CAN FD remains an ordinary guarded declaration. Separate U575 SPI byte ports
now take the selected controller and directly call private LL functions, with
no runtime forwarding. These changes do not rewrite the frozen source review.

### Bounded official-operation increments

The current development source uses locked LL setters for U575 PWM PSC/ARR/CCR1,
word RDR/TDR operations carrying UART byte values, and RTC write-protection keys
and Alarm A clear commands. Existing barriers, exact readbacks, IRQ/error order,
clock admission, timed phases and loan withdrawal remain in the same providers.
H743 SPI polling and DMA cleanup use controller-valued LL byte ports; masked
CSTART/CSUSP, quiet tail, Cache ownership and DMA/IRQ joins remain unchanged.

U575 GPIO now sends four existing command sites through the actual HAL GPIO C
unit: public write, initialization preload, stop and serial TX idle. Its private
15-file lock admits that unit and required headers; no HAL initialization or tick
is invoked. Disjoint authorized masks remain one BSRR command, and empty public
writes retain success without invoking HAL's nonempty-mask assertion.

H743 USART3 IRQ-byte RDR/TDR access uses locked LL word operations, including
the IRQ-byte companions of selected DMA modes. The provider retains saved-status
ordering, RX error/loss, TC completion and complete withdrawal. U575 watchdog
key, configuration and refresh commands use the locked LL IWDG header, including
watchdog-only assemblies without a GPIO source. Irreversible ownership precedes
activation; synchronization, exact readback, failed-update bounds and recovery
responsibility remain in the same C provider.

GD32F470 CAN and Ethernet verify 20 and 25 reviewed SDK register offsets in
isolated C11 compile-contract units. Each includes the real SDK before locally
expanding register macros as integer addresses. Runtime providers retain their mask and
IRQ checks and ordinary SDK register access. The contract units emit no firmware
code or data; no compiler-specific constant folding or scanner exception is
needed. Raw and enhanced Ethernet select the same MAC/DMA/VLAN/PTP offset
contract. Source compilation, linked-image costs and physical behavior remain
separate verification scopes.

Five SoC-private compile-time I2C ports bind F103/F407 receive and G474/H743/U575
receive/transmit to locked LL operations in the existing shared engines. They add
no runtime forwarder, vendor Handle, state or generated hardware algorithm.
Factory faces, public ABI, lifecycle and mode selection are unchanged. Actual
old-GREEN baselines and real-default probes remain development evidence;
final clean-source gates, complete SDK consumption and qualification are pending.

F407 uses official LL operations inside its existing provider state machines,
with one selected HAL transport for UART `dma-tx`. That transport retains request
admission, absolute deadlines, wire completion, cancellation, drain and quarantine.
It does not replace F407 IRQ-word, IRQ-block, RX-DMA or duplex providers. G4, H7
and U5 do not inherit F4 support or dependency qualification. GD32 remains a
separate provider.

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

## Selected F407 HAL UART transport

The same authored `dma-tx` mode selects a static private owner, immutable cold
configuration, one `UART_HandleTypeDef`, one `DMA_HandleTypeDef` and a shared
readonly Nexus ops table. The factory still returns the existing Nexus face;
neither vendor types nor callback registration become a public interface. The
private callback ABI sets `USE_HAL_UART_REGISTER_CALLBACKS=1` consistently for
the adapter and selected official C units. TX completion/error/transmit-abort/MSP
and TX half-transfer callbacks are explicitly bound per Handle, without a global
product callback override or runtime controller registry.

One provider owns both UART behavior domains. The LL receiver consumes RX
status/data once, then temporarily masks RXNE/parity/error enables while
`HAL_UART_IRQHandler` observes TX facts. HAL receive state remains idle, RX
buffer/size/count and `hdmarx` remain empty, and DMAR stays clear. This prevents
HAL error/abort paths from claiming the IRQ receiver. An unexpected receive
domain or Handle/route identity change closes admission and retains quarantine;
it does not adopt or abort a foreign DMA owner.

Admission establishes the request and payload borrow before a HAL call can
retain them or invoke a synchronous callback. Callbacks latch bounded facts;
task service alone withdraws transfer pointers and publishes final settlement.
DMA completion still differs from UART TC and from full reference withdrawal.
Cancel, timeout and deinitialization failures retain the request or cold lease
until the same owner proves actual drain. Stop checks peripheral enables,
DMA addresses/counts and pending/active IRQ references before releasing pins
or clocks. A Handle is reused only after this withdrawal; official `HAL_DMA_Init` restores
the reusable idle state where required, without assigning READY by hand.

The production IRQ gate saves the incoming CPU mask for short state changes.
If a relevant UART/DMA IRQ preempts a task HAL call, it disables only that
selected vector, leaves its hardware fact pending and restores the vector when
the retained HAL stack exits. It does not spin or let a level IRQ tail-chain
indefinitely inside that task call. Every `HAL_DMA_IRQHandler` entry requires EN readback clear, including an IRQ
with only HT or no observed error: an error can arrive after a flag snapshot.
While EN remains set, an HT-only event is acknowledged and HTIE disabled with
official primitives; its registered private no-op callback is not invoked and
no transfer state advances. Observed errors or inconsistent terminal facts
withdraw DMAT and request disable once. Refused disable masks that DMA vector
and quarantines the borrow; task service retries and dispatches HAL only after
EN is clear. This excludes the pinned handler's `SystemCoreClock/9600` wait
(17,500 iterations at 168 MHz) from the selected IRQ path.
This is a bounded-path design, not measured interrupt latency or WCET.

The target-private compile profile replaces CMSIS exclusive-retry RMW macros
with one saved-mask word update. This profile requires a single CPU owner of
the aligned UART control words and no NMI, fault/panic logger or other writer.
It preserves the incoming mask and leaves vendor source unchanged. The same
profile uses nearest OVER16 BRR rounding with overflow-safe quotient/remainder;
cold validation rejects zero baud or an out-of-field divisor before UART enable.
It does not grant dynamic clock or arbitrary format support.

Only the selected HAL UART/DMA/RCC source units enter the provider's private
build. `HAL_Init`, `HAL_InitTick` and a HAL SysTick handler are not selected.
`HAL_GetTick` reads `uint32_t` milliseconds modulo `2^32` from the existing live TIM2 epoch;
bootstrap retains its separate finite polling limits. `SystemCoreClock` follows
the last confirmed 16 MHz HSI or 168 MHz PLL observation, including partial
startup and cleanup failures. Official RCC frequency getters use the selected
prescaler tables rather than a fabricated constant clock.

The actual adapter/official-source model and clock tests establish their
recorded software scope. Generated factory execution, selected ARM/SDK links,
stack/cycle comparisons and the complete current-source gate remain separate
exits. Physical IRQ latency, wire timing and long-load HIL are not executed
without connected equipment. Do not infer a resource improvement from vendor
reuse or a passing model.

A paired development ARM firmware measurement uses the same F407ZG baremetal
`uart_rx_compact_assembly/f407-dma-tx.toml`, UART workload and `NX_UART_ID_UART1`,
GCC 14.3 M4F hard-float, `-Os`, no LTO and compiler cache off. Both builds add
measurement-only `-ffile-prefix-map=<each-source-root>=.` so different absolute
source-root spellings in retained `__FILE__` diagnostics do not bias Flash cost.
Their resolved configuration bytes are identical. The normalized pair is:

| Measurement (bytes) | Old LL (`9c0460b9`) | Selected HAL | Change |
| --- | --- | --- | --- |
| Flash LOAD / footprint | 12,592 | 18,188 | +5,596 |
| Static RAM | 248 | 456 | +208 |
| MSP reservation | 2,048 | 2,048 | 0 |
| Libc heap reservation | 0 | 0 | 0 |

The ordinary new build without that measurement-only option loads 18,200 Flash
bytes and uses 456 static RAM bytes. Both the ordinary build and normalized pair
fit this fixture's 64 KiB Flash and 32 KiB static RAM budgets; MSP remains a
separate 2,048-byte reservation and heap remains zero. This compares the complete
increment, including clock metadata and shared RX changes, not isolated HAL
overhead. The engineering decision favors reviewed official finite-TX reuse for
this F407 mode at the explicit cost; it is not a memory/performance win or a
mandate to use HAL for every family.

The observed final ordinary ELF retains no `HAL_Init`, `HAL_InitTick`,
`HAL_IncTick`, `HAL_RCC_ClockConfig` or `HAL_RCC_OscConfig`, and contains no
exclusive instructions. These are observations of this image, not all possible
callers. Stack high-water, execution cycles and physical behavior remain
unmeasured. Final-source or other-configuration costs require their own
ELF/map/resources.

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

The [SPI cost fixture](../../tests/contracts/io_storage_cost/spi_endpoints/README.md)
links 0/1/2/4 endpoints against the unchanged `89485943` Source SDK: development
LOAD/footprint is 3,048/7,880/7,940/8,064 B, with static SRAM 72/176/208/280 B,
plus 2,048 B MSP and zero heap. Nonzero images consume public controller topology;
the four-way image uses three CS banks, so its delta is not a linear endpoint
cost. These link observations do not establish timing, HIL or new-source
qualification.

The I2C leaf comparison retains direct V1 TX because the pinned LL helper adds a
DR read-modify-write, and retains V2 full CR2/ICR commands where vendor helpers
use read-modify-write. F103/F407 ADDR clear remains two ordered direct reads:
isolated ARM probes measured 6 B direct versus 18 B LL and 8 B extra stack.
GD470 hot byte/ADDR leaves retain direct operations after the narrow SPL
call/callee comparison; GD303 has no selected I2C SPL C unit. These isolated
translation-unit results are not whole-provider resource, stack-peak, timing or
hardware qualification.

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

Selected F407VE/ZG and GD470 offset images now have reviewed configuration,
startup, vector/linker and ELF-to-BIN boundaries; the
[assembly reference](../reference/configuration/assembly.md#physical-flash-and-the-selected-image)
distinguishes their image window from complete physical Flash. This is not boot
authentication, Flash access control or running-image handoff qualification.
The F407 handoff retains PRIMASK=1 and startup does not unmask it; an offset
FreeRTOS link cannot establish a valid FreeRTOS handoff.

Bootloader handoff, dynamic reclocking, deep STOP, broader H743 DMA and generic
U575 cross-domain I/O retain their separate unimplemented or partial exits in the
[SoC refinement ledger](soc-refinement.md). Official drivers do not automatically
supply the platform contracts for these features.
