# Native SDIO Implementation

## Overview

The Native SDIO implementation provides a simulated SD card interface for testing SDIO functionality without requiring actual hardware. It implements the complete `nx_sdio.h` interface with block storage simulation.

## Features

- **Block Storage**: Simulates 1024 blocks of 512 bytes each (512KB total)
- **Card Detection**: Simulates SD card presence/absence
- **Block Operations**: Read, write, and erase operations
- **Lifecycle Management**: Full init/deinit/suspend/resume support
- **Power Management**: Power interface implementation
- **Test Helpers**: Comprehensive test helper functions

## Usage Examples

### Basic Initialization

```c
#include "hal/interface/nx_sdio.h"
#include "native_sdio_test.h"

/* Get SDIO instance */
nx_sdio_t* sdio = nx_sdio_native_get(0);

/* Set card present */
nx_sdio_native_set_card_present(0, true);

/* Initialize */
nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
lifecycle->init(lifecycle);
```

### Reading and Writing Blocks

```c
/* Write data to block 0 */
uint8_t write_data[512] = {0x01, 0x02, 0x03, /* ... */};
sdio->write(sdio, 0, write_data, 1);

/* Read data from block 0 */
uint8_t read_data[512];
sdio->read(sdio, 0, read_data, 1);
```

### Multiple Block Operations

```c
/* Write 4 blocks starting at block 10 */
uint8_t write_data[2048];  /* 4 * 512 bytes */
/* Fill write_data... */
sdio->write(sdio, 10, write_data, 4);

/* Read 4 blocks */
uint8_t read_data[2048];
sdio->read(sdio, 10, read_data, 4);
```

### Erasing Blocks

```c
/* Erase 10 blocks starting at block 5 */
sdio->erase(sdio, 5, 10);

/* After erase, blocks contain 0xFF */
uint8_t read_data[512];
sdio->read(sdio, 5, read_data, 1);
/* read_data[i] == 0xFF for all i */
```

### Card Detection

```c
/* Check if card is present */
if (sdio->is_present(sdio)) {
    /* Card is present, can perform operations */
}

/* Simulate card removal */
nx_sdio_native_set_card_present(0, false);

/* Operations will fail with NX_ERR_INVALID_STATE */
```

### Getting Card Information

```c
/* Get block size (always 512 bytes) */
size_t block_size = sdio->get_block_size(sdio);

/* Get total capacity (524288 bytes = 512KB) */
uint64_t capacity = sdio->get_capacity(sdio);

/* Calculate number of blocks */
uint32_t num_blocks = (uint32_t)(capacity / block_size);  /* 1024 */
```

## Test Helper Functions

### Factory Functions

```c
/* Get SDIO instance by index (0-3) */
nx_sdio_t* nx_sdio_native_get(uint8_t index);
```

### Reset Functions

```c
/* Reset all SDIO instances */
void nx_sdio_native_reset_all(void);

/* Reset specific instance */
nx_status_t nx_sdio_native_reset(uint8_t index);
```

### State Query Functions

```c
/* Get initialization and suspend state */
bool initialized, suspended;
nx_sdio_native_get_state(0, &initialized, &suspended);
```

### Card Simulation Functions

```c
/* Set card present state */
nx_sdio_native_set_card_present(0, true);   /* Insert card */
nx_sdio_native_set_card_present(0, false);  /* Remove card */

/* Check card present state */
bool present = nx_sdio_native_is_card_present(0);
```

### Block Data Access

```c
/* Get block data directly for verification */
uint8_t block_data[512];
nx_sdio_native_get_block_data(0, block_num, block_data);
```

## Simulation Behavior

### Block Storage

- **Storage**: 1024 blocks × 512 bytes = 524288 bytes (512KB)
- **Initial State**: All blocks initialized to 0xFF (erased state) on init
- **Persistence**: Data persists across operations but not across resets
- **Alignment**: No alignment requirements (unlike real flash)

### Card Detection

- **Initial State**: Configurable via Kconfig (`NATIVE_SDIO_CARD_PRESENT`)
- **Dynamic Control**: Can be changed at runtime via test helpers
- **Operation Behavior**: Operations fail with `NX_ERR_INVALID_STATE` when card not present

### Error Conditions

The implementation validates:
- **NULL pointers**: Returns `NX_ERR_NULL_PTR`
- **Uninitialized state**: Returns `NX_ERR_NOT_INIT`
- **Card not present**: Returns `NX_ERR_INVALID_STATE`
- **Invalid block range**: Returns `NX_ERR_INVALID_PARAM`

### Lifecycle States

- **UNINITIALIZED**: Initial state, no operations allowed
- **RUNNING**: After init, all operations allowed
- **SUSPENDED**: After suspend, operations not allowed
- **RUNNING**: After resume, operations allowed again

## Configuration

### Kconfig Options

```kconfig
config NATIVE_SDIO
    bool "Enable SDIO support"
    default y

config NATIVE_SDIO_INSTANCES
    int "Number of SDIO instances"
    default 1
    range 1 4

config NATIVE_SDIO_CARD_PRESENT
    bool "Simulate card present at startup"
    default y

config NATIVE_SDIO_BLOCK_SIZE
    int "SDIO block size"
    default 512

config NATIVE_SDIO_NUM_BLOCKS
    int "Number of SDIO blocks"
    default 1024
```

## Testing

### Unit Tests

Unit tests cover:
- Lifecycle state transitions
- Power management
- Single and multiple block read/write
- Block erase operations
- Card detection
- Error conditions

### Property-Based Tests

Property tests verify:
- **Property 13**: Block read/write round trip
- **Additional**: Single block round trip
- **Additional**: Erase then read (0xFF verification)
- **Additional**: Multiple writes to same block

Run tests with:
```bash
python scripts/test/test.py -f "sdio"
```

## Limitations

- **No DMA**: All operations are synchronous
- **No Interrupts**: No interrupt simulation
- **No Timing**: Operations complete instantly
- **No Wear Leveling**: No simulation of flash wear
- **No Bad Blocks**: All blocks are always good
- **No Card Types**: Only simulates standard SD cards
- **No Bus Modes**: Bus width configuration has no effect

## Implementation Notes

- Block storage is allocated statically at compile time
- All operations are thread-safe (no concurrent access in simulation)
- Card presence can be changed dynamically for testing
- Reset functions clear all block data to zero

## See Also

- `hal/interface/nx_sdio.h` - SDIO interface definition
- `platforms/native/include/native_sdio_test.h` - Test helper declarations
- `tests/hal/test_nx_sdio.cpp` - Unit tests
- `tests/hal/test_nx_sdio_properties.cpp` - Property-based tests
