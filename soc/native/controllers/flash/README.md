# Native Platform Flash Implementation

This directory contains the Flash peripheral implementation for the Native platform.

## Overview

The Flash peripheral provides non-volatile memory storage with erase-before-write semantics. The Native platform implementation simulates Flash memory behavior including sector-based erase operations and persistence to disk.

## Features

- **Sector-Based Storage**: Configurable sector size (default 4KB)
- **Erase-Before-Write**: Enforces Flash write semantics
- **Persistence**: Saves Flash contents to file for testing persistence
- **Write Alignment**: Enforces write unit alignment requirements
- **Lock/Unlock**: Write protection mechanism
- **Lifecycle Management**: Init, deinit, suspend, resume operations
- **Power Management**: Enable, disable, power state callbacks

## Files

- `nx_flash_device.c` - Device registration and factory functions
- `nx_flash_interface.c` - Flash interface implementation
- `nx_flash_lifecycle.c` - Lifecycle management
- `nx_flash_power.c` - Power management
- `nx_flash_helpers.c` - Flash simulation helpers
- `nx_flash_helpers.h` - Helper function declarations
- `nx_flash_types.h` - Internal type definitions
- `Kconfig` - Configuration options

## Usage Example

```c
#include "hal/interface/nx_flash.h"
#include "native_flash_test.h"

/* Get Flash instance */
nx_internal_flash_t* flash = nx_flash_native_get(0);

/* Initialize */
nx_lifecycle_t* lifecycle = flash->get_lifecycle(flash);
lifecycle->init(lifecycle);

/* Unlock for writing */
flash->unlock(flash);

/* Get flash parameters */
uint32_t page_size = flash->get_page_size(flash);
size_t write_unit = flash->get_write_unit(flash);

/* Erase a sector */
uint32_t addr = 0;
flash->erase(flash, addr, page_size);

/* Write data (must be aligned to write_unit) */
uint8_t data[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
flash->write(flash, addr, data, sizeof(data));

/* Read data back */
uint8_t read_buf[16];
flash->read(flash, addr, read_buf, sizeof(read_buf));

/* Lock to prevent writes */
flash->lock(flash);

/* Cleanup (automatically saves to file) */
lifecycle->deinit(lifecycle);
```

## Test Helper Functions

The Native platform provides test helper functions for Flash testing:

### Factory Functions

```c
/* Get Flash instance by index (0-3) */
nx_internal_flash_t* nx_flash_native_get(uint8_t index);
```

### Reset Functions

```c
/* Reset all Flash instances */
void nx_flash_native_reset_all(void);

/* Reset specific Flash instance */
nx_status_t nx_flash_native_reset(uint8_t index);
```

### State Query Functions

```c
/* Get Flash state */
nx_status_t nx_flash_native_get_state(uint8_t index, 
                                      bool* initialized,
                                      bool* suspended);

/* Get lock status */
nx_status_t nx_flash_native_get_lock_status(uint8_t index, bool* locked);

/* Check if address range is erased */
bool nx_flash_native_is_erased(uint8_t index, uint32_t addr, size_t len);
```

### Persistence Functions

```c
/* Set backing file path for persistence */
nx_status_t nx_flash_native_set_backing_file(uint8_t index, const char* path);

/* Get backing file path */
nx_status_t nx_flash_native_get_backing_file(uint8_t index, char* path, 
                                             size_t path_len);
```

## Simulation Behavior

The Native platform Flash implementation simulates real Flash memory behavior:

### Memory Organization

- **Sector Size**: 4096 bytes (4KB) - configurable via Kconfig
- **Number of Sectors**: 128 sectors - configurable via Kconfig
- **Total Size**: 512KB (default configuration)
- **Write Unit**: 4 bytes (word-aligned) - configurable via Kconfig

### Erase Behavior

- **Erased Value**: 0xFF (all bits set)
- **Sector Alignment**: Erase operations round up to sector boundaries
- **Erase Time**: Instantaneous (no delay simulation)
- **State Tracking**: Each sector tracks whether it's erased

### Write Behavior

- **Erase Required**: Writing to non-erased memory returns `NX_ERR_INVALID_STATE`
- **Alignment**: Address and length must be aligned to write unit
- **Write Once**: After writing, the area is marked as non-erased
- **Lock Protection**: Writes fail with `NX_ERR_PERMISSION` when locked

### Read Behavior

- **No Restrictions**: Can read from any address at any time
- **Erased Data**: Reading erased sectors returns 0xFF
- **No Alignment**: Read operations don't require alignment

### Persistence

