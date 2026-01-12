# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Bilingual documentation support (English/Chinese)
- Sphinx documentation with separate EN/CN builds
- Documentation build script (build_docs.bat)

### Fixed
- UART test API compatibility with hal_uart.h
- C++20 designated initializers for MSVC compatibility
- Chinese navigation in Sphinx sidebar

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
- Doxygen backslash style comments (\file, \brief, \param)

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
