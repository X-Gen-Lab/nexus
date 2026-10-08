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
