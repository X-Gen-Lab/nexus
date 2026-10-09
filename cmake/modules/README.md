# Maintained CMake modules

The root uses four small modules with one effective configuration and one Board/backend per build root:

| Module | Responsibility |
|---|---|
| `NexusConfig.cmake` | Resolve the explicit Kconfig fragment, reject contradictions and generate build-local effective config/header/CMake outputs; remove obsolete CONFIG cache values. |
| `NexusBoard.cmake` | Select a maintained Board or one `NEXUS_BOARD_DIR`, validate declared inputs/reviewed resources and derive Board identity plus consumer-owned Flash layout/linker/region outputs. |
| `NexusApplications.cmake` | Create a consumer executable from `Nexus::Config` target context; explicitly link `Nexus::Firmware` objects/startup/runtime and selected dependencies, with isolated map/bin/hex/resource settings. |
| `NexusSDK.cmake` | Expose `nexus_package_source_sdk(OUTPUT_DIRECTORY ... [DEVELOPMENT_FIXTURE])` for a verified relocatable source package. It delegates preparation and identity verification to the package tooling. |

Libraries use ordinary CMake `add_library`, `target_sources`, include/options and link usage requirements. Presets own compiler/build mode, effective Kconfig owns software/resource selection, and fixed dependencies own SDK versions. Configuration performs no downloads. No extra module DSL, implicit Product assembly or automatic business entry point is maintained.

A parent declares languages/toolchain before adding the fixed source checkout, supplies `NEXUS_CONFIG_FILE`, and calls `nexus_add_application()` after SDK configuration. The helper obtains platform/output/config from the SDK target, preserving ordinary parent target outputs and settings. The application explicitly owns main, components, workers and scheduler. Top-level host tests/ARM link contracts are development facilities; source consumers default them off.

External Board paths, Flash layout schema, firmware budgets and source-consumer examples have one maintained description in [the CMake SDK guide](../README.md). Installed **source** package preparation, exact-version `find_package`, manifest verification, relocation and development opt-in are maintained in [the package guide](../package/README.md). No installed binary SDK or cross-compiler/configuration ABI guarantee follows from these aliases.

Actual implementation checks and remaining clean/HIL gates are in [the RF execution record](../../docs/implementation/refactor-execution.md). Historical source revision results are not inherited by later module changes.
