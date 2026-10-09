/** STM32F407 xE/xG full physical geometry; no product partitions. */
#ifndef NX_STM32F407_FLASH_GEOMETRY_H
#define NX_STM32F407_FLASH_GEOMETRY_H
#include "hal/interface/nx_flash_geometry.h"
#include <stdbool.h>
static inline bool nx_stm32f407_flash_geometry(uint32_t bytes,nx_flash_geometry_t* out) {
    if (!out || (bytes!=512U*1024U && bytes!=1024U*1024U)) return false;
    *out=(nx_flash_geometry_t){.base_address=0x08000000U,.size_bytes=bytes,
        .program_alignment=4,.block_count=bytes==512U*1024U ? 8 : 12,
        .flags=NX_FLASH_ERASE_STALLS_EXEC|NX_FLASH_PROGRAM_STALLS_EXEC,.erased_value=0xFF};
    return true;
}
static inline bool nx_stm32f407_flash_range(const nx_flash_geometry_t* geometry,
                                            uint32_t offset,size_t length) {
    return geometry && offset<=geometry->size_bytes && length<=geometry->size_bytes-offset;
}
static inline bool nx_stm32f407_flash_block(const nx_flash_geometry_t* geometry,
                                           uint32_t offset,nx_flash_block_t* out) {
    if (!geometry || !out || offset>=geometry->size_bytes) return false;
    if (offset<0x10000U) {
        *out=(nx_flash_block_t){.offset=offset&~0x3FFFU,.size=0x4000U,.index=offset/0x4000U};
    } else if (offset<0x20000U) {
        *out=(nx_flash_block_t){.offset=0x10000U,.size=0x10000U,.index=4};
    } else {
        *out=(nx_flash_block_t){.offset=offset&~0x1FFFFU,.size=0x20000U,
            .index=5U+(offset-0x20000U)/0x20000U};
    }
    return true;
}
/* Exact physical block boundaries; never round into adjacent regions. */
static inline bool nx_stm32f407_flash_erase_sector(const nx_flash_geometry_t* geometry,
                                                   uint32_t offset,size_t length,
                                                   uint32_t* first,uint32_t* count) {
    if (!first || !count || !nx_stm32f407_flash_range(geometry,offset,length)) return false;
    if (!length) { *first=0; *count=0; return true; }
    nx_flash_block_t start,end;
    if (!nx_stm32f407_flash_block(geometry,offset,&start) || start.offset!=offset ||
        !nx_stm32f407_flash_block(geometry,offset+(uint32_t)length-1U,&end) ||
        end.offset+end.size!=offset+length) return false;
    *first=start.index; *count=end.index-start.index+1U;
    return true;
}
#endif
