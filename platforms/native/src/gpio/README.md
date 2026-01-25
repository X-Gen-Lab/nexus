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

### Kconfig Configuration Symbols

The GPIO registration system uses a standardized naming convention for Kconfig symbols:

- **Instance Enable**: `INSTANCE_NX_GPIO<PORT>_PIN<PIN>` (e.g., `INSTANCE_NX_GPIOA_PIN0`)
- **Mode Configuration**: `CONFIG_GPIO<PORT>_PIN<PIN>_MODE` (e.g., `CONFIG_GPIOA_PIN0_MODE`)
- **Pull Configuration**: `CONFIG_GPIO<PORT>_PIN<PIN>_PULL_VALUE` (e.g., `CONFIG_GPIOA_PIN0_PULL_VALUE`)
- **Speed Configuration**: `CONFIG_GPIO<PORT>_PIN<PIN>_SPEED_VALUE` (e.g., `CONFIG_GPIOA_PIN0_SPEED_VALUE`)

### Device Naming Convention

GPIO devices are registered with a standardized naming format:
- **Format**: `"GPIO<PORT><PIN>"` (e.g., `"GPIOA0"`, `"GPIOB12"`)
- **Port Letter**: Calculated as `'A' + port_number` (0→A, 1→B, 2→C, etc.)
- **Device ID**: Calculated as `(port * 16 + pin)` for unique identification

### Example Configuration

```kconfig
# Enable GPIOA PIN0
CONFIG_INSTANCE_NX_GPIOA_PIN0=y
CONFIG_GPIOA_PIN0_MODE=1          # Output push-pull
CONFIG_GPIOA_PIN0_PULL_VALUE=0    # No pull
CONFIG_GPIOA_PIN0_SPEED_VALUE=1   # Medium speed

# Enable GPIOB PIN5
CONFIG_INSTANCE_NX_GPIOB_PIN5=y
CONFIG_GPIOB_PIN5_MODE=0          # Input
CONFIG_GPIOB_PIN5_PULL_VALUE=1    # Pull-up
CONFIG_GPIOB_PIN5_SPEED_VALUE=0   # Low speed
```

## Usage

### Getting GPIO Instance

There are two ways to get a GPIO instance:

#### 1. Using Factory Functions (Recommended)

```c
#include "hal/nx_factory.h"

/* Get GPIO read-write instance using factory function */
nx_gpio_read_write_t* gpio = nx_factory_gpio_read_write(0, 5);  /* GPIOA PIN5 */
if (gpio == NULL) {
    /* Device not registered or initialization failed */
}

/* Get read-only interface */
nx_gpio_read_t* gpio_read = nx_factory_gpio_read(0, 5);

/* Get write-only interface */
nx_gpio_write_t* gpio_write = nx_factory_gpio_write(0, 5);
```

#### 2. Using Native Platform Functions

```c
#include "nx_gpio_native.h"

/* Get GPIO read-write instance */
nx_gpio_read_write_t* gpio = nx_gpio_native_get_read_write(0, 0);  /* GPIOA PIN0 */

/* Get read-only interface */
nx_gpio_read_t* gpio_read = nx_gpio_native_get_read(0, 0);

/* Get write-only interface */
nx_gpio_write_t* gpio_write = nx_gpio_native_get_write(0, 0);
```

### Port and Pin Numbering

- **Port Numbers**: 0-7 (corresponding to ports A-H)
  - Port A = 0, Port B = 1, Port C = 2, etc.
- **Pin Numbers**: 0-15 (each port has 16 pins)

### Port Conversion Helpers

Helper macros are available for converting between port numbers and characters:

```c
#include "nx_gpio_helpers.h"

/* Convert port character to number */
uint8_t port_num = NX_GPIO_PORT_NUM('A');  /* Returns 0 */

/* Convert port number to character */
char port_char = NX_GPIO_PORT_CHAR(1);     /* Returns 'B' */
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

GPIO devices are registered using a Kconfig-driven registration system:

1. **Kconfig Configuration**: Users enable GPIO instances via Kconfig
2. **Macro Generation**: The Kconfig system generates `NX_DEFINE_INSTANCE_NX_GPIO(fn)` macro in `nexus_config.h`
3. **Device Registration**: The `NX_TRAVERSE_EACH_INSTANCE` macro expands to call `NX_GPIO_DEVICE_REGISTER` for each enabled instance
4. **Automatic Initialization**: Devices are initialized on first access via factory functions

#### Registration Macros

```c
/* Configuration macro - reads from Kconfig */
#define NX_GPIO_CONFIG(_P, _N)
    /* Creates static platform configuration structure */
    /* Reads CONFIG_GPIO<PORT>_PIN<PIN>_* symbols */

/* Device registration macro */
#define NX_GPIO_DEVICE_REGISTER(_P, _N)
    /* Generates configuration, state, and registration code */
    /* Device name: "GPIO<PORT><PIN>" */
    /* Device ID: (port_num * 16 + pin) */

/* Register all enabled instances */
NX_TRAVERSE_EACH_INSTANCE(NX_GPIO_DEVICE_REGISTER, DEVICE_TYPE);
```

#### Device Lookup

Factory functions construct device names using the same format as registration:

```c
static inline nx_gpio_t* nx_factory_gpio(char port, uint8_t pin) {
    char name[16];
    snprintf(name, sizeof(name), "GPIO%c%d", port, pin);
    return (nx_gpio_t*)nx_device_get(name);
}
```

This ensures consistency between device registration and lookup.

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

### Configuration Validation

Helper functions are provided to validate GPIO configurations:

```c
#include "nx_gpio_helpers.h"

/* Validate port number (0-7) */
bool valid = nx_gpio_validate_port(port);

/* Validate pin number (0-15) */
bool valid = nx_gpio_validate_pin(pin);

/* Validate both port and pin */
bool valid = nx_gpio_validate_config(port, pin);
```

## Notes

- The Native platform simulates GPIO behavior for testing purposes
- Pin states are stored in memory and can be read/written
- External interrupts are triggered manually via testing functions
- Power management is simulated (always enabled)
- All GPIO operations are thread-safe in single-threaded environments

## Migration from Old Implementation

If you're migrating from an older GPIO implementation:

1. **Update Kconfig symbols**: Ensure configuration symbols follow the new naming convention
   - Old: `CONFIG_GPIO_A_PIN_0_MODE`
   - New: `CONFIG_GPIOA_PIN0_MODE`

2. **Update device access**: Use factory functions with port character
   - Old: `nx_device_get("gpio_a_0")`
   - New: `nx_factory_gpio('A', 0)` or `nx_factory_gpio_read_write('A', 0)`

3. **Port parameter**: Use character port values ('A'-'H') instead of numbers
   - Port A = 'A', Port B = 'B', etc.
   - Port A = 0, Port B = 1, etc.

4. **Device names**: Devices are now named `"GPIOA0"`, `"GPIOB5"`, etc.
   - Format: `"GPIO<PORT><PIN>"`

5. **Regenerate configuration**: Run Kconfig generation script to update `nexus_config.h`
   ```bash
   python scripts/kconfig/generate_config.py
   ```
