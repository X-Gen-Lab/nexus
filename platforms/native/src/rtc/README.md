# Native RTC Implementation

## Overview

The Native RTC (Real-Time Clock) implementation provides a simulated RTC peripheral for testing and development on PC. It supports Unix timestamp and calendar date/time operations, alarm configuration with callbacks, and full lifecycle/power management.

## Features

- **Time Management**: Set and get time using Unix timestamps or calendar date/time
- **Time Simulation**: Uses system time to simulate realistic RTC behavior
- **Alarm Support**: Configure alarms with callbacks that trigger at specified times
- **Date/Time Validation**: Validates date/time values (year 2000-2099, proper month/day ranges)
- **Lifecycle Management**: Full init/deinit/suspend/resume support
- **Power Management**: Enable/disable with callback notifications
- **Kconfig Integration**: Compile-time configuration via Kconfig

## Usage Examples

### Basic Time Operations

```c
#include "native_platform.h"

/* Get RTC instance */
nx_rtc_t* rtc = nx_rtc_native_get(0);

/* Initialize RTC */
nx_lifecycle_t* lifecycle = rtc->get_lifecycle(rtc);
lifecycle->init(lifecycle);

/* Set time using datetime */
nx_datetime_t dt = {
    .year = 2026,
    .month = 1,
    .day = 19,
    .hour = 14,
    .minute = 30,
    .second = 0
};
rtc->set_datetime(rtc, &dt);

/* Get current time */
nx_datetime_t current;
rtc->get_datetime(rtc, &current);
printf("Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
       current.year, current.month, current.day,
       current.hour, current.minute, current.second);

/* Get Unix timestamp */
uint32_t timestamp = rtc->get_timestamp(rtc);
printf("Unix timestamp: %u\n", timestamp);
```

### Alarm Configuration

```c
/* Alarm callback function */
void alarm_callback(void* user_data) {
    printf("Alarm triggered!\n");
}

/* Set alarm for 5 minutes from now */
nx_datetime_t alarm_time = {
    .year = 2026,
    .month = 1,
    .day = 19,
    .hour = 14,
    .minute = 35,
    .second = 0
};

rtc->set_alarm(rtc, &alarm_time, alarm_callback, NULL);

/* Disable alarm */
rtc->set_alarm(rtc, NULL, NULL, NULL);
```

### Lifecycle Management

```c
/* Initialize */
nx_lifecycle_t* lifecycle = rtc->get_lifecycle(rtc);
lifecycle->init(lifecycle);

/* Suspend (preserves time) */
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
nx_power_t* power = rtc->get_power(rtc);

/* Enable power */
power->enable(power);

/* Check if enabled */
bool enabled = power->is_enabled(power);

/* Disable power */
power->disable(power);

/* Set power state callback */
void power_callback(void* user_data, bool enabled) {
    printf("RTC power %s\n", enabled ? "enabled" : "disabled");
}
power->set_callback(power, power_callback, NULL);
```

## Test Helper Functions

The Native RTC implementation provides test helper functions for advanced testing scenarios:

### Factory Functions

```c
/* Get RTC instance by index */
nx_rtc_t* rtc = nx_rtc_native_get(0);
```

### Reset Functions

```c
/* Reset all RTC instances */
nx_rtc_native_reset_all();

/* Reset specific RTC instance */
nx_rtc_native_reset(0);
```

### State Query Functions

```c
/* Get RTC state */
bool initialized, suspended;
nx_rtc_native_get_state(0, &initialized, &suspended);
```

### Time Simulation Functions

```c
/* Simulate time passage (advance by 60 seconds) */
nx_rtc_native_advance_time(0, 60);

/* Manually trigger alarm check */
nx_rtc_native_check_alarm(0);
```

## Simulation Behavior

### Time Tracking

The Native RTC uses the system clock to simulate realistic time progression:

1. When time is set (via `set_datetime` or `set_timestamp`), the implementation records:
   - The set time value
   - The current system timestamp

2. When time is read (via `get_datetime` or `get_timestamp`), the implementation:
   - Calculates elapsed time since the last set operation
   - Adds the elapsed time to the set time value
   - Returns the calculated current time

