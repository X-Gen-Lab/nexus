# Native Platform

The Native platform provides a simulated hardware environment for development and testing on PC (Windows, Linux, macOS).

## Directory Structure

```
platforms/native/
├── Kconfig                          # Native platform top-level configuration
├── CMakeLists.txt                   # Build configuration
├── README.md                        # This file
├── include/                         # Public header files
│   └── native_platform.h            # Platform common definitions
│
├── config/                          # Kconfig configuration files
│   ├── Kconfig.uart                 # UART configuration
│   ├── Kconfig.gpio                 # GPIO configuration
│   ├── Kconfig.spi                  # SPI configuration
│   └── Kconfig.i2c                  # I2C configuration
│
└── src/                             # Source code directory
    ├── platform/                    # Platform initialization
    │   ├── nx_platform_init.c       # Platform init/deinit
    │   └── README.md                # Platform initialization docs
    │
    ├── resource/                    # Resource managers
    │   ├── nx_dma_native.c          # DMA manager
    │   ├── nx_isr_native.c          # ISR manager
    │   └── README.md                # Resource manager docs
    │
    ├── uart/                        # UART peripheral
    │   ├── Kconfig                  # UART configuration
    │   ├── nx_uart_device.c         # Device registration
    │   ├── nx_uart_async.c          # Async interface
    │   ├── nx_uart_sync.c           # Sync interface
    │   ├── nx_uart_lifecycle.c      # Lifecycle management
    │   ├── nx_uart_power.c          # Power management
    │   ├── nx_uart_diagnostic.c     # Diagnostic interface
    │   ├── nx_uart_helpers.c/h      # Helper functions
    │   ├── nx_uart_types.h          # Type definitions
    │   └── README.md                # UART implementation docs
    │
    ├── gpio/                        # GPIO peripheral
    ├── spi/                         # SPI peripheral
    ├── i2c/                         # I2C peripheral
    ├── timer/                       # Timer peripheral
    ├── adc/                         # ADC peripheral
    ├── dac/                         # DAC peripheral
    ├── flash/                       # Flash peripheral
    ├── rtc/                         # RTC peripheral
    ├── watchdog/                    # Watchdog peripheral
    ├── crc/                         # CRC peripheral
    ├── usb/                         # USB peripheral
    ├── sdio/                        # SDIO peripheral
    └── option_bytes/                # Option Bytes
```

## Purpose

The Native platform serves multiple purposes:

1. **Development**: Develop and test application code on PC without hardware
2. **Testing**: Run automated tests in CI/CD pipelines
3. **Debugging**: Debug application logic without hardware dependencies
4. **Prototyping**: Quickly prototype features before hardware is available

## Architecture

The Native platform follows a modular architecture:

- **Platform Layer**: Initialization and resource management
- **Peripheral Modules**: Each peripheral in its own directory
- **Interface Separation**: Different interfaces in separate files
- **Configuration Management**: Kconfig-based compile-time configuration

## Building

The Native platform is built as part of the Nexus project:

```bash
# Configure for Native platform
cmake -B build -DPLATFORM=native

# Build
cmake --build build
```

## Configuration

Platform configuration is managed through Kconfig. The configuration follows a three-level hierarchy:

1. **Platform Selection** (`platforms/Kconfig`): Choose Native platform
2. **Platform Settings** (`platforms/native/Kconfig`): Configure platform-level options
3. **Peripheral Configuration** (`platforms/native/src/<peripheral>/Kconfig`): Configure individual peripherals

For detailed configuration instructions, see [KCONFIG_GUIDE.md](KCONFIG_GUIDE.md).

### Quick Start

```bash
# Copy example configuration
cp platforms/native/defconfig.example .config

# Modify as needed
# ...

# Build with configuration
cmake -B build
cmake --build build
```

### Configuration Examples

#### Enable UART with 2 instances

```kconfig
CONFIG_NX_PLATFORM_NATIVE=y
CONFIG_NX_UART_ENABLED=y
CONFIG_NX_UART_NUM_INSTANCES=2
CONFIG_NX_UART_0_ENABLED=y
CONFIG_NX_UART_1_ENABLED=y
```

#### Enable GPIO with custom port configuration

```kconfig
CONFIG_NX_PLATFORM_NATIVE=y
CONFIG_NX_GPIO_ENABLED=y
CONFIG_NX_GPIO_NUM_PORTS=3
CONFIG_NX_GPIO_PINS_PER_PORT=16
```

#### Enable SPI and I2C for communication testing

```kconfig
CONFIG_NX_PLATFORM_NATIVE=y
CONFIG_NX_SPI_ENABLED=y
CONFIG_NX_SPI_NUM_INSTANCES=2
CONFIG_NX_I2C_ENABLED=y
CONFIG_NX_I2C_NUM_INSTANCES=2
```

### Verify Configuration

```bash
python scripts/verify_kconfig.py
```

## Testing

The Native platform provides extensive testing support:

- **Data Injection**: Simulate received data for communication peripherals (UART, SPI, I2C)
- **Interrupt Triggering**: Simulate external interrupts for GPIO
- **State Queries**: Verify internal state for all peripherals
- **Reset Functions**: Clean up test state between tests

For detailed testing support documentation, see [TEST_SUPPORT.md](TEST_SUPPORT.md).

### Example: Testing UART Communication

```c
#include "hal/nx_uart.h"
#include "platforms/native/src/uart/nx_uart_test.h"

/* Get UART instance */
nx_uart_t* uart = nx_factory_uart(0);

/* Initialize */
nx_lifecycle_t* lc = uart->get_lifecycle(uart);
lc->init(lc);

/* Inject test data */
const uint8_t test_data[] = "Hello";
nx_uart_test_inject_rx_data(0, test_data, sizeof(test_data));

/* Read injected data */
nx_rx_async_t* rx = uart->get_rx_async(uart);
uint8_t buffer[32];
size_t len = sizeof(buffer);
rx->receive(rx, buffer, &len);

/* Verify state */
nx_uart_test_state_t state;
nx_uart_test_get_state(0, &state);
assert(state.rx_count == sizeof(test_data));

/* Clean up */
nx_uart_test_reset(0);
lc->deinit(lc);
```

## Backward Compatibility

The refactored Native platform maintains backward compatibility:

- All public interfaces remain unchanged
- Factory functions continue to work as before
- Existing tests continue to pass
- Migration guide provided for any changes

## Contributing

When adding new peripheral support:

1. Create a new directory under `src/<peripheral>/`
2. Follow the modular file organization pattern
3. Provide Kconfig configuration
4. Add README.md documentation
5. Implement test support functions
6. Update CMakeLists.txt

## Documentation

- [Platform Initialization](src/platform/README.md)
- [Resource Managers](src/resource/README.md)
- Individual peripheral README files in each directory

## License

Copyright (c) 2026 Nexus Team
