#include "runtime/nx_platform_info.h"
#include "nexus_config.h"

#if defined(NX_CONFIG_PLATFORM_NATIVE)
#define PLATFORM_ARCH "native"
#define PLATFORM_SOC "native-simulation"
#define PLATFORM_RAM 0u
#define PLATFORM_FLASH 0u
#define PLATFORM_REGIONS NULL
#define PLATFORM_REGION_COUNT 0u
#else
#define PLATFORM_ARCH NX_CONFIG_CPU_ARCH
#if defined(NX_CONFIG_PLATFORM_STM32)
#define PLATFORM_SOC NX_CONFIG_STM32_PART_NAME
#elif defined(NX_CONFIG_PLATFORM_GD32F470)
#define PLATFORM_SOC "GD32F470ZGT6"
#else
#error "The selected SoC requires resolved platform identity"
#endif
#define PLATFORM_RAM NX_CONFIG_LINKER_RAM_SIZE
#define PLATFORM_FLASH NX_CONFIG_LINKER_FLASH_SIZE
static const nx_platform_memory_region_t regions[] = {
    {NX_PLATFORM_MEMORY_MAIN_RAM, NX_CONFIG_LINKER_RAM_START, PLATFORM_RAM},
    {NX_PLATFORM_MEMORY_INTERNAL_FLASH, NX_CONFIG_LINKER_FLASH_START, PLATFORM_FLASH}
};
#define PLATFORM_REGIONS regions
#define PLATFORM_REGION_COUNT (sizeof(regions) / sizeof(regions[0]))
#endif

static const nx_platform_info_t info = {
    .arch = PLATFORM_ARCH,
    .platform = NX_CONFIG_PLATFORM_NAME,
    .soc = PLATFORM_SOC,
    .board = NX_CONFIG_BOARD_NAME,
    .backend = NX_CONFIG_OSAL_BACKEND_NAME,
    .board_sha256 = NEXUS_BOARD_SHA256,
    .layout_sha256 = NEXUS_LAYOUT_SHA256,
    .main_ram_bytes = PLATFORM_RAM,
    .physical_flash_bytes = PLATFORM_FLASH,
    .main_stack_bytes = NX_CONFIG_FIRMWARE_MAIN_STACK_SIZE,
    .libc_heap_bytes = NX_CONFIG_FIRMWARE_LIBC_HEAP_SIZE,
    .memory_regions = PLATFORM_REGIONS,
    .memory_region_count = PLATFORM_REGION_COUNT
};

const nx_platform_info_t* nx_platform_get_info(void) { return &info; }
