# Nexus embedded platform

Nexus is a C11 platform for industrial MCU software. It provides statically
assembled hardware access, explicit caller-owned resources and reusable components.
Applications and product policy live in [nexus-examples](https://github.com/X-Gen-Lab/nexus-examples)
or private product repositories.

The current implementation retains a typed factory and C polymorphic interfaces.
One authored TOML assembly creates fixed instances before execution; the factory
returns their stable identity, while public I/O calls dispatch to the selected
provider. Kconfig and runtime registries are no longer configuration authorities.

| Layer | Responsibility |
| --- | --- |
| `core/` | Status, monotonic deadlines, caller-owned requests and stable epochs |
| `arch/` | Compile-time Cortex-M/Native masks, privilege, exceptions, barriers and cycle snapshots |
| `soc/` | Exact chip memory, clocks, reset/startup, IRQs and controller drivers |
| `boards/` | Reviewed pin routes, oscillator facts, initial outputs and provenance |
| `io/` | GPIO, UART, SPI, I²C, physical Flash, watchdog, EXTI, PWM and ADC contracts |
| `os/` | Optional per-object baremetal, Native and static FreeRTOS adapters |
| `components/` | Bounded owner adapters, BMP280, log sinks, Modbus RTU and journals |
| `tools/` | TOML resolution, atomic output, CLI, resources, HIL and evidence |
| `tests/contracts/` | Executed behavior, fault, concurrency and tool boundary suites |

No default heap, registry, driver worker or maximum instance pool is introduced.
Products explicitly choose controller modes, queues, stack/TCB storage, component
ports and shutdown behavior. Polling SPI/I²C/Flash/ADC remains synchronous.
UART IRQ TX, finite UART/SPI DMA, UART IRQ/IDLE RX blocks and timer-triggered
ADC DMA blocks are implemented as explicit modes. DMA and wire completion remain
separate; cancellation preserves borrowed storage until proven settlement.
Continuous DMA RX, gap-free ADC sampling and product boot/update policy remain
outside the current support scope.

## First platforms

- STM32F407ZGT6: Qiming Xinxin V3.1 reference.
- STM32F407VET6: Lichuang Sky Youth reference.
- GD32F470ZGT6: Lichuang Liangshan reference.
- Native: deterministic or monotonic host models and real POSIX/FreeRTOS tests.

Each MCU has baremetal and FreeRTOS assemblies. Default physical Board assemblies
select reviewed LEDs and UART; additional wiring belongs in a reviewed external
Board package. Chip implementations, software fixtures and PCB qualification are
reported separately. **Physical board execution has not been performed.**

## Build and commit checks

```sh
git submodule update --init --recursive
python scripts/setup/install_dev_tools.py
.venv/bin/python -m pip install -r dependencies/environment-tools.txt
python tools/dev/dev.py doctor
python tools/dev/dev.py configure --preset native-debug
python tools/dev/dev.py build --preset native-debug --parallel 4
python tools/dev/dev.py test --preset native-debug
```

Use `stm32-qiming-baremetal`, `stm32-qiming-freertos`, `stm32-sky-baremetal`,
`stm32-sky-freertos`, `gd32-liangshan-baremetal` or `gd32-liangshan-freertos` for
MCU builds. Install the SHA-256-locked ARM GNU distribution with
`scripts/ci/install_arm_toolchain.py` and put its `bin` directory on `PATH`.
`native-release` and `native-asan` provide additional host contexts.

`NEXUS_ASSEMBLY_FILE` selects one schema-2 TOML input. SoC/Board facts remain
maintained JSON. The generated bundle contains `resolved.json`, `nexus_config.h`,
`nexus_bindings.h`, `nexus_factory.h`, binding C, linker memory and resource
budgets. Unknown options and resource conflicts fail configuration. The resolved
JSON is output, never an authored input. External applications consume
`Nexus::Platform` and `nexus_add_firmware()`.

The [derived capability matrix](docs/delivery/capabilities.md) separates SoC modes,
reference Board declarations, resolver fixtures and selected implementation
source. `doctor`, `check` and CI reject a stale generated matrix; source evidence
does not establish real hardware timing or qualification.

The unchanged root style and Doxygen conventions are enforced at commit time
and in CI. Hooks validate staged content, preserve unstaged work and never stage
or auto-fix files. Commit messages use Conventional Commits.

## Contracts and delivery

- [Architecture, engineering handbook and integration contracts](docs/design/README.md)
- [Software delivery status and reproducible acceptance](docs/delivery/README.md)
- [Team assignment and review preparation](tools/maintenance/README.md)
- [Source SDK packaging](cmake/package/README.md)
- [Contribution workflow](CONTRIBUTING.md)
- [Historical evidence](docs/archive/README.md)

Software checks bind source/configuration/tool identities and actual artifacts.
ELF budgets include physical load bytes and every supported RAM reservation.
Hardware timing, electrical behavior, watchdog accuracy and power-loss behavior
require a connected HIL station. Software candidate seals are unsigned and do not
promote a product release.
