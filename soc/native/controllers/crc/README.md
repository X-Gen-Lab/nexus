# Native Platform CRC Implementation

This directory contains the CRC (Cyclic Redundancy Check) peripheral implementation for the Native platform.

## Overview

The CRC peripheral provides hardware-accelerated CRC calculation for data integrity verification. The Native platform implementation simulates CRC hardware using software algorithms.

## Features

- **CRC-32 (IEEE 802.3)**: Standard CRC-32 polynomial (0x04C11DB7)
- **CRC-16 (CCITT)**: CCITT CRC-16 polynomial (0x1021)
- **Configurable Parameters**: Initial value and final XOR value
- **Incremental Calculation**: Support for streaming data
- **Lifecycle Management**: Init, deinit, suspend, resume operations
- **Power Management**: Enable, disable, power state callbacks

## Files

- `nx_crc_device.c` - Device registration and factory functions
- `nx_crc_interface.c` - CRC interface implementation
- `nx_crc_lifecycle.c` - Lifecycle management
- `nx_crc_power.c` - Power management
- `nx_crc_helpers.c` - CRC algorithm implementations
- `nx_crc_helpers.h` - Helper function declarations
- `nx_crc_types.h` - Internal type definitions
- `Kconfig` - Configuration options

## Usage Example

```c
#include "hal/interface/nx_crc.h"
#include "native_crc_test.h"

/* Get CRC instance */
nx_crc_t* crc = nx_crc_native_get(0);

/* Initialize */
nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
lifecycle->init(lifecycle);

/* Calculate CRC in one shot */
const uint8_t data[] = "123456789";
uint32_t result = crc->calculate(crc, data, 9);
/* Result: 0xCBF43926 for CRC-32 */

/* Calculate CRC incrementally */
crc->reset(crc);
crc->update(crc, data, 3);      /* "123" */
crc->update(crc, data + 3, 3);  /* "456" */
crc->update(crc, data + 6, 3);  /* "789" */
uint32_t result2 = crc->get_result(crc);
/* result2 == result */

/* Cleanup */
lifecycle->deinit(lifecycle);
```

## Test Helper Functions

The Native platform provides test helper functions for CRC testing:

### Factory Functions

```c
/* Get CRC instance by index (0-3) */
nx_crc_t* nx_crc_native_get(uint8_t index);
```

### Reset Functions

```c
/* Reset all CRC instances */
void nx_crc_native_reset_all(void);

/* Reset specific CRC instance */
nx_status_t nx_crc_native_reset(uint8_t index);
```

### State Query Functions

```c
/* Get CRC state */
nx_status_t nx_crc_native_get_state(uint8_t index, 
                                    bool* initialized,
                                    bool* suspended);

/* Get device descriptor */
nx_device_t* nx_crc_native_get_device(uint8_t index);
```

## Simulation Behavior

The Native platform CRC implementation simulates hardware CRC using software algorithms:

### CRC-32 (IEEE 802.3)

- **Polynomial**: 0x04C11DB7
- **Initial Value**: 0xFFFFFFFF (configurable)
- **Final XOR**: 0xFFFFFFFF (configurable)
- **Algorithm**: Table-driven for performance
- **Bit Order**: LSB first

### CRC-16 (CCITT)

- **Polynomial**: 0x1021
- **Initial Value**: 0xFFFF (configurable)
- **Final XOR**: 0x0000 (configurable)
- **Algorithm**: Table-driven for performance
- **Bit Order**: MSB first

### State Management

- **Initialized**: CRC is ready for use
- **Suspended**: CRC state is preserved but operations are paused
- **Running**: CRC is actively processing data

### Statistics

The implementation tracks:
- Reset count
- Update count
- Calculate count
- Total bytes processed

## Configuration

CRC instances are configured via Kconfig:

```kconfig
# Enable CRC support
CONFIG_CRC_NATIVE=y

# Number of CRC instances (1-4)
CONFIG_CRC_MAX_INSTANCES=1

# CRC0 configuration
CONFIG_INSTANCE_NX_CRC0=y
CONFIG_CRC0_ALGORITHM_CRC32=y
CONFIG_CRC0_POLYNOMIAL=0x04C11DB7
CONFIG_CRC0_INIT_VALUE=0xFFFFFFFF
CONFIG_CRC0_FINAL_XOR=0xFFFFFFFF
```

## Testing

The CRC implementation includes comprehensive tests:

### Unit Tests

- CRC-32 calculation correctness
- CRC-16 calculation correctness
- Lifecycle state transitions
- Power management
- Error conditions

### Property-Based Tests

- **Property 5: CRC Calculation Correctness**
  - Deterministic calculation
  - Incremental equals one-shot
  - Reset produces consistent results
  - Different inputs produce different results
  - Results within valid range
  - Incremental independent of split position
  - get_result is idempotent

## Known Test Vectors

### CRC-32

| Input | CRC-32 Result |
|-------|---------------|
| "123456789" | 0xCBF43926 |
| "" (empty) | 0x00000000 |

### CRC-16

| Input | CRC-16 Result |
|-------|---------------|
| "123456789" | 0x29B1 |
| "" (empty) | 0x0000 |

## Implementation Notes

1. **Performance**: Uses lookup tables for fast CRC calculation
2. **Thread Safety**: Not thread-safe by default (use external synchronization)
3. **Memory**: Minimal memory footprint (state + lookup tables)
4. **Accuracy**: Bit-accurate simulation of hardware CRC

## References

- [CRC-32 (IEEE 802.3)](https://en.wikipedia.org/wiki/Cyclic_redundancy_check)
- [CRC-16 (CCITT)](https://en.wikipedia.org/wiki/Cyclic_redundancy_check)
- [Nexus HAL CRC Interface](../../../../hal/include/hal/interface/nx_crc.h)
