# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

## [0.1.0] - 2026-01-12 (Phase 1 Complete)

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