This approach provides realistic time simulation without requiring periodic updates.

### Alarm Checking

Alarms are checked:
- Automatically when time is set or retrieved
- Manually via `nx_rtc_native_check_alarm()` for testing

When an alarm triggers:
- The registered callback is invoked
- The alarm is automatically disabled (one-shot behavior)
- Statistics are updated

### Date/Time Validation

The implementation validates all date/time values:
- **Year**: 2000-2099
- **Month**: 1-12
- **Day**: 1-31 (adjusted for month and leap years)
- **Hour**: 0-23
- **Minute**: 0-59
- **Second**: 0-59

Invalid values return `NX_ERR_INVALID_PARAM`.

### Leap Year Handling

The implementation correctly handles leap years:
- Years divisible by 4 are leap years
- Except years divisible by 100 (not leap years)
- Except years divisible by 400 (leap years)

February has 29 days in leap years, 28 days otherwise.

## Configuration

The RTC peripheral is configured via Kconfig:

```kconfig
config RTC_NATIVE
    bool "Enable RTC support for Native platform"
    default y

config INSTANCE_NX_RTC0
    bool "Enable RTC0"
    default y

config RTC0_ENABLE_ALARM
    bool "Enable alarm functionality for RTC0"
    default y

config RTC0_ALARM_COUNT
    int "Maximum number of alarms for RTC0"
    default 1
    range 1 8
```

## Implementation Details

### File Structure

```
platforms/native/src/rtc/
├── nx_rtc_device.c       # Device registration and factory functions
├── nx_rtc_interface.c    # RTC interface implementation
├── nx_rtc_lifecycle.c    # Lifecycle management
├── nx_rtc_power.c        # Power management
├── nx_rtc_helpers.c      # Helper functions (validation, conversion)
├── nx_rtc_helpers.h      # Helper function declarations
├── nx_rtc_types.h        # Type definitions
├── Kconfig               # Configuration options
└── README.md             # This file
```

### State Management

Each RTC instance maintains:
- Current date/time
- Start timestamp for simulation
- Alarm configuration (time, callback, user data)
- Statistics (set/get counts, alarm triggers)
- Initialization and suspend flags

### Thread Safety

The Native RTC implementation is **not thread-safe**. If used in a multi-threaded environment, external synchronization is required.

## Limitations

1. **Single Alarm**: Currently supports only one alarm per RTC instance
2. **One-Shot Alarms**: Alarms automatically disable after triggering
3. **No Periodic Alarms**: Recurring alarms must be manually re-configured
4. **No Subsecond Precision**: Time resolution is limited to seconds
5. **Year Range**: Limited to 2000-2099 for validation
6. **No Time Zones**: All times are treated as local time

## Testing

The RTC implementation includes comprehensive tests:

### Unit Tests
- Time set/get operations
- Date/time validation
- Alarm configuration and triggering
- Lifecycle state transitions
- Power management
- Error conditions

### Property-Based Tests
- **Property 8**: RTC Time Validation - validates that invalid date/time values are rejected
- **Property 9**: RTC Alarm Trigger - validates that alarms trigger at the correct time

See `tests/hal/native/test_nx_rtc.cpp` for complete test implementation.

## Error Codes

Common error codes returned by RTC functions:

- `NX_OK`: Operation successful
- `NX_ERR_NULL_PTR`: NULL pointer parameter
- `NX_ERR_INVALID_PARAM`: Invalid parameter value (e.g., invalid date/time)
- `NX_ERR_NOT_INIT`: RTC not initialized
- `NX_ERR_ALREADY_INIT`: RTC already initialized
- `NX_ERR_INVALID_STATE`: Invalid state for operation
- `NX_ERR_NOT_SUPPORTED`: Feature not supported (e.g., alarm disabled in Kconfig)

## See Also

- HAL RTC Interface: `hal/interface/nx_rtc.h`
- Native Platform Header: `platforms/native/include/native_platform.h`
- RTC Test Helpers: `platforms/native/include/native_rtc_test.h`
