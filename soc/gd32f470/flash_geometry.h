/** GD32F470ZG full geometry, using its additional 4KiB page erase. */
#ifndef NX_GD32F470_FLASH_GEOMETRY_H
#define NX_GD32F470_FLASH_GEOMETRY_H
#include "hal/interface/nx_flash_geometry.h"
#include <stdbool.h>
static inline bool nx_gd32f470_flash_geometry(uint32_t bytes,nx_flash_geometry_t* out) {
    if (!out || bytes!=1024U*1024U) return false;
    *out=(nx_flash_geometry_t){.base_address=0x08000000U,.size_bytes=bytes,
        .program_alignment=2,.block_count=256,.erased_value=0xFF,
        .flags=NX_FLASH_ERASE_STALLS_EXEC|NX_FLASH_PROGRAM_STALLS_EXEC};
    return true;
}
static inline bool nx_gd32f470_flash_range(const nx_flash_geometry_t* geometry,
                                         uint32_t offset,size_t length) {
    return geometry && offset<=geometry->size_bytes && length<=geometry->size_bytes-offset;
}
static inline bool nx_gd32f470_flash_block(const nx_flash_geometry_t* geometry,
                                         uint32_t offset,nx_flash_block_t* out) {
    if (!geometry || !out || offset>=geometry->size_bytes) return false;
    *out=(nx_flash_block_t){.offset=offset&~0xFFFU,.size=4096,.index=offset/4096};
    return true;
}
#endif
