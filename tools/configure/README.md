# One authored assembly, one resolved configuration

Write assembly **TOML schema 2**. Reviewed SoC and Board facts remain JSON. Python
3.11 or later supplies the standard-library TOML parser. The resolver validates
facts and the complete selected resource set, constructs an immutable typed IR,
and publishes one atomic compile/link bundle. CMake targets own the software
source graph.

```sh
python tools/configure/configure.py init \
  --board stm32f407_qiming_v31 --backend baremetal --clock hse8-pll168 \
  --output my-platform.toml
python tools/configure/configure.py list-bindings --assembly my-platform.toml
python tools/configure/configure.py check --assembly my-platform.toml
python tools/configure/configure.py explain --assembly my-platform.toml
python tools/configure/configure.py generate \
  --assembly my-platform.toml --output build/my-config
```

`init` creates an empty assembly with an explicit Board, backend and clock. It
validates that selection and refuses to overwrite an existing file. `check` and
`explain` have no generated-output side effects. `list-bindings` shows the actual
Board bindings and maintained modes; it does not invent connector wiring.

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
profile. `backend` is `native`, `baremetal` or `freertos`; Native cannot qualify
MCU execution. Optional `optimization` is `O2`, `Os`, `O3`, or a corresponding
`-lto` profile. Optional `abi` asserts the selected CPU facts. Optional `components`
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
| STM USART1 | `irq` bytes/events; `dma-tx` DMA2 stream7/channel4 with IRQ RX ring; `irq-blocks` TX IRQ and caller-owned IRQ RX blocks |
| GD USART0 | `irq` bytes/events; `dma-tx` DMA1 channel7/selector4 with IRQ RX ring; `irq-blocks` TX IRQ and caller-owned IRQ RX blocks |
| GD USART1 | `irq` bytes/events; `irq-blocks`; TX DMA is not maintained on this route |
| STM SPI1 | `short-poll`; `dma` finite full-duplex DMA2 stream3 TX/stream0 RX, selector3 |
| GD SPI4 | `short-poll`; `dma-full-duplex` finite DMA1 channel4 TX/channel3 RX, selector2 |
| GD SPI0 | `short-poll` |
| STM ADC1 | `single-shot`/`scan`; `trigger-dma` TIM3 TRGO and DMA2 stream4/channel0 |
| GD ADC0 | `single-shot`/`scan` fixed encoding7; `trigger-dma` TIMER2 TRGO and DMA1 channel0/selector0, fixed encoding1 (15 sample cycles) |

UART blocks requires `mode = "irq-blocks"` and `rx.format = "blocks"`.
`rx.capacity` is rejected: `nx_uart_port_rx_start` supplies the exact caller-owned
stream. `dma-tx` with RX blocks is rejected because those are different
implemented providers. An ADC `trigger-dma` table includes a nested `irq`
priority; `nx_adc_port_stream_start` supplies the runtime trigger frequency and
caller-owned blocks. The generator reserves no ADC block pool. Both ADC
providers stop after each complete block; explicit service resumes the next
free block, so these modes include gaps and do not promise continuous capture.

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
`trigger-dma`, rejecting simultaneous PWM use. CPU-only CCM/TCM is excluded from
immutable DMA domains. Silicon route support does not qualify PCB wiring.

SPI and I2C controller identity is separate from each child endpoint:

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
for normalization. It assists editor completion. The resolver additionally
checks hardware-specific routes, ranges, IRQ priorities, source identities and
resources. Schema completion does not establish a chip or PCB capability.

`ir.py` defines frozen `ControllerIR`, `EndpointIR`, `MemoryBudgetIR`,
`InterruptProfileIR` and `ConfigurationIR`. Controller options use frozen GPIO,
UART, SPI, I2C,
EXTI, PWM and ADC records. IRQ priority and kernel-call intent are a nested record;
ADC channel and sample sequences are tuples. Constructor emitters consume these
named decisions directly; reviewed hardware facts retain an immutable Mapping
projection. This does not make C factory ID aliases nominal strong types.
`resolved.json` is their JSON projection, with the same configuration identity.
There is no mutable configuration cache or generated software dependency graph.
The schema-1 JSON reader remains for explicit legacy contract fixtures and
migration measurements. Maintained assemblies and source-SDK examples use TOML;
JSON is never consulted as a fallback for a failed TOML assembly.

`providers/` has an explicit maintained map for Native, STM32F407 and GD32F470.
Each family package owns its implemented mode map, CPU/enum ABI, numerical limits,
fixed DMA/trigger routes, IRQ sharing and built-in CS rules. Its `bindings.py`
owns constructors, rollback and fixed IRQ wiring; `emission.py` owns system and
shared-clock lifecycle emission. The public resolver owns schema, reviewed facts,
input snapshots and resource conflicts. The public binding emitter owns Nexus
faces and common lifecycle progress. There is no package discovery or runtime
provider registration, and unknown families or unimplemented modes fail closed.

Adding a family requires a real capability and constructor contract in that map,
explicit CMake reconfiguration dependencies, source-SDK required files and tests.
CPU architecture, FPU, float ABI and enum ABI reach CMake from the validated
resolution. Arch and build options accept only maintained combinations; a new
non-Native family does not implicitly receive Cortex-M4 flags. These configuration
modules do not select software source files: CMake targets retain that authority.

CPU facts also declare the boolean `dwt_cyccnt` capability. Interrupt facts
declare `irq.priority_bits`; each maintained provider verifies the exact chip
values before resolving the logical priority range. Native retains a four-bit
model priority range without claiming a physical NVIC or DWT. Authored `[abi]`
continues to assert only architecture, FPU and float ABI, so an assembly cannot
override chip capabilities.

The selected FreeRTOS provider names its maintained `GCC/ARM_CM4F` port. That
port's immutable profile binds its CPU ABI and logical syscall ceiling; the
resolver verifies that the ceiling fits the chip's priority width before any
instance is emitted. The frozen IRQ decision supplies generated priority and
syscall macros, the kernel port selection and the Arch DWT compile fact. Baremetal
and Native have no kernel syscall ceiling. Unknown ports, contradictory CPU
facts and invalid OS-calling IRQ priorities fail closed. Adding a primitive
Arch implementation does not add a selectable SoC, ABI or kernel port.

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

The bundle contains `resolved.json`, `nexus_config.h`, `nexus_bindings.h`,
`nexus_factory.h`, `bindings.c`, `selection.cmake`, `memory.ld`,
`resource-budget.json`, declared input navigation and an ownership sentinel.
Every authored and fact input is hash-bound. CMake reads the resolver's exact
input paths for reconfiguration, including external packages; it never reparses
TOML. The resolved digest is embedded in MCU ELF. MSP and heap reservations have
explicit symbols and intervals; the stack is counted once.

A rejected reconfiguration invalidates the prior owned bundle before parsing.
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
