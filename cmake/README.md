# Source SDK, Board packages and firmware layout

Nexus is a source CMake SDK. A parent declares its languages/toolchain, supplies
one `NEXUS_CONFIG_FILE`, and adds the pinned checkout as a subdirectory. Each
build root has one Board/backend and one effective configuration. The SDK owns
its output subtree and does not alter the parent's C/C++ defaults or output.
No installed binary package is claimed by these source-consumer checks.

```cmake
cmake_minimum_required(VERSION 3.21)
project(firmware C ASM)
set(CMAKE_BUILD_TYPE Release)
set(NEXUS_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/platform.conf")
set(NEXUS_BOARD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/board") # optional external Board
set(NEXUS_FLASH_LAYOUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/layout.json") # optional physical Flash policy
add_subdirectory(external/nexus nexus-owned)
nexus_add_application(TARGET firmware SOURCES main.c VERSION "1.0.0")
target_link_libraries(firmware PRIVATE Nexus::ModbusRTU) # explicitly selected component
```

`nexus_add_application` links `Nexus::Firmware`: selected platform objects,
startup/linker, HAL/OSAL and the explicit common Runtime bootstrap.
Top-level ARM presets additionally enable `NEXUS_BUILD_CONTRACTS`; the SDK
subdirectory default is OFF. Contract firmware lives under `tests/firmware`
and validates platform linking without importing example repository sources. It never
constructs product tasks or initializes optional components. `Nexus::Runtime`
is the bootstrap contract alone. C++ consumers declare CXX in the parent.
Public GPIO/UART/SPI/I2C/Flash facades and explicit component adapters can be
linked separately. The vendor SDK is a private implementation dependency;
direct bring-up code must opt into `Nexus::STM32SDK` or `Nexus::GD32SDK`.

Firmware `FIRMWARE_MAIN_STACK_SIZE` is the main/exception-stack reservation;
`FIRMWARE_LIBC_HEAP_SIZE` is the libc heap reservation. Neither describes OSAL
task stacks, queue payload pools, FreeRTOS management/idle/daemon stacks or
hardware buffers. `FIRMWARE_GENERATE_BIN/HEX`, mandatory map output and size
reporting are generic artifact options; application targets live externally.
Old Product and `BUILD_APP_*`/`APP_*` configuration symbols are rejected.

## One external Board entry

`NEXUS_BOARD_DIR` selects a package containing `board.json`, `CMakeLists.txt` and
its narrow Board resources. The config fragment selects the matching supported
SoC and software features; it must not simultaneously select an internal Board.
`BOARD_EXTERNAL` and `BOARD_NAME` are resolved from the explicit package input.
Missing or contradictory packages fail configuration without a built-in fallback.

The version 1 manifest has only `schema`, `id`, `soc`, `hse_hz`,
`interface_target`, `object_targets`, `inputs`, and `resources`. Examples are in
`boards/*/board.json`; a real independent Native fixture is under
`tests/fixtures/external_board`. Input paths must stay within the package and
name regular files. The manifest and listed actual source files are hashed.
Target names must correspond to interface/object targets produced by the Board
CMake file. All Board OBJECT targets are forwarded explicitly, preserving strong
callbacks and registries in the final executable.

A resource owns pins (port, pin, AF, signal, and GPIO initial level), a maintained
controller route, clock/IRQ/priority and any supported DMA route. Conditional
activation refers to the effective Kconfig symbol. Validation rejects duplicate
pin/controller/IRQ/DMA ownership, missing enabled controller or GPIO bindings,
clock and density mismatches, invalid AF routes, incomplete selected SPI DMA,
and IRQ priorities outside the maintained FreeRTOS syscall range. These are
checks for the **explicitly reviewed routes**, not a complete all-peripheral
silicon topology solver. New UART/SPI pin routes, I2C hardware providers, external
oscillator profiles or DMA combinations need implementation/review/test coverage
before the route catalog can admit them. Manifest checks cannot inspect PCB
copper, jumpers, boot wiring, electrical safety or worst-case timing.

## One Flash layout input

SoC providers expose the entire physical Flash and actual erase geometry. Nexus
has no default parameter partition. Without `NEXUS_FLASH_LAYOUT_FILE`, the image
owns the whole physical Flash and the region list is empty. A consumer explicitly
reserves erase-aligned regions:

```json
{
  "schema": 1,
  "soc": "stm32f407zg",
  "image": {"offset": "0x0", "size": "0xc0000"},
  "regions": [
    {"name": "params_a", "offset": "0xc0000", "size": "0x20000"},
    {"name": "params_b", "offset": "0xe0000", "size": "0x20000"}
  ]
}
```

The parser rejects overlap, out-of-range/overflow, wrong silicon, duplicate names
and boundaries crossing a physical erase block. F407 uses nonuniform sectors;
F470 uses 4 KiB pages. This source-SDK entry currently requires image offset zero:
relocated applications need an explicit verified boot/vector/startup binding.

The same parse emits `generated/layout.json`, `nx_flash_layout.h` and
`firmware.ld`. The latter includes maintained section placement and defines
`__nexus_image_start/end` plus `__nexus_region_<name>_start/end`. The header exposes
`NX_LAYOUT_REGION_LIST(X)` for caller-selected typed Flash regions. Opening a
region does not authorize arbitrary partitions or boot/update policy. Generated
Board/layout/config hashes are bound into firmware identity; the linker
emits eight `__nexus_layout_sha256_<index>` and eight
`__nexus_board_sha256_<index>` words for an independently checked
ELF/layout match. Hashes are retained with
ELF/map artifacts. A changed layout triggers CMake reconfiguration and relinking;
a failed reconfiguration deletes the obsolete Board/layout bundle.

## Validation boundaries

Real external C and C++ Native consumers check bootstrap, parent isolation,
minimal component selection and independent Board identity. Board/layout behavior
checks exercise rejected conflicts and shared generated outputs. ARM ELF checks
prove link layout/startup/object contracts; physical IRQ/DMA/Flash and electrical
behavior require separate HIL. Install/export, toolchain/ABI variant packaging
and relocation are separate work and are not implied by target aliases.
