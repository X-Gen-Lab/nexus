# ADC Module - Native Platform

## Overview

This directory contains the ADC (Analog-to-Digital Converter) and ADC Buffer implementation for the Native platform. It provides simulated ADC functionality for testing and development on PC.

## Directory Structure

```
adc/
├── Kconfig                      # ADC configuration
├── README.md                    # This file
├── nx_adc_types.h               # Type definitions
├── nx_adc_helpers.h             # Helper functions
├── nx_adc_device.c              # ADC device registration
├── nx_adc_lifecycle.c           # ADC lifecycle implementation
├── nx_adc_power.c               # ADC power management
├── nx_adc_diagnostic.c          # ADC diagnostic interface
├── nx_adc_buffer_device.c       # ADC Buffer device registration
├── nx_adc_buffer_lifecycle.c    # ADC Buffer lifecycle implementation
└── nx_adc_buffer_power.c        # ADC Buffer power management
```

## Features

### ADC (Single-Shot Mode)
- Single-shot ADC conversion
- Multi-channel support (up to 16 channels)
- Configurable resolution (8-16 bits)
- Simulated conversion values
- Lifecycle management (init/deinit/suspend/resume)
- Power management
- Diagnostic interface with statistics

### ADC Buffer (Buffered Multi-Channel Sampling)
- High-performance buffered sampling
- DMA simulation
- Interleaved multi-channel samples
- Configurable buffer size
- Buffer-full callback support
- Lifecycle management
- Power management

## Configuration

ADC instances are configured via Kconfig. See `Kconfig` for available options.

Example configuration:
- ADC0: 16 channels, 12-bit resolution
- ADC_BUFFER0: 4 channels, 256 sample buffer

## Usage

### ADC (Single-Shot)

```c
/* Get ADC instance */
nx_adc_t* adc = nx_factory_adc(0);

/* Initialize */
nx_lifecycle_t* lc = adc->get_lifecycle(adc);
lc->init(lc);

/* Trigger conversion */
adc->trigger(adc);

/* Read channel value */
nx_adc_channel_t* ch = adc->get_channel(adc, 0);
uint32_t value = ch->get_value(ch);

/* Cleanup */
lc->deinit(lc);
```

### ADC Buffer (Buffered Sampling)

```c
/* Callback function */
void buffer_callback(const uint32_t* buffer, size_t size, void* user_data) {
    /* Process samples */
}

/* Get ADC buffer instance */
nx_adc_buffer_t* adc_buf = nx_factory_adc_buffer(0);

/* Initialize */
nx_lifecycle_t* lc = adc_buf->get_lifecycle(adc_buf);
lc->init(lc);

/* Register callback */
adc_buf->register_callback(adc_buf, buffer_callback, NULL);

/* Trigger sampling */
adc_buf->trigger(adc_buf);

/* Or access buffer directly */
uint32_t* buffer = adc_buf->get_buffer(adc_buf);
size_t size = adc_buf->get_buffer_size(adc_buf);

/* Cleanup */
lc->deinit(lc);
```

## Implementation Details

### Device Registration

Both ADC and ADC Buffer use the unified device registration mechanism:
- `NX_DEVICE_REGISTER` macro for registration
- `NX_TRAVERSE_EACH_INSTANCE` for Kconfig-based instantiation
- Platform configuration from Kconfig

### Simulation

- ADC values are randomly generated (0-4095 for 12-bit)
- ADC Buffer simulates DMA transfer with interleaved samples
- Channel values are updated on each trigger

### Testing Support

Testing functions are provided:
- `nx_adc_native_reset_all()` - Reset all ADC instances
- `nx_adc_native_set_simulated_value()` - Set channel value for testing
- `nx_adc_buffer_native_cleanup()` - Cleanup buffer instance

## Requirements

Validates requirements:
- 2.1: Modular organization
- 4.1-4.3: Unified device registration
- 3.3: Lifecycle management
- 3.4: Power management
- 3.5: Diagnostic interface (ADC only)

## Notes

- ADC Buffer allocates memory dynamically for sample buffer
- Buffer size is automatically aligned to multiple of channel count
- Both ADC and ADC Buffer support suspend/resume operations
- Diagnostic interface provides conversion and error statistics (ADC only)
