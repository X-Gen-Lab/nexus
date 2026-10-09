# Timer Module - Native Platform

## Overview

This directory contains the Timer peripheral implementation for the Native platform. The implementation provides simulated Timer functionality for testing and development on PC.

## Directory Structure

```
timer/
├── Kconfig                  # Timer configuration
├── nx_timer_device.c        # Device registration
├── nx_timer_lifecycle.c     # Lifecycle implementation
├── nx_timer_power.c         # Power management implementation
├── nx_timer_helpers.c       # Helper functions implementation
├── nx_timer_helpers.h       # Helper functions declarations
├── nx_timer_types.h         # Type definitions
└── README.md                # This file
```

## Features

- **Device Registration**: Uses NX_DEVICE_REGISTER macro for unified registration
- **Lifecycle Management**: Implements init/deinit/suspend/resume operations
- **Power Management**: Simulates power control operations
- **Timer Control**: Start/stop timer, set period, get counter value
- **Callback Support**: Timer callback for periodic events
- **Kconfig Integration**: Compile-time configuration for instances and parameters

## Configuration

Timer instances are configured via Kconfig:

```kconfig
config INSTANCE_NX_TIMER_0
    bool "Enable TIMER0"
    default y

config TIMER0_FREQUENCY
    int "TIMER0 frequency (Hz)"
    default 1000000

config TIMER0_CHANNEL_COUNT
    int "TIMER0 PWM channel count"
    default 4
```

## Usage Example

```c
#include "hal/interface/nx_timer.h"

/* Get timer instance */
nx_timer_base_t* timer = nx_factory_timer(0);

/* Initialize */
nx_lifecycle_t* lc = timer->get_lifecycle(timer);
lc->init(lc);

/* Configure period */
timer->set_period(timer, 1000, 1000);

/* Set callback */
timer->set_callback(timer, my_callback, user_data);

/* Start timer */
timer->start(timer);

/* Stop timer */
timer->stop(timer);

/* Cleanup */
lc->deinit(lc);
```

## Testing Support

The module provides testing functions:

- `nx_timer_native_reset_all()`: Reset all timer instances
- `nx_timer_native_trigger_callback()`: Manually trigger timer callback
- `nx_timer_native_increment_counter()`: Increment timer counter

## Implementation Details

### State Management

Each timer instance maintains:
- Configuration (frequency, prescaler, period, channel count)
- Runtime state (counter, running flag, initialized flag)
- Callback function and user data

### Simulation

The Native platform simulates timer behavior:
- Counter increments are simulated via test functions
- Callbacks are triggered manually for testing
- Period and prescaler are stored but not actively used

## Requirements Mapping

This implementation satisfies:
- **Requirement 2.1**: Modular organization with independent timer directory
- **Requirement 4.1**: Uses NX_DEVICE_REGISTER for device registration
- **Requirement 4.2**: Uses NX_TRAVERSE_EACH_INSTANCE for instance traversal
- **Requirement 4.3**: Provides unified initialization function
