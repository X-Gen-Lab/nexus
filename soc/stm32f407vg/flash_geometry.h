/** STM32F407 xE/xG single-bank geometry, independent of vendor headers. */
#ifndef NX_STM32F407_FLASH_GEOMETRY_H
#define NX_STM32F407_FLASH_GEOMETRY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint32_t flash_bytes, partition_base, partition_bytes, erase_bytes;
    uint32_t first_sector;
} nx_stm32f407_flash_geometry_t;
static inline bool nx_stm32f407_flash_geometry(uint32_t flash_bytes,
                                               nx_stm32f407_flash_geometry_t* out) {
    if (!out || (flash_bytes != 512U*1024U && flash_bytes != 1024U*1024U)) return false;
    *out=(nx_stm32f407_flash_geometry_t){.flash_bytes=flash_bytes,
        .partition_base=0x08000000U+flash_bytes-256U*1024U,
        .partition_bytes=256U*1024U,.erase_bytes=128U*1024U,
        .first_sector=flash_bytes==512U*1024U ? 6 : 10};
    return true;
}
static inline bool nx_stm32f407_flash_range(const nx_stm32f407_flash_geometry_t* geometry,
                                            size_t offset, size_t length) {
    return geometry && offset <= geometry->partition_bytes &&
        length <= geometry->partition_bytes-offset;
}
static inline bool nx_stm32f407_flash_erase_sector(const nx_stm32f407_flash_geometry_t* geometry,
                                                   size_t offset, size_t length,
                                                   uint32_t* first, uint32_t* count) {
    if (!geometry || !geometry->erase_bytes || !first || !count || !nx_stm32f407_flash_range(geometry,offset,length) ||
        offset%geometry->erase_bytes || length%geometry->erase_bytes) return false;
    *first=geometry->first_sector+(uint32_t)(offset/geometry->erase_bytes);
    *count=(uint32_t)(length/geometry->erase_bytes);
    return true;
}
#endif
