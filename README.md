# Nexus Embedded Platform

English | [中文](README_CN.md)

Nexus targets industrial control and connected devices through a common HAL/OSAL,
separate SoC/board/product profiles, and traceable engineering and delivery.
Maintained implementations include Linux Native, STM32F407VG Discovery,
STM32F407ZGT6 Qiming high-spec V3.1, STM32F407VET6 Sky Youth, and GD32F470ZGT6
Liangshan Pi. Each MCU board has baremetal and FreeRTOS configurations.

| Profile | Implementation/evidence | Qualification boundary |
|---|---|---|
| Linux Native / GCC | Full targets build; software contracts and application checks | Host peripheral models |
| FreeRTOS POSIX port | Pinned real kernel and OSAL contract execution | Does not validate Cortex-M interrupts/timing |
| STM32F407VG / MB997 | Existing implementation and real ARM build baseline | New changes require separate evidence; physical HIL pending |
| STM32F407ZG Qiming V3.1 / F407VE Sky Youth | Separate wiring, actual density and Flash partitions | Physical HIL and actual PCB revision confirmation pending |
| GD32F470ZG Liangshan | Official SDK 3.3.3; independent startup/clock/IRQ/GPIO/UART/SPI/time/Flash | Software/ARM evidence is separate from board qualification |
| Windows / macOS | Native presets retained | Not executed in this maintenance run |

The [support matrix](docs/strategy/support-matrix.yaml) defines promotion evidence.
Enterprise support and LTS are not currently promised.

## Build and verify

Use CMake 3.21+, Python 3.11+, and a C11/C++17 compiler. The default Native
security services require OpenSSL 3 development headers/libraries; the minimal
product does not. ARM uses the pinned complete ARM GNU 14.3.rel1/newlib toolchain.
Configuration does not download dependencies: initialize the pinned submodules.

```sh
git clone https://github.com/X-Gen-Lab/nexus.git
cd nexus
python -m pip install kconfiglib==14.1.0
git submodule update --init ext/googletest ext/freertos vendors/arm/CMSIS_5 vendors/st/cmsis_device_f4 vendors/st/stm32f4xx_hal_driver
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --parallel 4
ctest --preset linux-gcc-debug --output-on-failure --no-tests=error --parallel 4
python -m unittest discover -s scripts/ci -p 'test_*.py'
```

Linux packages include GCC, CMake, Ninja and libssl-dev. List other presets with
cmake --list-presets. Their presence is not cross-platform execution evidence.

Each build owns one generated bundle: effective.config, nexus_config.h and
config.cmake. Presets own compiler/build mode; Kconfig fragments own device and
resource selections. Source-root .config and generated headers are retired.
Unknown/conflicting/out-of-range settings and generation failures stop the build.

```sh
python scripts/ci/install_arm_toolchain.py --install-dir build/toolchains/arm-gnu-14.3.rel1
export PATH="$PWD/build/toolchains/arm-gnu-14.3.rel1/bin:$PATH"
cmake --preset stm32-armgcc-release
cmake --build --preset stm32-armgcc-release --parallel 4
cmake --preset stm32-armgcc-freertos-release
cmake --build --preset stm32-armgcc-freertos-release --parallel 4
```

| Board | Baremetal Release preset | FreeRTOS Release preset |
|---|---|---|
| Qiming high-spec V3.1 | `stm32-qiming-armgcc-baremetal-release` | `stm32-qiming-armgcc-freertos-release` |
| Sky Youth | `stm32-sky-armgcc-baremetal-release` | `stm32-sky-armgcc-freertos-release` |
| Liangshan Pi | `gd32f470-armgcc-baremetal-release` | `gd32f470-armgcc-freertos-release` |

Replace `release` with `debug` for each development profile. ELF/map/bin/hex use
the selected build's bin directory. Match silicon, actual PCB revision, supply,
partition layout and effective configuration before flashing. Consult the
[platform delivery record](docs/implementation/platform-delivery.md) for execution
scope and source/artifact identity. Physical HIL has not been executed.

## Runtime contracts

- Independent Arch saved interrupt state and barriers; typed HAL references,
  explicit open/close/recover, UART tickets and buffer leases.
- Product-owned board/backend/services/budgets and boot/rollback. FreeRTOS uses
  scheduled application workers and six static object pools. Source SDK products
  consume `Nexus::Product`.
- STM32 SPI bus/device/transaction separation, bounded asynchronous queues,
  cancellation and DMA drain before buffer release.
- Baremetal exposes an honest main-loop backend; unsupported scheduling,
  event and software timer operations fail explicitly.
- Dual-bank atomic snapshots on a real Flash port; persistent Native file-flash
  simulation. RAM remains volatile.
- A serialized Config management owner and authenticated AES-GCM records using
  maintained crypto providers. The MCU crypto provider and entropy source are
  not integrated; required crypto operations return unsupported.
- Authenticated update policy and recoverable trial/confirm/rollback metadata.
  Product bootloader, protected vault and hardware evidence still need binding.

```sh
build/linux-gcc-debug/bin/blinky --cycles 3
cmake --preset native-services-debug
cmake --build --preset native-services-debug --parallel 4
build/native-services-debug/bin/freertos_demo --run-ms 500
```

Read the implementation records for [build/config](docs/implementation/build-config.md),
[drivers](docs/implementation/platform-drivers.md),
[storage/security](docs/implementation/storage-security.md),
[update](docs/implementation/update.md) and
[reference applications](docs/implementation/reference-applications.md), plus
[integrated validation and live CI](docs/implementation/integration-validation.md).
Software fault injection is separate from physical power-loss, electrical and
control-deadline measurements.

## Enterprise workflow

The [architecture and workflow plan](docs/strategy/README.md) covers the
approximately ten-person allocation, 3–6 month roadmap, requirement/test
traceability, risk review, dependencies and release gates.
The [backlog](docs/strategy/backlog.csv) distinguishes implementation evidence,
pending qualification and external blockers.

A release candidate binds the same source commit, effective configuration,
dependency identities, artifact digests and nonzero tests. Production also
requires matching board HIL, signing identity, measured budgets and manufacturing
evidence. Missing evidence cannot become a successful production gate.

Read [AGENTS.md](AGENTS.md) before changing public contracts, persisted formats or
the build graph. Nexus uses the [MIT license](LICENSE); dependencies retain
their individual licenses.
