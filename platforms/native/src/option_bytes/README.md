# Native Option Bytes Implementation

## Overview

The Native Option Bytes implementation provides a simulated Option Bytes peripheral for testing and development on PC. It supports reading/writing user data, managing read protection levels, and applying pending changes with write protection support.

## Features

- **User Data Storage**: Read and write up to 16 bytes of user-configurable data
- **Read Protection**: Three levels of read protection (0=none, 1=level1, 2=level2)
- **Write Protection**: Simulate write protection to prevent unauthorized modifications
- **Pending Changes**: Changes are staged until explicitly applied
- **Lifecycle Management**: Full init/deinit/suspend/resume support
- **Power Management**: Enable/disable with callback notifications
- **Kconfig Integration**: Compile-time configuration via Kconfig

## Usage Examples

### Basic User Data Operations

```c
#include "native_platform.h"

/* Get Option Bytes instance */
nx_option_bytes_t* opt_bytes = nx_option_bytes_native_get(0);

/* Initialize Option Bytes */
nx_lifecycle_t* lifecycle = (nx_lifecycle_t*)((uint8_t*)opt_bytes + 
                                              sizeof(nx_option_bytes_t));
lifecycle->init(lifecycle);

/* Write user data */
uint8_t user_data[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                         0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
opt_bytes->set_user_data(opt_bytes, user_data, 16);

/* Apply changes (required for changes to take effect) */
opt_bytes->apply(opt_bytes);

/* Read user data */
uint8_t read_data[16];
opt_bytes->get_user_data(opt_bytes, read_data, 16);
```

### Read Protection Management

```c
/* Get current protection level */
uint8_t level = opt_bytes->get_read_protection(opt_bytes);
printf("Current protection level: %d\n", level);

/* Set protection level to 1 */
opt_bytes->set_read_protection(opt_bytes, 1);

/* Apply changes */
opt_bytes->apply(opt_bytes);

/* Verify new protection level */
level = opt_bytes->get_read_protection(opt_bytes);
printf("New protection level: %d\n", level);
```

### Lifecycle Management

```c
/* Initialize */
nx_lifecycle_t* lifecycle = (nx_lifecycle_t*)((uint8_t*)opt_bytes + 
                                              sizeof(nx_option_bytes_t));
lifecycle->init(lifecycle);

/* Suspend (preserves option bytes state) */
lifecycle->suspend(lifecycle);

/* Resume */
lifecycle->resume(lifecycle);

/* Check state */
nx_device_state_t state = lifecycle->get_state(lifecycle);

/* Deinitialize */
lifecycle->deinit(lifecycle);
```

### Power Management

```c
/* Get power interface */
nx_power_t* power = (nx_power_t*)((uint8_t*)opt_bytes + 
                                  sizeof(nx_option_bytes_t) + 
                                  sizeof(nx_lifecycle_t));

/* Enable power */
power->enable(power);

/* Check if enabled */
bool enabled = power->is_enabled(power);

/* Disable power */
power->disable(power);

/* Set power state callback */
void power_callback(void* user_data, bool enabled) {
    printf("Option Bytes power %s\n", enabled ? "enabled" : "disabled");
}
power->set_callback(power, power_callback, NULL);
```

## Test Helper Functions

The Native Option Bytes implementation provides test helper functions for advanced testing scenarios:

### Factory Functions

```c
/* Get Option Bytes instance by index */
nx_option_bytes_t* opt_bytes = nx_option_bytes_native_get(0);
```

### Reset Functions

```c
/* Reset all Option Bytes instances */
nx_option_bytes_native_reset_all();

/* Reset specific Option Bytes instance */
nx_option_bytes_native_reset(0);
```

### State Query Functions

```c
/* Get Option Bytes state */
bool initialized, suspended;
nx_option_bytes_native_get_state(0, &initialized, &suspended);
```

### Write Protection Control

```c
/* Enable write protection (for testing) */
nx_option_bytes_native_set_write_protection(0, true);

/* Check write protection status */
bool protected;
nx_option_bytes_native_get_write_protection(0, &protected);

/* Disable write protection */
nx_option_bytes_native_set_write_protection(0, false);
```

### Pending Changes Query

```c
/* Check if there are pending changes */
bool has_pending;
nx_option_bytes_native_has_pending_changes(0, &has_pending);
```

## Simulation Behavior

### Pending Changes Model

