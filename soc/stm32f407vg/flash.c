/** Real STM32F407 xE/xG Flash port. xE reserves sectors 6/7; xG 10/11.
 * Flash erase/program can stall instruction fetch. This is a maintenance
 * operation, not a control-loop operation. Runtime power-fail HIL is pending. */
#include "flash.h"
#include "flash_geometry.h"
#include "nexus_config.h"
#include "arch/nx_arch.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <string.h>

#if NX_CONFIG_STM32_FLASH_SIZE == (512U * 1024U)
#define PARTITION_BASE 0x08040000U
#elif NX_CONFIG_STM32_FLASH_SIZE == (1024U * 1024U)
#define PARTITION_BASE 0x080C0000U
#else
#error "STM32F407 storage requires the exact xE or xG physical Flash size"
#endif
#define PARTITION_SIZE ((size_t)UINT32_C(0x40000))
#define SECTOR_SIZE ((size_t)UINT32_C(0x20000))
extern char __nexus_storage_start[] __attribute__((weak));
extern char __nexus_storage_end[] __attribute__((weak));
static volatile bool programming;
static bool range(size_t offset, size_t n) {
    return offset <= PARTITION_SIZE && n <= PARTITION_SIZE - offset;
}
static bool reserved(void) {
    nx_stm32f407_flash_geometry_t geometry;
    uint32_t physical_bytes=(uint32_t)*(volatile const uint16_t*)FLASHSIZE_BASE * 1024U;
    return nx_stm32f407_flash_geometry(physical_bytes,&geometry) &&
           geometry.flash_bytes == NX_CONFIG_STM32_FLASH_SIZE &&
           geometry.partition_base == PARTITION_BASE &&
           (uintptr_t)__nexus_storage_start == geometry.partition_base &&
           (uintptr_t)__nexus_storage_end == geometry.partition_base + geometry.partition_bytes;
}
static bool take(void) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    bool taken = !programming;
    if (taken) programming = true;
    nx_arch_dmb(); nx_arch_irq_restore(saved);
    return taken;
}
static void give(void) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    programming = false; nx_arch_dmb(); nx_arch_irq_restore(saved);
}
static void flush_data_cache(void) {
    bool enabled = (FLASH->ACR & FLASH_ACR_DCEN) != 0;
    __HAL_FLASH_DATA_CACHE_DISABLE();
    __HAL_FLASH_DATA_CACHE_RESET();
    if (enabled) __HAL_FLASH_DATA_CACHE_ENABLE();
    nx_arch_dsb(); nx_arch_isb();
}
static nx_storage_status_t read_data(void* ctx, size_t offset, void* data, size_t n) {
    (void)ctx;
    if (nx_arch_in_isr() || !reserved() || !range(offset,n) || (!data && n))
        return NX_STORAGE_INVALID;
    if (!take()) return NX_STORAGE_IO;
    if (n) memcpy(data, (const void*)(uintptr_t)(PARTITION_BASE+offset), n);
    give();
    return NX_STORAGE_OK;
}
static nx_storage_status_t program_data(void* ctx, size_t offset,
                                         const void* data, size_t n) {
    (void)ctx;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked() || !reserved() || !range(offset,n) || (!data && n) ||
        (offset & 3U) || (n & 3U)) return NX_STORAGE_INVALID;
    if (!take()) return NX_STORAGE_IO;
    nx_storage_status_t r = NX_STORAGE_OK;
    if (HAL_FLASH_Unlock() != HAL_OK) { give(); return NX_STORAGE_IO; }
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                            FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    for (size_t i=0; i<n; i+=4) {
        uint32_t word;
        memcpy(&word,(const uint8_t*)data+i,4);
        uint32_t address=PARTITION_BASE+(uint32_t)(offset+i);
        uint32_t old=*(volatile uint32_t*)(uintptr_t)address;
        if ((old & word) != word) { r=NX_STORAGE_INVALID; break; }
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,address,word) != HAL_OK) {
            r=NX_STORAGE_IO; break;
        }
        flush_data_cache();
        if (*(volatile uint32_t*)(uintptr_t)address != word) { r=NX_STORAGE_IO; break; }
    }
    if (HAL_FLASH_Lock() != HAL_OK) r=NX_STORAGE_IO;
    flush_data_cache(); give();
    return r;
}
static nx_storage_status_t erase_data(void* ctx, size_t offset, size_t n) {
    (void)ctx;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked() || !reserved() || !range(offset,n) || offset % SECTOR_SIZE ||
        n % SECTOR_SIZE) return NX_STORAGE_INVALID;
    if (!n) return NX_STORAGE_OK;
    if (!take()) return NX_STORAGE_IO;
    if (HAL_FLASH_Unlock() != HAL_OK) { give(); return NX_STORAGE_IO; }
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                            FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    FLASH_EraseInitTypeDef erase={0};
    erase.TypeErase=FLASH_TYPEERASE_SECTORS;
    nx_stm32f407_flash_geometry_t geometry;
    uint32_t first_sector, sector_count;
    if (!nx_stm32f407_flash_geometry(NX_CONFIG_STM32_FLASH_SIZE,&geometry) ||
        !nx_stm32f407_flash_erase_sector(&geometry,offset,n,&first_sector,&sector_count)) {
        (void)HAL_FLASH_Lock(); give(); return NX_STORAGE_INVALID;
    }
    erase.Sector=first_sector;
    erase.NbSectors=sector_count;
    erase.VoltageRange=FLASH_VOLTAGE_RANGE_3; /* VDD 2.7..3.6V board prerequisite. */
    uint32_t failed_sector=UINT32_MAX;
    nx_storage_status_t r=HAL_FLASHEx_Erase(&erase,&failed_sector)==HAL_OK ? NX_STORAGE_OK : NX_STORAGE_IO;
    if (HAL_FLASH_Lock()!=HAL_OK) r=NX_STORAGE_IO;
    flush_data_cache();
    if (r==NX_STORAGE_OK) for(size_t i=0;i<n;i+=4) {
        uint32_t address=PARTITION_BASE+(uint32_t)(offset+i);
        if (*(volatile const uint32_t*)(uintptr_t)address!=UINT32_MAX) {
            r=NX_STORAGE_IO; break;
        }
    }
    give();
    return r;
}
static nx_storage_status_t sync_data(void* ctx) {
    (void)ctx;
    if (nx_arch_in_isr() || !reserved()) return NX_STORAGE_INVALID;
    if (!take()) return NX_STORAGE_IO;
    flush_data_cache(); give();
    return NX_STORAGE_OK;
}
const nx_flash_port_t* nx_stm32f407_flash_port(void) {
    static const nx_flash_port_t port={.ctx=NULL, .size=PARTITION_SIZE,
        .erase_size=SECTOR_SIZE, .program_size=4, .read=read_data,
        .program=program_data, .erase=erase_data, .sync=sync_data};
    return reserved() ? &port : NULL;
}
