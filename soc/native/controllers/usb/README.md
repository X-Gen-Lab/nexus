# USB Peripheral Implementation

This directory contains the Native platform implementation of the USB (Universal Serial Bus) peripheral.

## Overview

The USB implementation provides a simulated USB device controller for development and testing. It supports CDC (Communications Device Class) style communication with async/sync TX/RX interfaces, connection management, and event simulation.

## File Organization

```
usb/
├── Kconfig                  # USB configuration options
├── nx_usb_device.c          # Device registration and initialization
├── nx_usb_lifecycle.c       # Lifecycle management
├── nx_usb_power.c           # Power management
├── nx_usb_interface.c       # Communication interfaces (TX/RX)
├── nx_usb_helpers.c         # Helper functions
├── nx_usb_helpers.h         # Helper declarations
├── nx_usb_types.h           # Type definitions
└── README.md                # This file
```

## Implementation Details

### Device Registration

The USB device is registered using the `NX_DEVICE_REGISTER` macro based on Kconfig configuration. Each USB instance is configured at compile-time with buffer sizes and endpoint counts.

### Interfaces

- **Lifecycle Interface**: Initialize, deinitialize, suspend, resume, and query device state
- **Power Interface**: Enable, disable, and query power state with callback support
- **TX Async Interface**: Non-blocking data transmission
- **RX Async Interface**: Non-blocking data reception
- **TX Sync Interface**: Blocking data transmission with timeout
- **RX Sync Interface**: Blocking data reception with timeout
- **Connection Status**: Query USB connection state

### USB Simulation

The Native platform simulates USB device behavior:
- Connection/disconnection events
- Suspend/resume events
- Endpoint configuration and management
- Data buffering for TX/RX operations
- USB device states (connected, suspended)

### Endpoint Management

The implementation supports up to 8 configurable endpoints:
- Control endpoints (EP0)
- Bulk endpoints (for data transfer)
- Interrupt endpoints (for status/events)
- Isochronous endpoints (for streaming)

Each endpoint has:
- Configurable type and direction
- Maximum packet size configuration
- Internal data buffer (512 bytes)

## Configuration

USB configuration is managed through Kconfig:

```kconfig
CONFIG_NX_USB_ENABLED=y
CONFIG_NX_USB_NUM_ENDPOINTS=4
CONFIG_NX_USB_TX_BUFFER_SIZE=1024
CONFIG_NX_USB_RX_BUFFER_SIZE=1024
CONFIG_NX_USB_AUTO_CONNECT=y
```

## Usage Example

```c
#include "hal/interface/nx_usb.h"

/* Get USB instance */
nx_usb_t* usb = nx_usb_native_get(0);

/* Initialize */
nx_lifecycle_t* lc = usb->get_lifecycle(usb);
lc->init(lc);

/* Check connection status */
if (usb->is_connected(usb)) {
    /* Send data (async) */
    nx_tx_async_t* tx = usb->get_tx_async(usb);
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    tx->send(tx, data, sizeof(data));

    /* Receive data (async) */
    nx_rx_async_t* rx = usb->get_rx_async(usb);
    uint8_t buffer[64];
    size_t len = sizeof(buffer);
    nx_status_t status = rx->receive(rx, buffer, &len);
    if (status == NX_OK) {
        /* Process received data */
    }
}

/* Clean up */
lc->deinit(lc);
```

## Test Helper Functions

The USB implementation provides test helper functions for simulation and testing:

### Factory Functions

```c
/* Get USB instance */
nx_usb_t* nx_usb_native_get(uint8_t index);

/* Reset all USB instances */
void nx_usb_native_reset_all(void);
```

### State Query

```c
/* Get USB state */
nx_status_t nx_usb_native_get_state(uint8_t index, bool* initialized,
                                    bool* suspended);
```

### Data Injection

```c
/* Inject data into RX buffer (simulates host sending data) */
nx_status_t nx_usb_native_inject_rx(uint8_t index, const uint8_t* data,
                                    size_t len);
```

### Event Simulation

```c
/* Simulate USB connect event */
nx_status_t nx_usb_native_simulate_connect(uint8_t index);

/* Simulate USB disconnect event */
nx_status_t nx_usb_native_simulate_disconnect(uint8_t index);

/* Simulate USB suspend event */
nx_status_t nx_usb_native_simulate_suspend(uint8_t index);

/* Simulate USB resume event */
nx_status_t nx_usb_native_simulate_resume(uint8_t index);
```

## Simulation Behavior

### Connection Management

- USB starts in disconnected state (unless `NX_CONFIG_USB_AUTO_CONNECT=y`)
- Use `nx_usb_native_simulate_connect()` to simulate host connection
- Use `nx_usb_native_simulate_disconnect()` to simulate host disconnection
- Connection state affects TX/RX operations (fail when disconnected)

### Data Transfer

- **TX Operations**: Data is written to internal TX buffer
- **RX Operations**: Data is read from internal RX buffer
- Use `nx_usb_native_inject_rx()` to simulate host sending data
- Buffers are circular and have configurable sizes

### Power Management

- Power state is independent of connection state
- Power callbacks are invoked on enable/disable
- Power state does not affect data transfer (simulation only)

### Lifecycle States

- **UNINITIALIZED**: Device not initialized
- **RUNNING**: Device initialized and operational
- **SUSPENDED**: Device suspended (preserves state)
- **ERROR**: Invalid state or error condition

## Error Handling

All functions return `nx_status_t` status codes:

- `NX_OK`: Operation successful
- `NX_ERR_NULL_PTR`: NULL pointer parameter
- `NX_ERR_INVALID_PARAM`: Invalid parameter value
- `NX_ERR_NOT_INIT`: Device not initialized
- `NX_ERR_ALREADY_INIT`: Device already initialized
- `NX_ERR_INVALID_STATE`: Invalid state for operation
- `NX_ERR_BUSY`: Device busy
- `NX_ERR_FULL`: Buffer full
- `NX_ERR_NO_DATA`: No data available
- `NX_ERR_TIMEOUT`: Operation timeout

## Testing

The USB implementation includes comprehensive tests:

- **Unit Tests**: Test all interface functions, lifecycle, power management
- **Property Tests**: Test universal properties (endpoint configuration, round-trip)
- **Integration Tests**: Test USB with other peripherals

See `tests/hal/native/test_nx_usb.cpp` for test examples.

## Related Documentation

- [HAL USB Interface](../../../../hal/include/hal/interface/nx_usb.h)
- [Platform README](../../README.md)
- [Native Platform Test Helpers](../../include/native_usb_test.h)

