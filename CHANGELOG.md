# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## [Unreleased]

Future changes are tracked here.

## [0.2.0] - 2026-10-22 (Asia/Shanghai)

Source SDK preview preparation. Published previews and their immutable source
identities are recorded in [GitHub Releases](https://github.com/X-Gen-Lab/nexus/releases);
this entry alone establishes neither publication nor candidate qualification.
The published 0.1.0 tag, assets and original evidence remain unchanged. Factory
epoch `1` and authored assembly schema `2` remain separate, unchanged identities.

### Added

- A pinned official STM32F4 HAL Driver dependency with reviewed CMSIS pairing,
  selected LL headers and license closure. Selected provider operations reuse LL
  while Nexus retains its request, cancellation and drain state machines; this
  is not a platform-wide HAL Handle/callback migration.
- Optional caller-owned cold lifecycle diagnostics and a read-only mode review
  work-package view derived from existing authorities. The view does not execute
  tests or grant software or physical qualification.
- Explicit H743 V-revision, HSI64, baremetal AXI-SRAM reference selections for
  finite SPI DMA and continuous UART RX with Cache off/on. UART RX can retain
  independent IRQ TX or explicitly select finite DMA TX with a shared UART owner.
- U575 TZEN0 finite USART1 GPDMA hardware-chain TX and LPTIM2 RUN one-shot
  references. These do not enable autonomous STOP operation or arbitrary chains.
- Selected U575 [retained-time and bounded Stop2](docs/design/u575-retained-time.md),
  [finite SRAM4 LPDMA copy](docs/design/u575-stop2-copy.md) and
  [single-tuple LPTIM1 LPBAM](docs/design/u575-lptim1-lpbam.md) software references;
  these do not provide general LPBAM or complete low-power qualification.
- Selected [U575 MPU GPIO WRITE](docs/design/u575-mpu-gpio.md) and
  [F407 finite I2S2 slave TX](docs/design/f407-i2s2-slave-tx.md) components;
  physical isolation and external audio execution remain unqualified.
- Pure caller-owned timestamp budget comparison with known uncertainty bounds
  and separate WITHIN, EXCEEDED and UNKNOWN results. Raw DWT snapshots do not
  acquire an elapsed-time or clock-continuity guarantee.
- Software HIL preparation for the five selected I/O modes and seven independent
  ThreadX acceptance protocols. Reference fixtures remain unbound and cannot
  authorize a physical station.

### Changed

- Constructors return an explicit frozen construction contract for private state,
  static definitions, initialization and failed-init cleanup responsibility.
- Current Source SDK consumers request exact version `0.2.0`; old SDK and
  qualification records keep their original source and version.
- Host-only GoogleTest and GoogleMock use the pinned `ext/googletest` Git
  submodule. Firmware and its installed source SDK exclude the host framework.
- Source SDK consumer policy v11 requires 50 independent roots and 53 images;
  `h743 high` uses the canonical
  `tools/configure/assemblies/h743-high-peripherals.toml`. These are required-plan
  identities; execution results bind a particular source and retained evidence.
- Measurement schema 2 requires raw DWT endpoints, a continuous counter epoch,
  and an independent admitted reference with CPU-frequency and timing-error
  bounds. Older scalar assertions cannot qualify new measurements.
- A manual, exact-main-SHA workflow executes the complete software plan with
  bounded deadlines and preserves both original output trees. Running this
  entry and sealing a software candidate remain separate validation steps.

### Fixed

- H743 TX/RX domain-loss paths retain loans and stop peripheral accesses after
  isolation. RX publication requires a final live-domain observation.
- Successful H743 UART stop restores its owned cold registers for restart;
  refused drain or restoration retains resources and prevents false success.
- The limited H743 Flash ELF auditor follows actual callee register and stack
  argument provenance and rejects unsupported SP register-offset loads. Full
  production execution closure remains a separate, unfinished proof scope.
- Provider-required public consumers must also belong to the source SDK's
  reviewed export selection.
- Independent U575 retained-time, Stop2, LPDMA-copy and LPTIM1-LPBAM SDK
  consumers verify their canonical component owners, caller storage and linked
  code without claiming factory-face retention or hardware execution.
- The H743 RAM/stack audit derives each original `.su` source identity from
  its actual compile arguments and ordered GCC file/debug prefix maps. It
  rejects wrong or ambiguous sources instead of matching basenames or suffixes.
- Release CI admission requires the include-only pin-state provider audit within
  the exact provider matrix; missing, substituted, extra or unsuccessful jobs
  remain rejected.
- Complete CI evidence has a separate bounded aggregate inventory budget. An
  explicit tagged-source recovery mode preserves immutable SDK source/tag
  identities while validating the trusted main builder and both fresh full CI runs.

### Verification and limitations

Fresh tests, exact linked-image comparisons, independent SDK consumption and
candidate evidence must qualify the changed source separately. Physical MCU HIL
remains `not_executed`. General bootloader handoff, dynamic reclocking, complete
deep-STOP support and broader H743/U575 modes retain their existing unimplemented
or partial boundaries.

## [0.1.0] - 2026-10-15 (Asia/Shanghai)

First public Source SDK developer preview. The January Phase 1 entry below is a
historical project milestone; it does not identify this platform release.

### Added

- A relocatable source SDK with exact version selection, pinned recursive
  dependencies, license notices and a complete file manifest. Consumers rebuild
  the platform and their application together; no stable binary ABI is promised.
- Static C11 Core/Arch/SoC/Board, typed I/O, optional OS and component mechanisms.
  One authored TOML assembly resolves through validated typed IR into one atomic
  bundle. Generated factories return existing readonly faces without allocation
  or hardware startup; shared operations tables dispatch to private contexts.
- Exact F407ZG Qiming, F407VE Sky and GD32F470ZG Liangshan assemblies; separate
  G474 EXTI, H743 fixed-clock/cache/TX-DMA and U575 single-world or synchronous
  dual-world reference scopes. CPU-only Runtime profiles do not grant SoC/Board
  support. ThreadX maintains its explicit Native/M4 subset.
- Bilingual task documentation, public API references, software evidence indices
  and executable HIL preparation tools. Applications and product policy remain
  in external consumer repositories.

### Changed

- Normalize the prepublication `1.0.0` source version to the first public `0.1.0`.
  `dependencies/platform-compatibility.json` is the sole authored version policy;
  CMake, generated headers and package metadata derive their version from it.
  Source interface epoch `1`, exact SDK matching and authored schema `2` remain.
- Replace the superseded HAL/OSAL/Kconfig assembly path with static typed I/O,
  caller-owned requests and explicit ownership, configuration and target graphs.

### Verification and limitations

Release asset manifests and execution receipts identify the exact released
source, dependencies, tools and observed software checks. Historical evidence
keeps its original source: the build-system baseline is `cc6a065c`, with earlier
SoC baselines recorded separately. Those results do not transfer to this release.
The eight old H743/U575 candidate archives are currently unavailable at their
recorded paths because their relocated `/tmp` target is absent; old passed
receipts are preserved and do not make those archives downloadable.

Physical MCU HIL remains `not_executed`. Source distribution, host models, ARM
linking and documentation publication do not establish physical timing,
electrical safety, signed product qualification, certification or LTS. See the
[release guide](docs/status/release-0.1.0.md) for exact support, verification and
remaining acceptance, and [delivery](docs/delivery/README.md) for original records.

## Pre-refactor working history (superseded)

### Added

#### Build System Refactoring
- New simplified CMake module architecture:
  - NexusCore: Core build system functionality
  - NexusBuild: Target creation and management (nx_add_executable, nx_add_library)
  - NexusConfig: Kconfig integration and configuration management
  - NexusHelpers: Utility functions for build system
  - NexusToolchain: Toolchain detection and configuration
  - NexusExtension: Extension system for custom build logic
  - NexusOptimization: Build optimization and performance tuning
  - NexusQuality: Code quality tools integration
  - NexusVendor: Vendor library management
- Kconfig compiler configuration (cmake/Kconfig.compiler)
- Vendor library management system (cmake/vendors/)
- Dependency setup scripts:
  - setup_deps.sh for Linux/macOS
  - setup_deps.bat for Windows
  - Automatic platform detection from .config
  - Intelligent submodule initialization based on platform needs

#### Documentation
- Comprehensive dependency management guide in README
- Automatic, manual, and CMake auto-initialization methods
- Platform-specific dependency examples (STM32, native, FreeRTOS)

### Changed
- Simplified root CMakeLists.txt structure
- Updated all application CMakeLists to use new NexusBuild module
- Updated framework, HAL, and OSAL build files for new system
- Updated all platform build files (native, STM32)
- Updated test infrastructure for new build system
- Modernized all toolchain files (arm-none-eabi, armclang, iar-arm, native)
- Improved configuration management (Kconfig, nexus_config.h)

### Removed
- Complex modular build system (13 specialized modules):
  - LoadKconfig.cmake
  - NexusCache.cmake
  - NexusDependency.cmake
  - NexusDiagnostics.cmake
  - NexusIncremental.cmake
  - NexusIsolation.cmake
  - NexusKconfig.cmake
  - NexusPerformance.cmake
  - NexusPlatform.cmake
  - NexusPlugin.cmake
  - NexusReproducible.cmake
  - NexusResource.cmake
  - NexusTest.cmake
- ApplicationCommon.cmake (functionality moved to NexusBuild)
- Completed build system redesign spec files

### Fixed
- Code quality improvements in OSAL baremetal adapter
- Comment formatting in native platform I2C and SPI implementations
- Missing file header comments aligned with Nexus standards

### Added

#### CI/CD Infrastructure
- Modular GitHub Actions workflow architecture:
  - New `ci.yml` workflow for unified continuous integration
  - `build-matrix.yml` for multi-platform build testing
  - `docs-build.yml` for automated documentation generation
  - `quality-checks.yml` for code quality validation
  - Reusable actions in `.github/actions/` for better maintainability
- Enhanced performance testing and release automation workflows
- Improved workflow documentation in `.github/workflows/README.md`

#### Build System Enhancements
- CMakePresets.json for standardized build configurations across platforms
- NexusPlatform.cmake module for platform-specific configurations
- Enhanced CTestScript.cmake with better error handling and reporting
- Improved root CMakeLists.txt with better project structure

#### Testing Infrastructure
- Comprehensive test infrastructure improvements:
  - Consistent target naming across all test CMakeLists.txt
  - New native_flash_helpers.c/h for flash device testing utilities
  - Enhanced test_nx_flash_properties.cpp with additional test cases
  - Better test organization and maintainability

#### Documentation
- Complete rewrite of CI/CD integration documentation (ci_cd_integration.rst)
- Expanded build system documentation with detailed CMake configuration guide (build_system.rst)
- Updated Chinese translations for all documentation changes
- Improved documentation structure and readability

#### Scripts
- Enhanced build.py with better error handling and logging
- Improved setup.py with code cleanup
- Better script maintainability and user feedback

### Changed
- Refactored GitHub Actions workflows from monolithic to modular architecture
- Replaced old workflows (build.yml, test.yml, docs.yml, validation.yml) with new modular structure
- Updated performance.yml and release.yml to align with new architecture
- Improved test infrastructure organization

### Removed
- Obsolete monolithic workflow files (build.yml, test.yml, docs.yml, validation.yml)
- Unused code in osal_native.c

### Fixed
- Configuration file line ending consistency (.config, nexus_config.h)

### Breaking Changes
- **HAL Configuration Removal**: Removed all runtime configuration methods from HAL interfaces
  - Removed `nx_uart_config_t`, `nx_gpio_config_t`, `nx_can_config_t`, `nx_spi_config_t`, and `nx_i2c_config_t` structures
  - Removed `get_config()` and `set_config()` methods from all HAL interfaces
  - All static configuration must now be done at compile-time through Kconfig
  - Device-specific runtime parameters (SPI device config, I2C device address) are still supported
  - See MIGRATION_GUIDE.md in `.kiro/specs/hal-config-removal/` for migration instructions

### Added

#### Documentation
- Complete Sphinx documentation restructure with clear sections:
  - Getting Started guides (installation, quick start, first application, configuration, build and flash)
  - User Guide (build system, IDE integration, Kconfig tutorials, debugging, error handling, performance)
  - Development guides (API design, architecture, CI/CD, code review, debugging, porting, release process, security)
  - API reference documentation for all modules
  - Platform guides (Native, STM32F4, STM32H7, GD32)
  - Tutorials (GPIO control, UART communication, I2C sensors, SPI communication, ADC sampling, Timer PWM, etc.)
- Documentation templates for modules, platforms, and tutorials
- Comprehensive Chinese translations for all documentation
- Documentation contribution guidelines and translation tools
- Enhanced README files for all framework modules (config, init, log, shell)
- Module-specific documentation directories with design docs, user guides, porting guides, troubleshooting
- Chinese versions of README and CONTRIBUTING files
- Codecov configuration for test coverage reporting

#### CI/CD
- Comprehensive issue templates (bug report, feature request, platform request, build issue, documentation, performance, security, test failure, question)
- Structured PR templates with detailed checklists (standard, hotfix, platform, simple)
- Enhanced CI workflows with better matrix testing
- New workflows for performance testing, release automation, and security scanning
- GitHub automation scripts for coverage comparison and CI validation

#### Testing
- Complete test suite reorganization:
  - Moved HAL tests to `tests/hal/native/` for better organization
  - Added comprehensive native device test helpers for all peripherals (ADC, DAC, GPIO, I2C, SPI, UART, Timer, CRC, Flash, RTC, SDIO, USB, Watchdog)
  - Added property-based tests for all HAL peripherals
  - New test suites for config, init, log, and shell modules
  - Integration, performance, and thread-safety tests
  - Python tests for Kconfig validation and generation
- Test infrastructure improvements with CMakeLists and helper utilities
- Test documentation and comprehensive testing guides

#### HAL Implementation
- Enhanced native platform HAL implementation:
  - Improved device lifecycle management and power control
  - Comprehensive Kconfig options for all peripherals (ADC, DAC, GPIO, I2C, SPI, UART, Timer, USB, Watchdog, CRC, Flash, RTC, SDIO, Option Bytes)
  - Enhanced device registration and factory pattern implementation
  - Detailed peripheral configuration options
  - Improved error handling and validation
  - Helper functions for device operations
  - Updated device types and interfaces

#### Kconfig System
- Comprehensive Kconfig tools infrastructure:
  - Kconfig validation and generation tools
  - Naming convention validators
  - Structure validators
  - Report generation utilities
  - CLI integration for Kconfig management
- Enhanced configuration generation script with better error handling
- Improved Kconfig generation with comprehensive validation
- Device registration mechanism for Kconfig-driven device instantiation
- Kconfig-based compile-time configuration system for all HAL peripherals:
  - UART: Baudrate, data bits, stop bits, parity, buffer sizes
  - SPI: Bus clock frequency, pin mapping
  - I2C: Bus speed, pin mapping
  - GPIO: Mode, pull-up/down, speed, alternate function
  - CAN: Baudrate, mode, bit timing parameters
  - ADC, DAC, Timer, USB, Watchdog, and other peripherals

#### Build System
- Updated CMakeLists.txt with improved build options
- Expanded nexus_config.h with comprehensive configuration options
- Improved build system flexibility and modularity

### Changed
- HAL interfaces now focus purely on runtime operations
- Factory functions no longer accept configuration parameters
- Platform implementations read configuration from Kconfig-generated macros
- Refactored demo applications with improved structure:
  - Cleaner blinky demo code structure
  - Better config_demo examples
  - Enhanced freertos_demo with improved task management
  - More command examples in shell_demo
  - Improved code comments following Doxygen standards
  - Better error handling and validation

### Removed
- `nx_configurable.h` generic configuration interface
- All `*_with_config()` factory functions
- Runtime configuration methods from UART, SPI, I2C, GPIO, and CAN interfaces
- Obsolete example files from docs/examples
- Python egg-info build artifacts
- Outdated Sphinx translation tracking files
- Deprecated platform stub files
- Old test migration guides

### Fixed
- UART test API compatibility with hal_uart.h
- C++20 designated initializers for MSVC compatibility
- Chinese navigation in Sphinx sidebar
- Log module formatting issues
- Removed unused code in shell autocomplete
- Improved code consistency across framework

### Documentation
- Enhanced OSAL mutex header documentation with better API docs
- Improved Doxygen comments throughout the codebase
- Better parameter documentation and usage examples

### Migration Notes
- **Action Required**: Update all code using runtime configuration to use Kconfig
- **Backward Compatibility**: Deprecated structures were marked with `NX_DEPRECATED` in previous versions
- **Migration Path**: See `.kiro/specs/hal-config-removal/MIGRATION_GUIDE.md` for detailed instructions
- **Timeline**: This is a breaking change - update your code before upgrading

---

## Historical Phase 1 milestone - 2026-01-12

This entry preserves the early development record, which originally used the
label `0.1.0`. No corresponding public platform GitHub tag/release was observed
during the 2026-10-15 release inventory. Its old architecture and platform claims
describe that milestone and do not establish current support or qualification.

### Added

#### Project Infrastructure
- Project directory structure following embedded best practices
- MIT License
- README.md with project overview
- CONTRIBUTING.md with contribution guidelines
- .gitignore for embedded development

#### Code Standards
- .clang-format (80 char, 4 space indent, pointer left-aligned)
- .clang-tidy for static analysis
- .editorconfig for consistent formatting

#### Development Environment
- VS Code configuration (settings, tasks, launch, extensions)
- Doxygen backslash style comments (\\file, \\brief, \\param)

#### Build System
- CMake root configuration with platform selection
- ARM GCC toolchain file (arm-none-eabi.cmake)
- NexusHelpers.cmake utility functions
- Support for native and STM32F4 platforms

#### HAL (Hardware Abstraction Layer)
- hal_def.h - Common definitions and status codes
- hal_gpio.h - GPIO interface (init, read, write, toggle, IRQ)
- hal_uart.h - UART interface (init, read, write, callbacks)
- hal.h - Unified HAL header

#### OSAL (OS Abstraction Layer)
- osal_def.h - Common definitions
- osal_task.h - Task management interface
- osal_mutex.h - Mutex interface
- osal_sem.h - Semaphore interface
- osal_queue.h - Message queue interface
- osal.h - Unified OSAL header
- Baremetal adapter implementation

#### Platform Support
- STM32F4 platform:
  - stm32f4xx.h device header
  - system_stm32f4xx.c system initialization
  - startup_stm32f407xx.c startup code with vector table
  - hal_gpio_stm32f4.c GPIO implementation
  - hal_uart_stm32f4.c UART implementation
  - hal_system_stm32f4.c system functions
  - STM32F407VGTx_FLASH.ld linker script
- Native platform (for host testing):
  - hal_gpio_native.c simulated GPIO
  - hal_uart_native.c console I/O
  - hal_system_native.c timing functions

#### Example Applications
- Blinky LED example for STM32F4 Discovery board

#### Test Framework
- Google Test integration
- HAL GPIO unit tests
- HAL UART unit tests
- Test main entry point

#### Documentation
- Doxyfile configuration
- Sphinx + Breathe integration
- Documentation index page

#### CI/CD
- GitHub Actions build workflow (native + ARM cross-compile)
- GitHub Actions test workflow (coverage, sanitizers, MISRA)
- Multi-platform support (Ubuntu, Windows, macOS)
