/** Physical flash facts, independent of Board, storage and product layout. */
#ifndef NX_FLASH_GEOMETRY_H
#define NX_FLASH_GEOMETRY_H
#include <stdint.h>
#include <stddef.h>
#define NX_FLASH_ERASE_STALLS_EXEC (1u << 0)
#define NX_FLASH_PROGRAM_STALLS_EXEC (1u << 1)
typedef struct nx_flash_geometry_s {
    uintptr_t base_address;
    uint32_t size_bytes;
    uint32_t program_alignment;
    uint32_t block_count;
    uint32_t flags;
    uint8_t erased_value;
} nx_flash_geometry_t;
/** The complete physical erase block containing an offset. */
typedef struct nx_flash_block_s {
    uint32_t offset;
    uint32_t size;
    uint32_t index;
} nx_flash_block_t;
#endif
