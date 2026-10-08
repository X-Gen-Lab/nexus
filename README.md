# Nexus Embedded Platform

English | [中文](README_CN.md)

Nexus targets industrial control and connected devices through a common HAL/OSAL,
separate SoC/board/product profiles, and traceable engineering and delivery.
The maintained reference is Linux Native plus STM32F407VG / STM32F4DISCOVERY
(MB997), with baremetal and FreeRTOS configurations.

| Profile | Implementation/evidence | Qualification boundary |
|---|---|---|
| Linux Native / GCC | Full targets build; software contracts and application checks | Host peripheral models |
| FreeRTOS POSIX port | Pinned real kernel and OSAL contract execution | Does not validate Cortex-M interrupts/timing |
| STM32F407VG / MB997 | Baremetal and FreeRTOS compiled and linked online with official pinned SDKs and ARM GCC at `4a283eb` | Physical HIL and PCB revision confirmation pending |
| GD32 | Independent porting constraints and candidate metadata | No verified SDK/board backend; configure rejects |
| Windows / macOS | Native presets retained | Not executed in this maintenance run |

The [support matrix](docs/strategy/support-matrix.yaml) defines promotion evidence.
Enterprise support and LTS are not currently promised.

## Build and verify

Use CMake 3.21+, Python 3.11+, a C11/C++17 compiler and OpenSSL 3 development
headers/libraries. ARM requires the complete ARM GCC/newlib toolchain.
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
cmake --preset stm32-armgcc-release
cmake --build --preset stm32-armgcc-release --parallel 4
cmake --preset stm32-armgcc-freertos-release
cmake --build --preset stm32-armgcc-freertos-release --parallel 4
```

ELF/map/bin/hex artifacts use the selected build's bin directory. Match silicon,
board revision, supply, partition layout and effective configuration before
flashing. GitHub Actions compiled and linked both STM32F407 profiles at revision
`4a283eb`; see the [integration evidence](docs/implementation/integration-validation.md).
The local maintenance environment lacked the complete ARM toolchain. Physical
HIL and PCB revision confirmation remain pending.

## Runtime contracts

- CPU-specific critical sections and explicit context, deadline, ownership,
  generation and destruction rules.
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