The Native Option Bytes uses a two-stage commit model:

1. **Write Stage**: When `set_user_data()` or `set_read_protection()` is called:
   - Changes are written to a pending buffer
   - The pending changes flag is set
   - Current values remain unchanged

2. **Apply Stage**: When `apply()` is called:
   - Pending changes are copied to the active buffer
   - The pending changes flag is cleared
   - New values become visible to read operations

This simulates the behavior of real hardware where option bytes changes require an explicit commit operation.

### Write Protection

Write protection prevents modifications to option bytes:
- When enabled, `set_user_data()`, `set_read_protection()`, and `apply()` return `NX_ERR_PERMISSION`
- Read operations are not affected
- Write protection can be controlled via test helpers for testing scenarios

### Read Protection Levels

The implementation supports three read protection levels:
- **Level 0**: No protection (default)
- **Level 1**: Basic read protection
- **Level 2**: Enhanced read protection

Note: In the simulation, read protection levels are stored but do not affect read operations. They are primarily for testing protection level management.

### User Data Storage

User data is stored in a 16-byte array:
- Default value: 0xFF (erased state)
- Can be written in full (16 bytes) or partially (1-16 bytes)
- Changes are not visible until `apply()` is called

## Configuration

The Option Bytes peripheral is configured via Kconfig:

```kconfig
config NATIVE_OPTION_BYTES
    bool "Enable Option Bytes support"
    default y

config NATIVE_OPTION_BYTES_INSTANCES
    int "Number of Option Bytes instances"
    default 1
    range 1 4

config NATIVE_OPTION_BYTES_USER_DATA_SIZE
    int "User data size in bytes"
    default 16

config NATIVE_OPTION_BYTES_DEFAULT_PROTECTION
    int "Default read protection level"
    default 0
    range 0 2
```

## Implementation Details

### File Structure

```
platforms/native/src/option_bytes/
├── nx_option_bytes_device.c      # Device registration and factory functions
├── nx_option_bytes_interface.c   # Option Bytes interface implementation
├── nx_option_bytes_lifecycle.c   # Lifecycle management
├── nx_option_bytes_power.c       # Power management
├── nx_option_bytes_helpers.c     # Helper functions (validation, operations)
├── nx_option_bytes_helpers.h     # Helper function declarations
├── nx_option_bytes_types.h       # Type definitions
├── Kconfig                       # Configuration options
└── README.md                     # This file
```

### State Management

Each Option Bytes instance maintains:
- Active option bytes data (user data, read protection level, write protection flag)
- Pending option bytes data (staged changes)
- Pending changes flag
- Initialization and suspend flags

### Thread Safety

The Native Option Bytes implementation is **not thread-safe**. If used in a multi-threaded environment, external synchronization is required.

## Limitations

1. **User Data Size**: Fixed at 16 bytes (configurable via Kconfig)
2. **Read Protection**: Levels are stored but do not affect read operations in simulation
3. **Write Protection**: Controlled via test helpers, not through standard interface
4. **No Persistence**: Option bytes are not persisted across program runs
5. **Single Pending Buffer**: Only one set of pending changes can be staged at a time

## Testing

The Option Bytes implementation includes comprehensive tests:

### Unit Tests
- User data read/write operations
- Read protection level management
- Write protection enforcement
- Pending changes behavior
- Lifecycle state transitions
- Power management
- Error conditions

### Property-Based Tests
- **Property 14**: Option Bytes Write Protection - validates that write protection prevents modifications

See `tests/hal/test_nx_option_bytes.cpp` for complete test implementation.

## Error Codes

Common error codes returned by Option Bytes functions:

- `NX_OK`: Operation successful
- `NX_ERR_NULL_PTR`: NULL pointer parameter
- `NX_ERR_INVALID_PARAM`: Invalid parameter value (e.g., invalid protection level, invalid length)
- `NX_ERR_NOT_INIT`: Option Bytes not initialized
- `NX_ERR_ALREADY_INIT`: Option Bytes already initialized
- `NX_ERR_INVALID_STATE`: Invalid state for operation
- `NX_ERR_PERMISSION`: Write protection prevents modification

## See Also

- HAL Option Bytes Interface: `hal/interface/nx_option_bytes.h`
- Native Platform Header: `platforms/native/include/native_platform.h`
- Option Bytes Test Helpers: `platforms/native/include/native_option_bytes_test.h`
