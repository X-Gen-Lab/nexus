# UART Module - Native Platform

## Overview

This directory contains the UART peripheral implementation for the Native platform. The implementation provides simulated UART functionality for development and testing on PC.

## Architecture

The UART module follows a modular architecture with clear separation of concerns:

```
uart/
├── nx_uart_types.h          # Type definitions
├── nx_uart_helpers.h/c      # Helper functions
├── nx_uart_device.c         # Device registration
├── nx_uart_async.c          # Async interface
├── nx_uart_sync.c           # Sync interface
├── nx_uart_lifecycle.c      # Lifecycle management
├── nx_uart_power.c          # Power management
├── nx_uart_diagnostic.c     # Diagnostic interface
├── Kconfig                  # Configuration
└── README.md                # This file
```

## File Descriptions

### nx_uart_types.h
Defines all type structures used by the UART implementation:
- `nx_uart_platform_config_t`: Platform configuration from Kconfig
- `nx_uart_buffer_t`: Circular buffer structure
- `nx_uart_config_t`: Runtime configuration
- `nx_uart_state_t`: Runtime state and statistics
- `nx_uart_impl_t`: Implementation structure with all interfaces

### nx_uart_helpers.h/c
Provides helper functions for:
- Getting implementation from base interface
- Circular buffer operations (init, read, write, get count)

### nx_uart_device.c
Handles device registration and initialization:
- Device registration using `NX_DEVICE_REGISTER` macro
- Instance traversal using `NX_TRAVERSE_EACH_INSTANCE`
- Platform configuration from Kconfig
- Legacy factory functions for backward compatibility
- Test support functions (inject RX data, reset, etc.)

### nx_uart_async.c
Implements asynchronous TX/RX interfaces:
- `tx_async_send`: Non-blocking send (writes to stdout)
- `tx_async_get_state`: Get TX state
- `rx_async_receive`: Non-blocking receive (reads from buffer)

### nx_uart_sync.c
Implements synchronous TX/RX interfaces:
- `tx_sync_send`: Blocking send with timeout (simulated)
- `rx_sync_receive`: Blocking receive with timeout
- `rx_sync_receive_all`: Receive exact amount or timeout

### nx_uart_lifecycle.c
Implements lifecycle management:
- `init`: Initialize buffers and state
- `deinit`: Clean up resources
- `suspend`: Suspend operation
- `resume`: Resume operation
- `get_state`: Get current device state

### nx_uart_power.c
Implements power management (simulated):
- `enable`: Enable power (no-op)
- `disable`: Disable power (no-op)
- `is_enabled`: Check if powered
- `set_callback`: Set power callback (no-op)

### nx_uart_diagnostic.c
Implements diagnostic interface:
- `get_status`: Get current status
- `get_statistics`: Get statistics (TX/RX counts, errors)
- `clear_statistics`: Clear statistics

### Kconfig
Defines compile-time configuration:
- Enable/disable UART instances (0-3)
- Baud rate, data bits, stop bits, parity
- TX/RX buffer sizes

## Usage

### Getting a UART Instance

```c
/* Using factory function (legacy) */
nx_uart_t* uart = nx_uart_native_get(0);

/* Using device registration (new) */
nx_uart_t* uart = (nx_uart_t*)nx_device_kconfig_get("UART0");
```

### Initializing UART

```c
nx_lifecycle_t* lc = uart->get_lifecycle(uart);
nx_status_t status = lc->init(lc);
```

### Sending Data (Async)

```c
nx_tx_async_t* tx = uart->get_tx_async(uart);
const uint8_t data[] = "Hello";
nx_status_t status = tx->send(tx, data, sizeof(data));
```

### Receiving Data (Sync)

```c
nx_rx_sync_t* rx = uart->get_rx_sync(uart);
uint8_t buffer[64];
size_t len = sizeof(buffer);
nx_status_t status = rx->receive(rx, buffer, &len, 1000);
```

### Getting Statistics

```c
nx_diagnostic_t* diag = uart->get_diagnostic(uart);
nx_uart_stats_t stats;
nx_status_t status = diag->get_statistics(diag, &stats, sizeof(stats));
printf("TX: %u, RX: %u\n", stats.tx_count, stats.rx_count);
```

## Testing Support

The UART module provides several functions for testing:

### Inject RX Data
```c
/* Simulate received data */
const uint8_t data[] = "Test";
nx_uart_native_inject_rx(0, data, sizeof(data));
```

### Reset All Instances
```c
/* Reset all UART instances */
nx_uart_native_reset_all();
```

## Configuration

UART instances are configured via Kconfig. Example configuration:

```kconfig
CONFIG_INSTANCE_NX_UART_0=y
CONFIG_UART0_BAUDRATE=115200
CONFIG_UART0_DATA_BITS=8
CONFIG_UART0_STOP_BITS=1
CONFIG_UART0_PARITY_VALUE=0
CONFIG_UART0_TX_BUFFER_SIZE=256
CONFIG_UART0_RX_BUFFER_SIZE=256
```

## Implementation Details

### Circular Buffers
Each UART instance has separate TX and RX circular buffers. Buffer sizes are configured via Kconfig and allocated statically.

### Simulation
- TX operations write to stdout
- RX operations read from internal buffers (populated via inject function)
- Timeouts are ignored in simulation
- Power management is no-op

### Thread Safety
The Native platform implementation is not thread-safe. External synchronization is required for multi-threaded access.

## Backward Compatibility

The refactored implementation maintains backward compatibility:
- Legacy factory functions (`nx_uart_native_get`) continue to work
- All public interfaces remain unchanged
- Existing tests continue to pass

## Future Enhancements

Potential improvements:
- Add interrupt simulation
- Add DMA simulation
- Add error injection for testing
- Add flow control simulation
- Add multi-threaded support

## References

- [HAL UART Interface](../../../../hal/include/hal/interface/nx_uart.h)
- [Device Registration](../../../../hal/include/hal/base/nx_device.h)
- [Native Platform README](../../README.md)
