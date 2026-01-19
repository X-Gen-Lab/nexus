# GPIO Implementation for Native Platform

## Overview

This directory contains the GPIO (General Purpose Input/Output) implementation for the Native platform. The implementation provides simulated GPIO functionality for testing and development on PC.

## Directory Structure

```
gpio/
├── Kconfig                    # GPIO configuration options
├── README.md                  # This file
├── nx_gpio_types.h            # Type definitions
├── nx_gpio_helpers.h          # Helper function declarations
├── nx_gpio_helpers.c          # Helper function implementations
├── nx_gpio_device.c           # Device registration
├── nx_gpio_read.c             # Read interface implementation
├── nx_gpio_write.c            # Write interface implementation
├── nx_gpio_read_write.c       # Read-write interface implementation
├── nx_gpio_lifecycle.c        # Lifecycle interface implementation
└── nx_gpio_power.c            # Power management interface implementation
```

## Features

- **Multiple GPIO Ports**: Supports up to 8 GPIO ports (A-H)
- **Multiple Pins per Port**: Supports up to 16 pins per port
- **Read Interface**: Input-only GPIO pins with external interrupt support
- **Write Interface**: Output-only GPIO pins with toggle functionality
- **Read-Write Interface**: Bidirectional GPIO pins
- **Lifecycle Management**: Initialize, deinitialize, suspend, resume
- **Power Management**: Simulated power control
- **External Interrupts**: Simulated interrupt callbacks with trigger types
- **Statistics**: Read, write, toggle, and interrupt counters

## Configuration

GPIO instances are configured via Kconfig. Each GPIO pin can be individually enabled and configured.

### Example Configuration

```kconfig
# Enable GPIOA
CONFIG_INSTANCE_NX_GPIOA=y

# Enable GPIOA PIN0
CONFIG_INSTANCE_NX_GPIOA_PIN0=y
CONFIG_GPIOA_PIN0_MODE=1          # Output push-pull
CONFIG_GPIOA_PIN0_PULL_VALUE=0    # No pull
CONFIG_GPIOA_PIN0_SPEED_VALUE=1   # Medium speed
CONFIG_GPIOA_PIN0_OUTPUT_VALUE=0  # Initial low
```

## Usage

### Getting GPIO Instance

```c
/* Get GPIO read-write instance */
nx_gpio_read_write_t* gpio = nx_gpio_native_get(0, 0);  /* GPIOA PIN0 */

/* Get read-only interface */
nx_gpio_read_t* gpio_read = &gpio->read;

/* Get write-only interface */
nx_gpio_write_t* gpio_write = &gpio->write;
```

### Initialization

```c
/* Get lifecycle interface */
nx_lifecycle_t* lc = gpio->read.get_lifecycle(&gpio->read);

/* Initialize GPIO */
nx_status_t status = lc->init(lc);
if (status != NX_OK) {
    /* Handle error */
}
```

### Reading GPIO

```c
/* Read pin state */
uint8_t state = gpio->read.read(&gpio->read);
```

### Writing GPIO

```c
/* Write pin state */
gpio->write.write(&gpio->write, 1);  /* Set high */

/* Toggle pin state */
gpio->write.toggle(&gpio->write);
```

### External Interrupts

```c
/* Interrupt callback */
void gpio_callback(void* user_data) {
    /* Handle interrupt */
}

/* Register interrupt */
nx_status_t status = gpio->read.register_exti(&gpio->read,
                                              gpio_callback,
                                              NULL,
                                              NX_GPIO_TRIGGER_RISING);
```

## Testing Support

The Native platform provides testing functions for GPIO:

### Trigger External Interrupt

```c
/* Trigger interrupt by changing pin state */
nx_gpio_native_trigger_exti(0, 0, 1);  /* GPIOA PIN0, high */
```

### Get Pin State

```c
/* Get current pin state */
uint8_t state = nx_gpio_native_get_pin_state(0, 0);  /* GPIOA PIN0 */
```

### Reset All Instances

```c
/* Reset all GPIO instances */
nx_gpio_native_reset_all();
```

## Implementation Details

### Device Registration

GPIO devices are registered using the `NX_DEVICE_REGISTER` macro and the `NX_DEFINE_INSTANCE_NX_GPIO` traversal macro from the Kconfig system.

### Interface Separation

The implementation separates different interfaces into separate files:
- `nx_gpio_read.c`: Read interface (input only)
- `nx_gpio_write.c`: Write interface (output only)
- `nx_gpio_read_write.c`: Combined read-write interface (bidirectional)
- `nx_gpio_lifecycle.c`: Lifecycle management
- `nx_gpio_power.c`: Power management

### State Management

Each GPIO pin has its own state structure containing:
- Configuration (mode, pull, speed, etc.)
- Statistics (read, write, toggle, interrupt counts)
- External interrupt context (callback, trigger type)
- Current pin state
- Initialization and suspend flags

## Notes

- The Native platform simulates GPIO behavior for testing purposes
- Pin states are stored in memory and can be read/written
- External interrupts are triggered manually via testing functions
- Power management is simulated (always enabled)
- All GPIO operations are thread-safe in single-threaded environments
