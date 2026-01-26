# Nexus Embedded Platform

[![CI Status](https://github.com/nexus-platform/nexus/workflows/CI/badge.svg)](https://github.com/nexus-platform/nexus/actions)
[![Build Matrix](https://github.com/nexus-platform/nexus/workflows/Build%20Matrix/badge.svg)](https://github.com/nexus-platform/nexus/actions)
[![Documentation](https://github.com/nexus-platform/nexus/workflows/Documentation%20Build/badge.svg)](https://github.com/nexus-platform/nexus/actions)
[![codecov](https://codecov.io/gh/nexus-platform/nexus/branch/main/graph/badge.svg)](https://codecov.io/gh/nexus-platform/nexus)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-0.1.0-blue.svg)](CHANGELOG.md)

[English](#english) | [中文](README_CN.md)

---

<a name="english"></a>

**Nexus** is a professional embedded software development platform designed for building reliable, secure, and portable embedded applications across multiple MCU platforms with comprehensive testing and documentation.

## ✨ Key Features

### Core Layers
- **🔧 Hardware Abstraction Layer (HAL)** - Unified hardware interface with Kconfig-based compile-time configuration
- **⚙️ OS Abstraction Layer (OSAL)** - Support for FreeRTOS, RT-Thread, Zephyr, and bare-metal
- **� Framework Layer** - Config management, logging, shell, and initialization systems

### Development Experience
- **🌐 Cross-platform Development** - Windows, Linux, macOS with native simulation
- **🧪 Comprehensive Testing** - 1539+ tests with 100% code coverage for native platform
- **📚 Bilingual Documentation** - Complete English and Chinese documentation
- **🛠️ Python Build Tools** - Cross-platform scripts for build, test, and format
- **⚡ Kconfig Configuration** - Compile-time configuration system for all peripherals

### Advanced Features
- **🔒 Security** - Secure boot, TLS 1.3, hardware crypto acceleration (planned)
- **☁️ Cloud Integration** - AWS IoT, Azure IoT, Alibaba Cloud (planned)
- **🤖 TinyML** - TensorFlow Lite Micro support for edge AI (planned)

## 🎯 Supported Platforms

| Platform | Status | Peripherals | RTOS Support |
|----------|--------|-------------|--------------|
| **Native** | ✅ Production | Full simulation for testing | Bare-metal, FreeRTOS |
| **STM32F4** | ✅ Production | GPIO, UART, SPI, I2C, ADC, PWM, Timer, DMA, CAN | Bare-metal, FreeRTOS |
| **STM32H7** | 🚧 In Progress | + TrustZone, Crypto, Ethernet | Bare-metal, FreeRTOS |
| **GD32** | 🚧 In Progress | GPIO, UART, SPI, I2C | Bare-metal |
| **ESP32** | 📋 Planned | + WiFi, BLE, Touch | FreeRTOS |
| **nRF52** | 📋 Planned | + BLE, NFC, Crypto | FreeRTOS, Zephyr |

## 🚀 Quick Start

### Prerequisites

**All Platforms:**
- CMake 3.16+
- Git
- Python 3.8+ (for build scripts)

**For Native Build (Testing):**
- Windows: Visual Studio 2019+ or MSVC Build Tools
- Linux: GCC 9+ or Clang 10+
- macOS: Xcode Command Line Tools (Clang 12+)

**For ARM Cross-Compilation:**
- ARM GCC Toolchain 10.3+ (`arm-none-eabi-gcc`)
- Download: https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain

**For Documentation:**
- Doxygen 1.9+
- Python packages: `pip install sphinx breathe sphinx_rtd_theme`

### Build for Native (Host Testing)

```bash
# Clone repository
git clone https://github.com/nexus-platform/nexus.git
cd nexus

# Method 1: Using Python script (recommended, cross-platform)
python scripts/building/build.py

# Method 2: Using CMake Presets (recommended for CMake 3.19+)
cmake --preset native-debug      # Debug build
cmake --build --preset native-debug

cmake --preset native-release    # Release build
cmake --build --preset native-release

# Method 3: Using CMake directly
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNEXUS_PLATFORM=native
cmake --build build --config Release

# Run tests
python scripts/test/test.py
# Or: ctest --test-dir build -C Release --output-on-failure
```

### Build for STM32F4

```bash
# Method 1: Using Python script
python scripts/building/build.py --platform stm32f4 --toolchain arm-none-eabi

# Method 2: Using CMake Presets (recommended for CMake 3.19+)
cmake --preset stm32f4-debug     # Debug build
cmake --build --preset stm32f4-debug

cmake --preset stm32f4-release   # Release build
cmake --build --preset stm32f4-release

# Method 3: Using CMake directly
cmake -B build-stm32f4 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
    -DNEXUS_PLATFORM=stm32f4

cmake --build build-stm32f4 --config Release

# Output: build-stm32f4/applications/blinky/blinky.elf
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `NEXUS_PLATFORM` | `native` | Target platform: `native`, `stm32f4`, `stm32h7`, `gd32`, `esp32`, `nrf52` |
| `NEXUS_OSAL_BACKEND` | `baremetal` | OSAL backend: `baremetal`, `freertos`, `rtthread`, `zephyr` |
| `NEXUS_BUILD_TESTS` | `ON` | Build unit tests (native only) |
| `NEXUS_BUILD_EXAMPLES` | `ON` | Build example applications |
| `NEXUS_ENABLE_COVERAGE` | `OFF` | Enable code coverage analysis |
| `CMAKE_BUILD_TYPE` | `Debug` | Build type: `Debug`, `Release`, `MinSizeRel`, `RelWithDebInfo` |

### CMake Presets

The project includes CMakePresets.json for standardized configurations:

```bash
# List available presets
cmake --list-presets

# Use a preset
cmake --preset native-debug
cmake --build --preset native-debug

# Common presets:
# - native-debug: Native platform debug build
# - native-release: Native platform release build
# - stm32f4-debug: STM32F4 debug build
# - stm32f4-release: STM32F4 release build
```

## 📖 First Project

Create a simple LED blink application:

```c
#include "hal/nx_factory.h"
#include "osal/osal.h"

int main(void)
{
    /* Initialize OSAL and HAL */
    osal_init();
    nx_hal_init();

    /* Get GPIO device (Port A, Pin 5) */
    nx_gpio_write_t* led = nx_factory_gpio_write('A', 5);
    if (!led) {
        return -1;
    }

    /* Configure as output (done via Kconfig at compile-time) */
    led->set_mode(led, NX_GPIO_MODE_OUTPUT_PP);

    /* Blink loop */
    while (1) {
        led->toggle(led);
        osal_task_delay(500);  /* 500ms delay */
    }

    /* Cleanup (never reached) */
    nx_factory_gpio_release((nx_gpio_t*)led);
    nx_hal_deinit();
    return 0;
}
```

### Configure via Kconfig

```kconfig
# In your project's Kconfig or defconfig

# Enable GPIO Port A Pin 5
CONFIG_HAL_GPIO_A_5=y
CONFIG_HAL_GPIO_A_5_MODE=OUTPUT_PP
CONFIG_HAL_GPIO_A_5_PULL=NONE
CONFIG_HAL_GPIO_A_5_SPEED=LOW
CONFIG_HAL_GPIO_A_5_LEVEL=LOW
```

## 📁 Project Structure

```
nexus/
├── hal/                    # Hardware Abstraction Layer
│   ├── include/hal/        #   Public API headers
│   ├── src/                #   Common implementations
│   ├── docs/               #   Complete documentation (EN/CN)
│   └── Kconfig             #   HAL configuration options
├── osal/                   # OS Abstraction Layer
│   ├── include/osal/       #   Public API headers
│   ├── adapters/           #   RTOS adapters (baremetal, freertos, rtthread)
│   ├── docs/               #   Complete documentation (EN/CN)
│   └── Kconfig             #   OSAL configuration options
├── framework/              # High-level frameworks
│   ├── config/             #   Configuration management system
│   ├── log/                #   Logging system
│   ├── shell/              #   Command shell
│   └── init/               #   Initialization framework
├── platforms/              # Platform-specific implementations
│   ├── native/             #   Host simulation (Windows/Linux/macOS)
│   ├── stm32/              #   STM32 family (F4, H7)
│   ├── gd32/               #   GigaDevice GD32
│   ├── esp32/              #   Espressif ESP32
│   ├── nrf52/              #   Nordic nRF52
│   └── Kconfig             #   Platform configuration
├── applications/           # Example applications
│   ├── blinky/             #   LED blink example
│   ├── shell_demo/         #   Command shell demo
│   ├── config_demo/        #   Configuration system demo
│   └── freertos_demo/      #   FreeRTOS integration demo
├── tests/                  # Comprehensive test suite (1539+ tests)
│   ├── hal/                #   HAL unit and property tests
│   ├── osal/               #   OSAL tests
│   ├── config/             #   Config framework tests
│   ├── log/                #   Log framework tests
│   ├── shell/              #   Shell framework tests
│   ├── init/               #   Init framework tests
│   └── integration/        #   Integration tests
├── docs/                   # Documentation
│   ├── api/                #   Doxygen API documentation
│   ├── sphinx/             #   User guides (EN/CN)
│   └── requirements/       #   Requirements and design docs
├── scripts/                # Build and utility scripts
│   ├── building/           #   Build scripts (Python/Bash/Batch)
│   ├── test/               #   Test scripts
│   ├── tools/              #   Format, clean, docs scripts
│   └── coverage/           #   Coverage analysis scripts
├── cmake/                  # CMake modules
│   ├── modules/            #   Helper functions
│   └── toolchains/         #   Cross-compilation toolchains
├── vendors/                # Vendor SDKs and libraries
│   ├── st/                 #   STMicroelectronics
│   ├── espressif/          #   Espressif
│   ├── nordic/             #   Nordic Semiconductor
│   └── arm/                #   ARM CMSIS
├── ext/                    # External dependencies
│   ├── freertos/           #   FreeRTOS kernel
│   └── googletest/         #   Google Test framework
└── .github/workflows/      # CI/CD pipelines
    ├── build.yml           #   Multi-platform build
    ├── test.yml            #   Unit tests with coverage
    └── docs.yml            #   Documentation deployment
```

## 📚 Documentation

### Online Documentation

Visit our comprehensive documentation site: **[nexus-platform.github.io/nexus](https://nexus-platform.github.io/nexus/)**

- **English**: https://nexus-platform.github.io/nexus/en/
- **中文**: https://nexus-platform.github.io/nexus/zh_CN/
- **API Reference**: https://nexus-platform.github.io/nexus/api/

### Build Documentation Locally

```bash
# Install dependencies
pip install sphinx breathe sphinx_rtd_theme

# Build all documentation (API + User Guides)
python scripts/tools/docs.py

# Or build separately:

# API documentation (Doxygen)
doxygen Doxyfile

# User guides (Sphinx)
cd docs/sphinx
sphinx-build -b html . _build/html/en                    # English
sphinx-build -b html -D language=zh_CN . _build/html/cn  # Chinese
```

### Module Documentation

Each module has comprehensive documentation:

#### HAL (Hardware Abstraction Layer)
- [📖 Overview](hal/docs/README.md)
- [👤 User Guide](hal/docs/USER_GUIDE.md) - Complete API usage and examples
- [🏗️ Design Document](hal/docs/DESIGN.md) - Architecture and implementation
- [🧪 Testing Guide](hal/docs/TEST_GUIDE.md) - Test strategy and cases
- [🔧 Porting Guide](hal/docs/PORTING_GUIDE.md) - How to port to new platforms
- [🔍 Troubleshooting](hal/docs/TROUBLESHOOTING.md) - Common issues and solutions

#### OSAL (OS Abstraction Layer)
- [📖 Overview](osal/docs/README.md)
- [👤 User Guide](osal/docs/USER_GUIDE.md) - Tasks, synchronization, memory
- [🏗️ Design Document](osal/docs/DESIGN.md) - RTOS adapter architecture
- [🧪 Testing Guide](osal/docs/TEST_GUIDE.md) - Unit and integration tests
- [🔧 Porting Guide](osal/docs/PORTING_GUIDE.md) - Adapt to new RTOS
- [🔍 Troubleshooting](osal/docs/TROUBLESHOOTING.md) - Debugging tips

#### Framework Modules
- [⚙️ Config System](framework/config/docs/README.md) - Configuration management
- [📝 Log System](framework/log/docs/README.md) - Logging framework
- [💻 Shell System](framework/shell/docs/README.md) - Command shell
- [🚀 Init System](framework/init/docs/README.md) - Initialization framework

## 🛠️ Development

### Using Python Scripts (Recommended)

```bash
# Build
python scripts/building/build.py                 # Debug build
python scripts/building/build.py -t release      # Release build
python scripts/building/build.py -c              # Clean build
python scripts/building/build.py -j 8            # Parallel build (8 jobs)

# Test
python scripts/test/test.py                      # Run all tests
python scripts/test/test.py -f "GPIO*"           # Filter tests
python scripts/test/test.py -v                   # Verbose output
python scripts/test/test.py --xml report.xml     # Generate XML report

# Format code
python scripts/tools/format.py                   # Format all code
python scripts/tools/format.py --check           # Check formatting only

# Clean
python scripts/tools/clean.py                    # Clean build artifacts

# Generate documentation
python scripts/tools/docs.py                     # Build all docs
```

### Code Style

This project follows strict coding standards:

- **C Standard**: C11
- **C++ Standard**: C++17 (tests use C++20)
- **Line Length**: 80 characters maximum
- **Indentation**: 4 spaces (no tabs)
- **Naming**: snake_case for functions and variables
- **Comments**: Doxygen with backslash style (`\brief`, `\param`)

Format code before committing:

```bash
python scripts/tools/format.py
```

### Doxygen Comment Style

Use backslash style for Doxygen comments:

```c
/**
 * \file            nx_gpio.h
 * \brief           GPIO device interface definition
 * \author          Nexus Team
 */

/**
 * \brief           Get GPIO device with write capability
 * \param[in]       port: GPIO port ('A'-'K')
 * \param[in]       pin: GPIO pin number (0-15)
 * \return          GPIO write interface pointer, NULL on failure
 */
nx_gpio_write_t* nx_factory_gpio_write(char port, uint8_t pin);
```

### Running Tests

```bash
# Using Python script (recommended)
python scripts/test/test.py                      # All tests
python scripts/test/test.py -f "GPIO*"           # Specific suite
python scripts/test/test.py -l unit              # By label
python scripts/test/test.py -v                   # Verbose

# Using CTest directly
cd build
ctest -C Release --output-on-failure             # All tests
ctest -C Release -R "GPIO*"                      # Specific suite
ctest -C Release -L unit                         # By label
ctest -C Release -j8                             # Parallel (8 jobs)
```

### Test Statistics

Current test suite contains:

- **Total Tests**: 1539+ tests
- **HAL Tests**: ~400 tests (unit + property-based)
- **OSAL Tests**: ~200 tests
- **Config Tests**: ~300 tests
- **Log Tests**: ~130 tests
- **Shell Tests**: ~400 tests
- **Init Tests**: ~15 tests
- **Integration Tests**: ~40 tests

### Code Coverage

Generate code coverage reports:

```bash
# Linux/WSL
cd scripts/coverage
./run_coverage_linux.sh

# Windows (PowerShell)
cd scripts\coverage
.\run_coverage_windows.ps1

# View report
# Linux: xdg-open ../../coverage_html/index.html
# Windows: start ..\..\coverage_report\html\index.html
```

**Target**: 100% code coverage for native platform HAL implementations.

## 🔄 CI/CD

GitHub Actions workflows automatically run on every push and pull request:

| Workflow | Description | Triggers |
|----------|-------------|----------|
| **ci.yml** | Unified continuous integration pipeline | Push, PR |
| **build-matrix.yml** | Multi-platform build testing (Windows, Linux, macOS, ARM) | Push, PR |
| **docs-build.yml** | Build and deploy documentation to GitHub Pages | Push to main |
| **quality-checks.yml** | Code quality validation and static analysis | Push, PR |
| **performance.yml** | Performance benchmarking and regression testing | Push to main, Manual |
| **release.yml** | Automated release process and artifact publishing | Tag push |

### Modular Architecture

The CI/CD system uses a modular architecture with reusable actions:

- **Reusable Actions** in `.github/actions/`:
  - `setup-build/` - Common build environment setup
  - Shared across multiple workflows for consistency

### CI Status

- ✅ All platforms build successfully
- ✅ 1539+ tests passing
- ✅ Code coverage > 95%
- ✅ Documentation builds successfully
- ✅ Quality checks passing

## 🤝 Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for:

- Development environment setup
- Code style guidelines
- Testing requirements (all contributions must include tests)
- Pull request process
- Documentation guidelines

Quick checklist before submitting a PR:

- [ ] Code follows style guidelines (`python scripts/tools/format.py`)
- [ ] All tests pass (`python scripts/test/test.py`)
- [ ] New code has corresponding tests
- [ ] Documentation is updated
- [ ] Commit messages follow conventional commits

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🌟 Community

- **Issues**: [GitHub Issues](https://github.com/nexus-platform/nexus/issues)
- **Discussions**: [GitHub Discussions](https://github.com/nexus-platform/nexus/discussions)
- **Documentation**: [Online Docs](https://nexus-platform.github.io/nexus/)
- **Changelog**: [CHANGELOG.md](CHANGELOG.md)

## 🗺️ Roadmap

### v0.1.0 (Current)

- ✅ HAL core functionality (GPIO, UART, SPI, I2C, ADC, PWM, CAN)
- ✅ OSAL core functionality (tasks, synchronization, memory)
- ✅ Framework modules (Config, Log, Shell, Init)
- ✅ STM32F4 platform support
- ✅ Native platform for testing
- ✅ Kconfig configuration system
- ✅ Comprehensive testing (1539+ tests)
- ✅ Complete bilingual documentation

### v0.2.0 (Planned)

- 🚧 STM32H7 platform support
- 🚧 GD32 platform support
- 🚧 DMA advanced features
- 🚧 Low-power management
- 🚧 Enhanced security features

### v1.0.0 (Future)

- 📋 ESP32 platform support
- 📋 nRF52 platform support
- 📋 Cloud integration (AWS IoT, Azure IoT)
- 📋 TinyML support
- 📋 Secure boot implementation

## 🙏 Acknowledgments

Thanks to all contributors who have helped make Nexus better!

Special thanks to:
- FreeRTOS team for the excellent RTOS
- Google Test team for the testing framework
- Doxygen and Sphinx teams for documentation tools

---

**Made with ❤️ by the Nexus Team**

*Building the future of embedded systems, one commit at a time.*
