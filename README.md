# Nexus Embedded Platform

[![Build Status](https://github.com/nexus-platform/nexus/workflows/Build/badge.svg)](https://github.com/nexus-platform/nexus/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Documentation](https://img.shields.io/badge/docs-online-blue.svg)](https://nexus-platform.github.io/nexus/)

[English](README.md) | [中文](README.md)

**Nexus** is a world-class embedded software development platform designed for building reliable, secure, and portable embedded applications.

## Features

- **Hardware Abstraction Layer (HAL)** - Unified hardware interface across multiple MCU platforms
- **OS Abstraction Layer (OSAL)** - Support for FreeRTOS, RT-Thread, Zephyr, and bare-metal
- **Security** - Secure boot, TLS 1.3, hardware crypto acceleration
- **Cloud Integration** - AWS IoT, Azure IoT, Alibaba Cloud
- **TinyML** - TensorFlow Lite Micro support for edge AI
- **Cross-platform** - Develop on Windows, Linux, or macOS

## Supported Platforms

| Platform | Status | Features |
|----------|--------|----------|
| STM32F4 | ✅ Supported | GPIO, UART, SPI, I2C, ADC, Timer |
| STM32H7 | 🚧 Planned | + TrustZone, Crypto |
| ESP32 | 🚧 Planned | + WiFi, BLE |
| nRF52 | 🚧 Planned | + BLE, Crypto |
| Native | ✅ Supported | Host simulation for testing |

## Quick Start

### Prerequisites

**All Platforms:**
- CMake 3.16+
- Git

**For Native Build (Testing):**
- Windows: Visual Studio 2019+ or MSVC Build Tools
- Linux: GCC 9+
- macOS: Clang 12+

**For ARM Cross-Compilation:**
- ARM GCC Toolchain 10.3+ (`arm-none-eabi-gcc`)

**For Documentation:**
- Doxygen 1.9+
- Python 3.8+ with Sphinx (`pip install sphinx breathe`)

### Build for Native (Host Testing)

```bash
# Clone repository
git clone https://github.com/nexus-platform/nexus.git
cd nexus

# Configure for native platform
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNEXUS_PLATFORM=native

# Build
cmake --build build --config Release

# Run tests
ctest --test-dir build -C Release --output-on-failure
```

### Build for STM32F4

```bash
# Configure for STM32F4
cmake -B build-stm32f4 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
    -DNEXUS_PLATFORM=stm32f4

# Build
cmake --build build-stm32f4 --config Release

# Output: build-stm32f4/applications/blinky/blinky.elf
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `NEXUS_PLATFORM` | `native` | Target platform: `native`, `stm32f4` |
| `NEXUS_BUILD_TESTS` | `ON` | Build unit tests |
| `NEXUS_BUILD_EXAMPLES` | `ON` | Build example applications |
| `NEXUS_ENABLE_COVERAGE` | `OFF` | Enable code coverage |

### Build Documentation

```bash
# Generate API documentation (Doxygen)
doxygen Doxyfile

# Build Sphinx documentation (English)
cd docs/sphinx
python -m sphinx -b html . _build/html/en

# Build Sphinx documentation (Chinese)
python -m sphinx -b html . _build/html/cn -D master_doc=index_cn -D language=zh_CN

# Or use the build script (Windows)
build_docs.bat
```

## First Project

```c
#include "hal/hal_gpio.h"
#include "hal/hal_system.h"

int main(void)
{
    hal_system_init();

    hal_gpio_config_t config = {
        .direction   = HAL_GPIO_DIR_OUTPUT,
        .pull        = HAL_GPIO_PULL_NONE,
        .output_mode = HAL_GPIO_OUTPUT_PP,
        .speed       = HAL_GPIO_SPEED_LOW,
        .init_level  = HAL_GPIO_LEVEL_LOW
    };

    hal_gpio_init(HAL_GPIO_PORT_A, 5, &config);

    while (1) {
        hal_gpio_toggle(HAL_GPIO_PORT_A, 5);
        hal_delay_ms(500);
    }
}
```

## Project Structure

```
nexus/
├── hal/                    # Hardware Abstraction Layer
│   ├── include/hal/        #   Public headers
│   └── src/                #   Common implementations
├── osal/                   # OS Abstraction Layer
│   ├── include/osal/       #   Public headers
│   └── adapters/           #   RTOS adapters (baremetal, freertos)
├── platforms/              # Platform-specific implementations
│   ├── native/             #   Host simulation (Windows/Linux/macOS)
│   └── stm32f4/            #   STM32F4 HAL implementations
├── applications/           # Example applications
│   └── blinky/             #   LED blink example
├── tests/                  # Unit tests (Google Test)
│   └── hal/                #   HAL unit tests
├── docs/                   # Documentation
│   ├── api/                #   Doxygen output
│   ├── sphinx/             #   Sphinx documentation (EN/CN)
│   └── requirements/       #   PRD and roadmap
├── cmake/                  # CMake modules
│   ├── modules/            #   Helper functions
│   └── toolchains/         #   Cross-compilation toolchains
└── .github/workflows/      # CI/CD pipelines
```

## Documentation

- **API Reference**: Run `doxygen Doxyfile` then open `docs/api/html/index.html`
- **User Guide**: Run Sphinx build then open `docs/sphinx/_build/html/index.html`
- **[Contributing Guide](CONTRIBUTING.md)**: How to contribute
- **[Changelog](CHANGELOG.md)**: Version history

## Development

### Code Style

This project uses:
- `.clang-format` - Code formatting (80 char limit, 4 space indent)
- `.clang-tidy` - Static analysis
- `.editorconfig` - Editor settings

Format code before committing:
```bash
clang-format -i hal/**/*.c hal/**/*.h
```

### Running Tests

```bash
# Build with tests enabled
cmake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
cmake --build build --config Release

# Run all tests
ctest --test-dir build -C Release --output-on-failure

# Run specific test
./build/tests/Release/nexus_tests --gtest_filter="HalGpioTest.*"
```

### Doxygen Comment Style

Use backslash style for Doxygen comments:
```c
/**
 * \file            hal_gpio.h
 * \brief           GPIO Hardware Abstraction Layer
 */

/**
 * \brief           Initialize GPIO pin
 * \param[in]       port: GPIO port
 * \param[in]       pin: Pin number (0-15)
 * \param[in]       config: Pointer to configuration
 * \return          HAL_OK on success
 */
hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin,
                           const hal_gpio_config_t* config);
```

## CI/CD

GitHub Actions workflows:
- **build.yml**: Multi-platform build (Windows, Linux, macOS) + ARM cross-compilation
- **test.yml**: Unit tests with coverage + sanitizers + MISRA checks

## Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

# Nexus 嵌入式平台

**Nexus** 是一个世界级的嵌入式软件开发平台，专为构建可靠、安全、可移植的嵌入式应用而设计。

## 功能特性

- **硬件抽象层 (HAL)** - 跨多种 MCU 平台的统一硬件接口
- **操作系统抽象层 (OSAL)** - 支持 FreeRTOS、RT-Thread、Zephyr 和裸机
- **安全特性** - 安全启动、TLS 1.3、硬件加密加速
- **云端集成** - AWS IoT、Azure IoT、阿里云
- **TinyML** - 支持 TensorFlow Lite Micro 边缘 AI
- **跨平台开发** - 支持 Windows、Linux、macOS 开发环境

## 快速开始

### 环境要求

- CMake 3.16+
- Git
- Windows: Visual Studio 2019+ 或 MSVC Build Tools
- ARM 交叉编译: arm-none-eabi-gcc 10.3+

### 构建 (本地测试)

```bash
git clone https://github.com/nexus-platform/nexus.git
cd nexus

# 配置
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNEXUS_PLATFORM=native

# 构建
cmake --build build --config Release

# 运行测试
ctest --test-dir build -C Release --output-on-failure
```

### 构建 STM32F4

```bash
cmake -B build-stm32f4 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
    -DNEXUS_PLATFORM=stm32f4

cmake --build build-stm32f4 --config Release
```

### 构建文档

```bash
# API 文档
doxygen Doxyfile

# 用户文档 (中文)
cd docs/sphinx
python -m sphinx -b html . _build/html/cn -D master_doc=index_cn -D language=zh_CN
```

## 文档

- **API 参考**: 运行 `doxygen Doxyfile` 后打开 `docs/api/html/index.html`
- **用户指南**: 运行 Sphinx 构建后打开 `docs/sphinx/_build/html/cn/index_cn.html`
- **[贡献指南](CONTRIBUTING.md)**: 如何参与贡献
- **[更新日志](CHANGELOG.md)**: 版本历史

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。
