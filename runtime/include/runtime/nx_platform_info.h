/** Resolved platform identity. This API never initializes hardware. */
#ifndef NX_PLATFORM_INFO_H
#define NX_PLATFORM_INFO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NX_PLATFORM_MEMORY_MAIN_RAM,
    NX_PLATFORM_MEMORY_INTERNAL_FLASH
} nx_platform_memory_kind_t;

typedef struct {
    nx_platform_memory_kind_t kind;
    uintptr_t start;
    uint32_t bytes;
} nx_platform_memory_region_t;

typedef struct {
    const char* arch;
    const char* platform;
    const char* soc;
    const char* board;
    const char* backend;
    const char* board_sha256;  /**< Resolved Board manifest/config identity. */
    const char* layout_sha256; /**< Empty on a build without a Flash layout. */
    uint32_t main_ram_bytes;
    uint32_t physical_flash_bytes;
    uint32_t main_stack_bytes; /**< MSP/exception stack linker reservation. */
    uint32_t libc_heap_bytes;  /**< Separate from OSAL and kernel pools. */
    const nx_platform_memory_region_t* memory_regions;
    size_t memory_region_count;
} nx_platform_info_t;

/** Borrowed constant identity for this effective build configuration.
 * Native has no physical MCU memory and returns an empty region table.
 * The regions describe main SRAM and internal Flash; they are not a product
 * partition map or an exhaustive declaration of all SoC memory domains.
 * Backend capabilities/budgets remain authoritative in
 * osal_get_backend_info(); device capabilities use typed HAL discovery.
 * No allocation, lock, device open, clock change or scheduler start occurs. */
const nx_platform_info_t* nx_platform_get_info(void);

#ifdef __cplusplus
}
#endif
#endif
