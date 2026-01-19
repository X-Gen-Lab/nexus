# Watchdog Peripheral Implementation

This directory contains the Native platform implementation of the Watchdog Timer peripheral.

## Overview

The Watchdog implementation provides a simulated watchdog timer for development and testing. It supports timeout configuration, feeding, early warning callbacks, and reset simulation.

## File Organization

```
watchdog/
├── Kconfig                     # Watchdog configuration options
├── nx_watchdog_device.c        # Device registration and initialization
├── nx_watchdog_lifecycle.c     # Lifecycle management
├── nx_watchdog_power.c         # Power management
├── nx_watchdog_interface.c     # Watchdog interface implementation
├── nx_watchdog_helpers.c       # Helper functions
├── nx_watchdog_helpers.h       # Helper function declarations
├── nx_watchdog_types.h         # Internal type definitions
└── README.md                   # This file
```

## Implementation Details

### Device Registration

The Watchdog device is registered using the `NX_DEVICE_REGISTER` macro based on Kconfig configuration. Each instance is configured with a default timeout value from Kconfig.

### Interfaces

- **Lifecycle Interface**: Initialize, deinitialize, suspend, resume, and query device state
- **Power Interface**: Enable, disable, and query power state
- **Watchdog Control**: Start, stop, and feed the watchdog timer
- **Timeout Query**: Get configured timeout value
- **Callback Management**: Set early warning callback for timeout events

### Watchdog Simulation

The Native platform simulates watchdog behavior:
- Tracks time since last feed using system time
- Triggers early warning callback when timeout occurs
- Logs timeout events instead of resetting system
- Provides timeout status for testing

The simulation uses system time (milliseconds) to track elapsed time since the last feed operation. When the elapsed time exceeds the configured timeout, the watchdog is considered to have timed out.

## Configuration

Watchdog configuration is managed through Kconfig:

```kconfig
CONFIG_WATCHDOG_NATIVE=y
CONFIG_WATCHDOG_MAX_INSTANCES=1
CONFIG_INSTANCE_NX_WATCHDOG0=y
CONFIG_WATCHDOG0_DEFAULT_TIMEOUT_MS=5000
```

## Usage Example

```c
#include "hal/interface/nx_watchdog.h"
#include "native_watchdog_test.h"

/* Get Watchdog instance */
nx_watchdog_t* wdt = nx_watchdog_native_get(0);

/* Initialize */
nx_lifecycle_t* lifecycle = wdt->get_lifecycle(wdt);
lifecycle->init(lifecycle);

/* Set early warning callback (optional) */
void watchdog_callback(void* user_data) {
    printf("Watchdog timeout warning!\n");
}
wdt->set_callback(wdt, watchdog_callback, NULL);

/* Start watchdog */
wdt->start(wdt);

/* Feed watchdog periodically */
while (running) {
    /* Do work */
    perform_task();
    
    /* Feed watchdog to prevent timeout */
    wdt->feed(wdt);
}

/* Stop watchdog */
wdt->stop(wdt);

/* Clean up */
lifecycle->deinit(lifecycle);
```

## Test Helper Functions

The Native platform provides test helper functions for testing watchdog behavior:

### Factory Functions

```c
/* Get Watchdog instance by index */
nx_watchdog_t* nx_watchdog_native_get(uint8_t index);
```

### Reset Functions

```c
/* Reset all Watchdog instances */
void nx_watchdog_native_reset_all(void);

/* Reset specific Watchdog instance */
nx_status_t nx_watchdog_native_reset(uint8_t index);
```

### State Query Functions

```c
/* Get Watchdog state */
nx_status_t nx_watchdog_native_get_state(uint8_t index, 
                                         bool* initialized,
                                         bool* suspended);

/* Check if watchdog has timed out */
bool nx_watchdog_native_has_timed_out(uint8_t index);
```

### Time Simulation Functions

```c
/* Simulate time passage (for testing) */
nx_status_t nx_watchdog_native_advance_time(uint8_t index, 
                                            uint32_t milliseconds);
```

## Error Handling

All functions return `nx_status_t` status codes for error reporting:

- `NX_OK`: Operation successful
- `NX_ERR_NULL_PTR`: NULL pointer parameter
- `NX_ERR_INVALID_PARAM`: Invalid parameter value
- `NX_ERR_NOT_INIT`: Device not initialized
- `NX_ERR_ALREADY_INIT`: Device already initialized
- `NX_ERR_BUSY`: Watchdog already running
- `NX_ERR_INVALID_STATE`: Invalid state for operation

## Testing Considerations

In the Native platform, watchdog timeout does not actually reset the system. Instead:
- Timeout events trigger the early warning callback if configured
- Timeout status can be queried using `nx_watchdog_native_has_timed_out()`
- System continues running for test verification
- Time can be simulated using `nx_watchdog_native_advance_time()` for deterministic testing

### Testing Example

```c
#include <gtest/gtest.h>
#include "native_watchdog_test.h"

TEST(WatchdogTest, TimeoutDetection) {
    /* Get and initialize watchdog */
    nx_watchdog_t* wdt = nx_watchdog_native_get(0);
    nx_lifecycle_t* lifecycle = wdt->get_lifecycle(wdt);
    lifecycle->init(lifecycle);
    
    /* Start watchdog */
    wdt->start(wdt);
    
    /* Get timeout value */
    uint32_t timeout_ms = wdt->get_timeout(wdt);
    
    /* Simulate time passage past timeout */
    nx_watchdog_native_advance_time(0, timeout_ms + 100);
    
    /* Verify timeout occurred */
    EXPECT_TRUE(nx_watchdog_native_has_timed_out(0));
    
    /* Clean up */
    lifecycle->deinit(lifecycle);
    nx_watchdog_native_reset_all();
}
```

## Simulation Behavior

The watchdog simulation provides realistic behavior for testing:

1. **Start**: Records the current system time as the last feed time
2. **Feed**: Updates the last feed time to the current system time
3. **Timeout Check**: Compares elapsed time since last feed to configured timeout
4. **Callback**: Invokes early warning callback when timeout is detected
5. **Stop**: Stops the watchdog timer (prevents timeout detection)

The simulation does not actually reset the system, allowing tests to verify timeout behavior without interrupting test execution.

## Related Documentation

- [HAL Watchdog Interface](../../../../hal/include/hal/interface/nx_watchdog.h)
- [Native Platform Test Helpers](../../include/native_watchdog_test.h)
- [Platform README](../../README.md)
- [Requirements Document](.kiro/specs/native-platform-improvements/requirements.md)
- [Design Document](.kiro/specs/native-platform-improvements/design.md)
