# Nexus Embedded Platform

[![Build Status](https://github.com/nexus-platform/nexus/workflows/Build/badge.svg)](https://github.com/nexus-platform/nexus/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Documentation](https://img.shields.io/badge/docs-online-blue.svg)](https://nexus-platform.github.io/nexus/)

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
| STM32H7 | ✅ Supported | + TrustZone, Crypto |
| ESP32 | ✅ Supported | + WiFi, BLE |
| nRF52 | 🚧 In Progress | + BLE, Crypto |

## Quick Start

### Prerequisites

- CMake 3.16+
- ARM GCC Toolchain 10.3+
- Ninja (recommended) or Make

### Build

```bash
# Clone repository
git clone https://github.com/nexus-platform/nexus.git
cd nexus

# Configure
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DNEXUS_PLATFORM=stm32f4

# Build
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

### First Project

```c
#include "hal/hal_gpio.h"

int main(void)
{
    hal_gpio_config_t config = {
        .direction = HAL_GPIO_DIR_OUTPUT,
        .pull = HAL_GPIO_PULL_NONE,
        .output_mode = HAL_GPIO_OUTPUT_PP,
    };

    hal_gpio_init(HAL_GPIO_PORT_A, 5, &config);

    while (1) {
        hal_gpio_toggle(HAL_GPIO_PORT_A, 5);
        hal_delay_ms(500);
    }
}
```

## Documentation

- [Getting Started Guide](docs/guides/getting_started.md)
- [API Reference](https://nexus-platform.github.io/nexus/api/)
- [Porting Guide](docs/guides/porting.md)
- [Security Guide](docs/guides/security.md)

## Project Structure

```
nexus/
├── hal/                # Hardware Abstraction Layer
├── osal/               # OS Abstraction Layer
├── middleware/         # Middleware components
├── security/           # Security modules
├── components/         # Reusable components
├── platforms/          # Platform-specific code
├── boards/             # Board support packages
├── applications/       # Example applications
├── tests/              # Unit and integration tests
├── docs/               # Documentation
├── tools/              # Development tools
└── third_party/        # Third-party libraries
```

## Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contact

- GitHub Issues: [Report a bug](https://github.com/nexus-platform/nexus/issues)
- Discussions: [Join the community](https://github.com/nexus-platform/nexus/discussions)
