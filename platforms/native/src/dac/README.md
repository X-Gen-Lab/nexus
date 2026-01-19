# DAC Module - Native Platform

## Overview

This directory contains the DAC (Digital-to-Analog Converter) implementation for the Native platform. It provides simulated DAC functionality for testing and development on PC.

## Directory Structure

```
dac/
├── Kconfig                      # DAC configuration
├── README.md                    # This file
├── nx_dac_types.h               # Type definitions
├── nx_dac_helpers.h             # Helper functions
├── nx_dac_device.c              # DAC device registration
├── nx_dac_lifecycle.c           # DAC lifecycle implementation
└── nx_dac_power.c               # DAC power management
```

## Features

- Multi-channel DAC support (up to 4 channels per instance)
- Configurable resolution (8-16 bits)
- Configurable reference voltage
- Raw value and voltage (mV) output modes
- Synchronized output trigger
- Lifecycle management (init/deinit/suspend/resume)
- Power management

## Configuration

DAC instances are configured via Kconfig. See `Kconfig` for available options.

Example configuration:
- DAC0: 2 channels, 12-bit resolution, 3.3V reference

## Usage

```c
/* Get DAC instance */
nx_dac_t* dac = nx_factory_dac(0);

/* Initialize */
nx_lifecycle_t* lc = dac->get_lifecycle(dac);
lc->init(lc);

/* Get channel */
nx_dac_channel_t* ch = dac->get_channel(dac, 0);

/* Set raw value (0-4095 for 12-bit) */
ch->set_value(ch, 2048);

/* Or set voltage in millivolts */
ch->set_voltage_mv(ch, 1650);  /* 1.65V */

/* Trigger output update (for synchronized mode) */
dac->trigger(dac);

/* Cleanup */
lc->deinit(lc);
```

## Implementation Details

### Device Registration

Uses the unified device registration mechanism:
- `NX_DEVICE_REGISTER` macro for registration
- `NX_TRAVERSE_EACH_INSTANCE` for Kconfig-based instantiation
- Platform configuration from Kconfig

### Voltage Conversion

The `set_voltage_mv` function automatically converts millivolts to raw DAC values based on the configured reference voltage and resolution:

```
raw_value = (voltage_mv * max_value) / vref_mv
```

Where `max_value = (2^resolution) - 1`

### Testing Support

Testing functions are provided:
- `nx_dac_native_reset_all()` - Reset all DAC instances
- `nx_dac_native_get_channel_value()` - Get channel value for testing

## Requirements

Validates requirements:
- 2.1: Modular organization
- 4.1-4.3: Unified device registration
- 3.3: Lifecycle management
- 3.4: Power management

## Notes

- DAC values are stored internally but not actually output (simulation)
- Trigger function is a no-op in simulation
- Both raw value and voltage modes are supported
- Values are automatically clamped to valid range
