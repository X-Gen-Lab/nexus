# Source SDK and static firmware composition

One authored TOML assembly selects SoC/Board, backend, typed instances and budgets.
`NEXUS_ASSEMBLY_FILE` is the sole configuration input. CMake targets own code
and component dependencies; generated `selection.cmake` selects those targets.
The atomic resolved bundle also contains bindings, memory layout and identity.

```cmake
cmake_minimum_required(VERSION 3.31)
project(firmware C ASM)
set(NEXUS_ASSEMBLY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assembly.toml")
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "" FORCE)
add_subdirectory(external/nexus nexus-owned)
nexus_add_firmware(firmware SOURCES main.c LIBRARIES Nexus::Log)
```

`nexus_add_firmware` attaches the precise startup, system source, linker input,
ABI flags and selected static bindings once. It does not initialize application
components or start a scheduler. Products own tasks, stack/TCB and queue storage,
Flash regions and shutdown policy. Vendor includes stay private to providers.

The internal `nexus_cmsis_core` target in `platform/CMSISCore.cmake` owns the
single pinned CMSIS-Core include tree. Family implementation targets use
`nexus_use_cmsis_core(target)`; callers do not add a second bundled Core include
root or override `NEXUS_CMSIS_CORE_INCLUDE_DIR` as a product setting. This is
separate from the public `Nexus::Core` target and does not export vendor types
through ordinary Nexus interfaces. Device/startup and HAL/LL remain family-owned.

Including the helper only derives its owned path; selecting an MCU
implementation performs the header-presence check and sets
`CMSIS_DISABLE_DEPRECATED=1`. Ordinary Native production configuration does not
require an Arm Core checkout. Native MCU-model fixtures explicitly opt into
the corresponding private dependency.

The installed SDK contains source and exported `find_package(Nexus CONFIG)`
helpers, not a precompiled universal MCU library. To prepare and verify it:

```sh
python cmake/package/package_source_sdk.py --source . --output /your/sdk-prefix
python cmake/package/package_source_sdk.py --verify /your/sdk-prefix/share/nexus/src
```

Packaging requires a clean Git tree and exact clean submodules. A separately
marked development fixture cannot be published. Relocate the complete prefix;
file hashes, licensing, dependencies and the CMake exports are verified. Actual
C/C++ Native execution and both MCU-family ARM relocation contracts live in
`tests/contracts/test_source_sdk.py`.

The source package retains the complete recursive CMSIS-Core `Include` tree
and its Apache-2.0 `LICENSE` from official v6.4.0 at the locked commit. Compiler
and architecture header closure is preserved; Core templates, RTOS components
and the rest of the upstream repository are not implicitly packaged or enabled.
The unchanged GigaDevice imports may still contain older bundled Core files;
retaining their original source/notices is distinct from compiling against them.
Actual include-closure, selected ARM and relocated-SDK checks establish the
migration's software scope; the version inventory alone establishes none of it.

Board packages are read-only `board.json` facts with provenance and reviewed
wiring. SoC resources and route catalogs live in `soc/<vendor>/<family>`. Unreviewed
physical connections cannot be enabled by claiming chip-level capability.
See [integration contracts](../docs/design/integration-contracts.md).