- **Automatic Save**: Flash contents are saved to file on deinit
- **Automatic Load**: Flash contents are loaded from file on init
- **File Format**: Binary format for efficiency
- **File Location**: Configurable via `nx_flash_native_set_backing_file()`
- **Default Path**: `flash_<index>.bin` in current directory

### State Management

- **Initialized**: Flash is ready for use, file loaded if exists
- **Suspended**: Flash state is preserved but operations return `NX_ERR_INVALID_STATE`
- **Running**: Flash is actively processing operations
- **Locked**: Write and erase operations are blocked

## Configuration

Flash instances are configured via Kconfig:

```kconfig
# Enable Flash support
CONFIG_FLASH_NATIVE=y

# Number of Flash instances (1-4)
CONFIG_FLASH_MAX_INSTANCES=1

# Flash0 configuration
CONFIG_INSTANCE_NX_FLASH0=y
CONFIG_FLASH0_SIZE=524288          # 512KB
CONFIG_FLASH0_PAGE_SIZE=4096       # 4KB sectors
CONFIG_FLASH0_WRITE_UNIT=4         # 4-byte alignment
CONFIG_FLASH0_BASE_ADDRESS=0x08000000
```

## Testing

The Flash implementation includes comprehensive tests:

### Unit Tests

- Erase operations (single/multiple sectors)
- Write operations (with/without erase)
- Read operations (erased/written data)
- Write alignment requirements
- Lock/unlock functionality
- Cross-sector boundary operations
- Lifecycle state transitions
- Power management
- Error conditions (NULL pointers, invalid addresses, uninitialized state)

### Property-Based Tests

- **Property 6: Flash Erase Before Write**
  - Writing without erase fails with `NX_ERR_INVALID_STATE`
  - Erase marks area as erased
  - Write after erase succeeds

- **Property 7: Flash Persistence Round Trip**
  - Write-save-load-read preserves data
  - Multiple persistence cycles preserve data
  - Read after persistence matches before persistence

## Common Use Cases

### Basic Read/Write

```c
/* Erase, write, read pattern */
flash->erase(flash, addr, page_size);
flash->write(flash, addr, data, len);
flash->read(flash, addr, buffer, len);
```

### Firmware Update Simulation

```c
/* Simulate firmware update */
uint32_t fw_addr = 0x10000;
size_t fw_size = 64 * 1024;  /* 64KB firmware */

/* Erase firmware area */
flash->erase(flash, fw_addr, fw_size);

/* Write firmware in chunks */
for (size_t offset = 0; offset < fw_size; offset += chunk_size) {
    flash->write(flash, fw_addr + offset, fw_data + offset, chunk_size);
}

/* Verify firmware */
flash->read(flash, fw_addr, verify_buf, fw_size);
```

### Persistence Testing

```c
/* Write data */
flash->erase(flash, addr, page_size);
flash->write(flash, addr, data, len);

/* Save and reload */
lifecycle->deinit(lifecycle);  /* Saves to file */
lifecycle->init(lifecycle);    /* Loads from file */

/* Verify data persisted */
flash->read(flash, addr, buffer, len);
/* buffer should match data */
```

## Error Handling

The Flash implementation returns standard error codes:

| Error Code | Condition |
|------------|-----------|
| `NX_OK` | Operation successful |
| `NX_ERR_NULL_PTR` | NULL pointer parameter |
| `NX_ERR_INVALID_PARAM` | Invalid address, length, or alignment |
| `NX_ERR_NOT_INIT` | Flash not initialized |
| `NX_ERR_INVALID_STATE` | Write without erase, or suspended |
| `NX_ERR_PERMISSION` | Operation blocked by lock |

## Implementation Notes

1. **Performance**: No delay simulation - operations are instantaneous
2. **Thread Safety**: Not thread-safe by default (use external synchronization)
3. **Memory**: Allocates sector array in memory (512KB default)
4. **File I/O**: Uses standard C file I/O for persistence
5. **Accuracy**: Accurately simulates Flash erase-before-write semantics

## Limitations

1. **No Wear Leveling**: Does not simulate Flash wear or limited erase cycles
2. **No Bad Blocks**: Does not simulate bad sectors or wear-out
3. **No Timing**: Operations complete instantly (no delay simulation)
4. **No Power Loss**: Does not simulate power loss during write
5. **Single Process**: File-based persistence is not multi-process safe

## References

- [Flash Memory](https://en.wikipedia.org/wiki/Flash_memory)
- [Nexus HAL Flash Interface](../../../../hal/include/hal/interface/nx_flash.h)
- [Native Platform Test Helpers](../../include/native_flash_test.h)

