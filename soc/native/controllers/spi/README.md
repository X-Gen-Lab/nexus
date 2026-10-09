# SPI Peripheral Implementation

This directory contains the Native platform implementation of the SPI (Serial Peripheral Interface) peripheral.

## Overview

The SPI implementation provides a simulated SPI controller for development and testing. It supports both synchronous and asynchronous transfer modes, along with full lifecycle and power management.

## File Organization

```
spi/
├── Kconfig                  # SPI configuration options
├── nx_spi_device.c          # Device registration and initialization
├── nx_spi_async.c           # Asynchronous transfer interface
├── nx_spi_sync.c            # Synchronous transfer interface
├── nx_spi_lifecycle.c       # Lifecycle management (init/deinit/suspend/resume)
├── nx_spi_power.c           # Power management interface
├── nx_spi_diagnostic.c      # Diagnostic and statistics interface
├── nx_spi_helpers.c/h       # Helper functions
├── nx_spi_types.h           # Type definitions and structures
├── nx_spi_test.h            # Test support functions
└── README.md                # This file
```

## Implementation Details

### Device Registration

The SPI device registration uses the `NX_DEVICE_REGISTER` macro to register devices based on Kconfig configuration. Each enabled SPI instance is automatically registered during platform initialization.

```c
/* Configuration from Kconfig */
#define NX_SPI_CONFIG(index) \
    static const nx_spi_platform_config_t spi_config_##index = { \
        .spi_index = index, \
    }

/* Register all enabled instances */
NX_TRAVERSE_EACH_INSTANCE(NX_SPI_DEVICE_REGISTER, NX_DEVICE_TYPE_SPI);
```

### Interfaces

#### Asynchronous Transfer Interface

Provides non-blocking SPI transfer operations:

- `transfer_async()`: Initiate asynchronous transfer
- Simulates hardware behavior with internal buffers
- Returns immediately without blocking

#### Synchronous Transfer Interface

Provides blocking SPI transfer operations:

- `transfer_sync()`: Perform synchronous transfer with timeout
- Simulates timeout behavior (ignored in Native platform)
- Returns after transfer completes

#### Lifecycle Interface

Manages device lifecycle:

- `init()`: Initialize SPI device and buffers
- `deinit()`: Clean up resources
- `suspend()`: Suspend device operation
- `resume()`: Resume device operation
- `get_state()`: Query current device state

#### Power Management Interface

Controls device power state (simulated):

- `enable()`: Enable device power
- `disable()`: Disable device power
- `is_enabled()`: Check power state
- `set_callback()`: Register power state callback

#### Diagnostic Interface

Provides status and statistics:

- `get_status()`: Get current device status
- `get_statistics()`: Get transfer statistics
- `clear_statistics()`: Reset statistics counters

### Data Structures

#### Implementation Structure

```c
typedef struct {
    nx_spi_t base;                  /* Base SPI interface */
    nx_transfer_async_t transfer_async;  /* Async interface */
    nx_transfer_sync_t transfer_sync;    /* Sync interface */
    nx_lifecycle_t lifecycle;       /* Lifecycle interface */
    nx_power_t power;               /* Power interface */
    nx_diagnostic_t diagnostic;     /* Diagnostic interface */
    nx_spi_state_t* state;          /* Runtime state */
    nx_device_t* device;            /* Device descriptor */
} nx_spi_impl_t;
```

#### State Structure

```c
typedef struct {
    uint8_t index;                  /* Instance index */
    nx_spi_config_t config;         /* Configuration */
    nx_spi_stats_t stats;           /* Statistics */
    /* Buffers and flags */
    bool initialized;
    bool suspended;
    bool busy;
} nx_spi_state_t;
```

## Configuration

SPI configuration is managed through Kconfig:

```kconfig
CONFIG_NX_SPI_ENABLED=y
CONFIG_NX_SPI_NUM_INSTANCES=2
CONFIG_NX_SPI_0_ENABLED=y
CONFIG_NX_SPI_1_ENABLED=y
```

## Usage Example

```c
#include "hal/nx_spi.h"

/* Get SPI instance */
nx_spi_t* spi = nx_factory_spi(0);

/* Initialize */
nx_lifecycle_t* lc = spi->get_lifecycle(spi);
lc->init(lc);

/* Perform synchronous transfer */
nx_transfer_sync_t* sync = spi->get_transfer_sync(spi);
uint8_t tx_data[] = {0x01, 0x02, 0x03};
uint8_t rx_data[3];
sync->transfer(sync, tx_data, rx_data, sizeof(tx_data), 1000);

/* Clean up */
lc->deinit(lc);
```

## Testing Support

The SPI implementation provides test support functions in `nx_spi_test.h`:

- `nx_spi_test_inject_rx_data()`: Inject simulated received data
- `nx_spi_test_get_state()`: Query internal state for verification
- `nx_spi_test_reset()`: Reset device state for testing

### Test Example

```c
#include "platforms/native/src/spi/nx_spi_test.h"

/* Inject test data */
uint8_t test_data[] = {0xAA, 0xBB, 0xCC};
nx_spi_test_inject_rx_data(0, test_data, sizeof(test_data));

/* Perform transfer and verify */
nx_transfer_sync_t* sync = spi->get_transfer_sync(spi);
uint8_t rx_buffer[3];
sync->transfer(sync, NULL, rx_buffer, sizeof(rx_buffer), 1000);

/* Verify received data matches injected data */
assert(memcmp(rx_buffer, test_data, sizeof(test_data)) == 0);

/* Clean up test state */
nx_spi_test_reset(0);
```

## Error Handling

All functions return `nx_status_t` status codes:

- `NX_OK`: Operation successful
- `NX_ERR_NULL_PTR`: Null pointer parameter
- `NX_ERR_INVALID_PARAM`: Invalid parameter
- `NX_ERR_NOT_INIT`: Device not initialized
- `NX_ERR_BUSY`: Device busy
- `NX_ERR_TIMEOUT`: Operation timeout

## Performance Considerations

- Uses efficient circular buffers for data storage
- Minimal function call overhead with static inline helpers
- No unnecessary memory copies in transfer operations

## Related Documentation

- [HAL SPI Interface](../../../../hal/include/hal/nx_spi.h)
- [Platform README](../../README.md)
- [Test Support Documentation](../../TEST_SUPPORT.md)
