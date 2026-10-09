# Nexus Embedded Platform

Nexus provides the common firmware platform for industrial control and connected devices: CPU primitives, typed HAL, OSAL, SoC/Board integration, explicit infrastructure lifecycle, reusable components, and traceable builds. Application entry points, workers, product policy, private protocols, partitions and manufacturing identities belong to external repositories.

Reference examples live in [X-Gen-Lab/nexus-examples](https://github.com/X-Gen-Lab/nexus-examples). They pin a Nexus source revision and use public targets and headers; platform contract tests build independently of those applications. The [38-task execution record](docs/implementation/refactor-execution.md) separates production implementation, actual software checks, final integration and physical qualification.

| Platform | Maintained software | Qualification boundary |
|---|---|---|
| Linux Native | HAL/OSAL contracts and peripheral models; explicit OpenSSL provider | Host behavior does not establish MCU electrical or real-time behavior |
| Pinned FreeRTOS POSIX port | Real kernel execution for OSAL contract tests | ARM port interrupts and timing remain separate |
| STM32F407VG Discovery / F407ZG Qiming V3.1 / F407VE Sky Youth | Real startup, private vendor SDK, GPIO/UART/SPI providers and full physical Flash geometry | Exact PCB, wiring, IRQ/DMA/wire timing and power loss require HIL |
| GD32F470ZG Liangshan Pi | Independent official SDK 3.3.3, startup, clock, GPIO/UART/SPI/time and Flash implementation | MCU qualification has not been executed |
| Windows / macOS Native | Retained presets and common wrappers | Not executed in this Linux maintenance session |

STM32/GD32 modern typed **I2C hardware providers are currently unsupported**. Native typed I2C is implemented and tested. Unsupported capabilities fail explicitly. No physical board, enterprise support, LTS, installed binary SDK or MPU protection qualification is claimed by this iteration. See the [support matrix](docs/strategy/support-matrix.yaml).

## Source ownership

`arch/` owns CPU primitives. `soc/stm32f407/`, `soc/gd32f470/` and `soc/native/`
own controllers, clock/IRQ/time, physical or virtual resources, private headers
and SDK assembly. Native is a host model. `boards/` owns reviewed wiring;
`platforms/` owns startup/lifecycle and final object assembly through one common
helper. F407 family directories retain precise VE/VG/ZG configuration, density
and artifact identities. Unmaintained platform/configuration shells are removed.

## Build the platform

Use CMake 3.21+, Ninja, a C11/C++17 compiler and Python (3.11 recommended). Kconfiglib is pinned; configuration does not download dependencies. The full Native profile explicitly selects OpenSSL 3 and needs its development package. `native-minimal-debug` builds without optional services or OpenSSL.

```sh
git clone --recurse-submodules https://github.com/X-Gen-Lab/nexus.git
cd nexus
python3 -m pip install kconfiglib==14.1.0
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --parallel 4
ctest --preset linux-gcc-debug --output-on-failure --no-tests=error --parallel 4
python3 -m unittest discover -s scripts/ci -p 'test_*.py'
```

The same build/test sequence is available through the maintained wrapper:

```sh
python3 scripts/ci/ci_build.py --preset linux-gcc-debug --stage all --jobs 4
cmake --preset native-minimal-debug
cmake --build --preset native-minimal-debug --parallel 4
```

The minimal and optional-component presets compile platform libraries; application demos are in the external repository. `cmake --list-presets` lists the available profiles. A preset's existence is not execution evidence.

ARM builds use the complete fixed ARM GNU 14.3.rel1/newlib toolchain. The installer verifies the archive hash in [toolchains.lock.json](dependencies/toolchains.lock.json).

```sh
python3 scripts/ci/install_arm_toolchain.py --install-dir build/toolchains/arm-gnu-14.3.rel1
export PATH="$PWD/build/toolchains/arm-gnu-14.3.rel1/bin:$PATH"
cmake --preset stm32-qiming-armgcc-freertos-release
cmake --build --preset stm32-qiming-armgcc-freertos-release --parallel 4
python3 scripts/ci/validate_firmware_elf.py \
  --build-dir build/stm32-qiming-armgcc-freertos-release \
  --report build/stm32-qiming-armgcc-freertos-release/firmware-static.json
```

| Board | Baremetal Release preset | FreeRTOS Release preset |
|---|---|---|
| Discovery F407VG | `stm32-armgcc-release` | `stm32-armgcc-freertos-release` |
| Qiming F407ZG V3.1 | `stm32-qiming-armgcc-baremetal-release` | `stm32-qiming-armgcc-freertos-release` |
| Sky F407VE Youth | `stm32-sky-armgcc-baremetal-release` | `stm32-sky-armgcc-freertos-release` |
| Liangshan F470ZG | `gd32f470-armgcc-baremetal-release` | `gd32f470-armgcc-freertos-release` |

Top-level MCU profiles build `nexus_contract_firmware.elf` plus configured map/bin/hex in `build/<preset>/bin`. This fixture checks startup and platform linking; it is not a business application or physical UART echo qualification. Source consumers default to no platform development tests or fixtures.

## Consume Nexus

A parent project can add a fixed complete checkout as a subdirectory:

```cmake
cmake_minimum_required(VERSION 3.21)
project(my_firmware LANGUAGES C CXX ASM)
set(NEXUS_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/platform.conf" CACHE FILEPATH "")
add_subdirectory(external/nexus nexus)
nexus_add_application(TARGET firmware SOURCES main.c)
```

Select the toolchain before `project()` using `-DCMAKE_TOOLCHAIN_FILE=.../cmake/toolchains/arm-gcc.cmake`; ARM also selects `NEXUS_PLATFORM=stm32` or `gd32f470`. Start from a maintained configuration fragment and keep one platform/Board/backend per build directory. The consumer owns `main()`, component initialization, tasks, scheduling and recovery.

```c
#include "runtime/nx_runtime.h"

int main(void) {
    nx_boot_report_t report;
    if (nx_runtime_bootstrap(&report) != NX_OK) return 1;
    /* Initialize selected components/devices, then run your loop or scheduler. */
    return 0;
}
```

`Nexus::Firmware` explicitly links the selected platform objects/startup plus Runtime and typed HAL. `nx_runtime_bootstrap()` initializes HAL then OSAL, reports ownership and rollback failures, and starts no worker or scheduler. `nx_runtime_shutdown()` requires callers to settle their objects. Idle baremetal/pre-scheduler MCU cleanup is bounded; a running or suspended FreeRTOS kernel remains BUSY. Read-only close rejection retains READY; hardware cleanup failure retains PARTIAL and admission fencing for retry. Direct SDK users and products first quiesce their own resources and safe outputs. `nx_platform_get_info()` reads Board/backend, identity digests and main SRAM/Flash/resource information without starting hardware.

A clean complete checkout can also prepare a relocatable **source SDK** outside its source tree:

```sh
python3 -B cmake/package/package_source_sdk.py \
  --source /absolute/path/to/nexus --output '/absolute/path/to/nexus sdk prefix'
python3 -B '/absolute/path/to/nexus sdk prefix/share/nexus/src/cmake/package/package_source_sdk.py' \
  --verify '/absolute/path/to/nexus sdk prefix/share/nexus/src'
```

Use `find_package(Nexus 0.1.0 EXACT CONFIG REQUIRED)` instead of `add_subdirectory`, passing `Nexus_DIR=<prefix>/lib/cmake/Nexus`, your `NEXUS_CONFIG_FILE` and `NEXUS_EXPECTED_SOURCE_REVISION`. The package rebuilds actual source, dependencies, startup and linker. See [package preparation and consumer commands](cmake/package/README.md). The historical clean `5498b2b` snapshot completed strict publishable preparation and actual relocated consumers. A later source revision must prepare, verify and consume a new source-bound package. Development fixtures require explicit opt-in and remain ineligible for release promotion.

## Configuration and ownership

- `generated/effective.config`, `nexus_config.h` and `config.cmake` come from one Kconfig parse. Contradictory, unknown or out-of-range settings stop configuration. No source-root `.config` or stale header fallback exists.
- `NEXUS_BOARD_DIR` supplies one external Board package. Its manifest binds source inputs and reviewed GPIO/UART/SPI routes. This validation is scoped to maintained routes, not a complete automatic resource solver.
- `NEXUS_FLASH_LAYOUT_FILE` supplies consumer-owned image/regions. One parse generates linker and region declarations; Board/layout digests are bound to ELF symbols. The default image covers full physical Flash with no automatic storage reservation. Nonzero image offsets are rejected.
- Device discovery is side-effect free. Explicit typed open/close and owner/generation references protect lifetime. Timeout or cancellation does not release an unsettled hardware buffer.
- Log/Shell core and typed UART adapters, Config RAM/Flash, StorageHAL and OpenSSL are explicit choices. Test fakes do not enter production libraries. Crypto core has no default provider; a missing MCU crypto provider returns unsupported.

Read [architecture decisions](docs/strategy/architecture-decisions.md), [HAL/OSAL contracts](docs/strategy/hal-osal-design.md) and [enterprise workflow](docs/strategy/enterprise-workflow.md). Historical `3129550` Native 1781 tests and eight ARM/15 ELF results remain recorded in [platform-validation.json](docs/implementation/platform-validation.json); they do not qualify the new refactor. The historical clean `5498b2b` delivery passed 1810 Native tests, 8 platform contract ELF links and 38 external example ELF links; current source changes require fresh verification. Current software status is in [support-matrix.yaml](docs/strategy/support-matrix.yaml), with exact source-pair identity in the [external validation record](https://github.com/X-Gen-Lab/nexus-examples/blob/main/evidence/platform-refactor-validation.json). The [RF execution table](docs/implementation/refactor-execution.csv) preserves targeted historical scopes and separates current revalidation from physical/operational acceptance.

A release binds the same source/dependencies, effective configuration, Board/layout, toolchain, ELF/BIN and nonzero executed tests. Product promotion additionally needs actual HIL, trust/signing, measured budgets and manufacturing evidence. Read [AGENTS.md](AGENTS.md) before changing contracts. Nexus uses the [MIT license](LICENSE); dependencies keep their own licenses.
