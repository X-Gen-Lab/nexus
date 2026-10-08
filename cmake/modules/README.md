# Maintained CMake modules

The root build uses two small modules:

- `NexusConfig.cmake`: resolve an explicit Kconfig fragment once, produce the
  build-local effective configuration/header/CMake bundle, reject invalid inputs,
  and remove legacy cached configuration symbols.
- `NexusApplications.cmake`: create an application target, select target usage
  requirements, add STM32 startup/linker inputs, and emit flashable artifacts.

Libraries use ordinary `add_library`, `target_sources`,
`target_include_directories`, `target_compile_options` and
`target_link_libraries`. Usage requirements flow through `nexus_build_options`.
CMake presets own build mode and compiler selection. The checked-in pinned
submodules own dependency versions; configuration performs no downloads.

The former `NexusCore`, `NexusBuild`, `NexusQuality`, `NexusOptimization`,
`NexusCompilerConfig`, `NexusExtension`, `NexusToolchain`, `NexusHelpers` and
`NexusVendor` abstractions have been removed. Add targets with the ordinary CMake commands.
 Compiler/linker
Kconfig menus from the old design are excluded from the schema: presets and
actual target settings define the compiler behavior.

See `docs/implementation/build-config.md` for the contract and executed checks.

## Consume Nexus from a product source tree

Nexus supports `add_subdirectory` consumption with one selected platform,
backend and effective configuration per build. Its internal source paths use
`NEXUS_SOURCE_DIR`, initialized from the Nexus root's `CMAKE_CURRENT_LIST_DIR`.
Generated configuration, libraries and applications use `NEXUS_BINARY_DIR`,
initialized from that root's `CMAKE_CURRENT_BINARY_DIR`. Neither variable is a
cache setting, and the parent keeps its own output directories and configuration
cache entries. Tests and examples default to ON for a standalone checkout and
OFF when Nexus is a subproject.

For example, a product can contain a pinned Nexus checkout at `external/nexus`:

```cmake
cmake_minimum_required(VERSION 3.21)
project(controller C)
set(NEXUS_CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/product_defconfig")
add_subdirectory(external/nexus nexus)
nexus_add_application(
    TARGET controller
    SOURCES src/main.c
    EXTRA_DEPS Nexus::ConfigManager Nexus::ModbusRTU)
```

Configure the parent with an explicit build type, for example
`cmake -S . -B out/controller -DCMAKE_BUILD_TYPE=Release`. An MCU product also
selects the ARM toolchain before the parent `project()` call, normally using its
own preset and `toolchainFile`. Its fragment selects the maintained SoC, board,
backend and enabled components. `NEXUS_PLATFORM` and `NEXUS_OSAL_BACKEND`, when
provided, constrain that fragment; disagreements fail configuration. The default
platform is Native; a maintained MCU preset also selects `NEXUS_PLATFORM=stm32`.

`nexus_add_application` obtains configuration, platform and output paths from
properties on `Nexus::Config`, so the parent can call it after
`add_subdirectory` returns. Application artifacts go to the Nexus build root's
`bin`; ordinary parent targets retain the parent's output policy. Firmware maps,
BIN and HEX files follow the application's actual output directory.

Public aliases are `Nexus::Config` (build configuration), `Nexus::HAL`,
`Nexus::HALInterface`, `Nexus::OSAL`, `Nexus::OSALInterface`, `Nexus::Platform`,
`Nexus::Storage`, `Nexus::Security`, `Nexus::Update`, `Nexus::ModbusRTU` and
`Nexus::Industrial`. Enabled frameworks provide `Nexus::ConfigManager`,
`Nexus::Log`, `Nexus::Shell` and `Nexus::Init`. STM32 hardware bring-up code can
explicitly depend on `Nexus::STM32SDK`; ordinary products use HAL/OSAL contracts.
The application helper includes the selected platform objects, including its
startup, rather than relying on an archive scan to retain registration objects.

One build directory owns one mode. A multi-config parent must expose only the
configuration matching `CMAKE_BUILD_TYPE`; Nexus refuses to rewrite the parent's
configuration list. Build different boards, backends or products in separate
build directories. C-only consumers do not require Nexus's C++ test compiler.

This is a source SDK contract. Installed/exported binary SDK packages are not
implemented. Services are still added unconditionally, and the Native security
provider still requires OpenSSL 3; moving its lookup into the provider does not
establish service pruning. The independent consumer regression builds and runs
a real Native application and checks isolation, missing input, mode conflicts
and the multi-config boundary. It does not establish MCU hardware validation.
