/**
 * \file            layout.c
 * \brief           Verify external MPU linker reservations and real MPU_TYPE
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "private.h"

extern uint8_t __nexus_privileged_flash_start__[];
extern uint8_t __nexus_privileged_flash_end__[];
extern uint8_t __nexus_privileged_ram_start__[];
extern uint8_t __nexus_privileged_ram_end__[];
extern uint8_t __nexus_user_flash_start__[];
extern uint8_t __nexus_user_flash_end__[];
extern uint8_t __nexus_user_ram_start__[];
extern uint8_t __nexus_user_ram_end__[];
extern uint8_t __nexus_syscall_flash_start__[];
extern uint8_t __nexus_syscall_flash_end__[];
extern uint8_t __privileged_functions_start__[];
extern uint8_t __privileged_functions_end__[];
extern uint8_t __syscalls_flash_start__[];
extern uint8_t __syscalls_flash_end__[];
#if NEXUS_ARCH_MPU_VERSION == 7
extern uint8_t __privileged_data_start__[];
extern uint8_t __privileged_data_end__[];
extern uint8_t __FLASH_segment_start__[];
extern uint8_t __FLASH_segment_end__[];
extern uint8_t __SRAM_segment_start__[];
extern uint8_t __SRAM_segment_end__[];
#elif NEXUS_ARCH_MPU_VERSION == 8
extern uint8_t __privileged_sram_start__[];
extern uint8_t __privileged_sram_end__[];
extern uint8_t __unprivileged_flash_start__[];
extern uint8_t __unprivileged_flash_end__[];
#else
#error "MPU layout requires a maintained MPU v7 or v8 profile"
#endif

/** \brief Version-specific port bounds must match exact reserved regions. */
static bool port_bounds_match(const nx_freertos_mpu_layout_t* layout) {
    if ((uintptr_t)__privileged_functions_start__ !=
            layout->privileged_flash.begin ||
        (uintptr_t)__syscalls_flash_start__ != layout->syscall_flash.begin) {
        return false;
    }
#if NEXUS_ARCH_MPU_VERSION == 7
    nx_freertos_mpu_span_t flash = {(uintptr_t)__FLASH_segment_start__,
                                    (uintptr_t)__FLASH_segment_end__};
    nx_freertos_mpu_span_t ram = {(uintptr_t)__SRAM_segment_start__,
                                  (uintptr_t)__SRAM_segment_end__};
    size_t flash_bytes = flash.end - flash.begin;
    size_t ram_bytes = ram.end - ram.begin;
    return flash.begin < flash.end && ram.begin < ram.end &&
           (flash_bytes & (flash_bytes - 1)) == 0 &&
           (flash.begin & (flash_bytes - 1)) == 0 &&
           (ram_bytes & (ram_bytes - 1)) == 0 &&
           (ram.begin & (ram_bytes - 1)) == 0 &&
           flash.begin <= layout->privileged_flash.begin &&
           flash.end >= layout->privileged_flash.end &&
           flash.begin <= layout->user_flash.begin &&
           flash.end >= layout->user_flash.end &&
           flash.begin <= layout->syscall_flash.begin &&
           flash.end >= layout->syscall_flash.end &&
           ram.begin <= layout->privileged_ram.begin &&
           ram.end >= layout->privileged_ram.end &&
           ram.begin <= layout->user_ram.begin &&
           ram.end >= layout->user_ram.end &&
           (uintptr_t)__privileged_functions_end__ ==
               layout->privileged_flash.end &&
           (uintptr_t)__privileged_data_start__ ==
               layout->privileged_ram.begin &&
           (uintptr_t)__privileged_data_end__ == layout->privileged_ram.end &&
           (uintptr_t)__syscalls_flash_end__ + 1 == layout->syscall_flash.end;
#else
    return (uintptr_t)__privileged_functions_end__ + 1 ==
               layout->privileged_flash.end &&
           (uintptr_t)__privileged_sram_start__ ==
               layout->privileged_ram.begin &&
           (uintptr_t)__privileged_sram_end__ + 1 ==
               layout->privileged_ram.end &&
           (uintptr_t)__unprivileged_flash_start__ ==
               layout->user_flash.begin &&
           (uintptr_t)__unprivileged_flash_end__ + 1 ==
               layout->user_flash.end &&
           (uintptr_t)__syscalls_flash_end__ + 1 == layout->syscall_flash.end;
#endif
}

nx_result_t nx_freertos_mpu_layout(nx_freertos_mpu_layout_t* layout) {
    if (layout == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_freertos_mpu_layout_t value = {
        {(uintptr_t)__nexus_privileged_flash_start__,
         (uintptr_t)__nexus_privileged_flash_end__},
        {(uintptr_t)__nexus_privileged_ram_start__,
         (uintptr_t)__nexus_privileged_ram_end__},
        {(uintptr_t)__nexus_user_flash_start__,
         (uintptr_t)__nexus_user_flash_end__},
        {(uintptr_t)__nexus_user_ram_start__,
         (uintptr_t)__nexus_user_ram_end__},
        {(uintptr_t)__nexus_syscall_flash_start__,
         (uintptr_t)__nexus_syscall_flash_end__},
        NEXUS_ARCH_MPU_VERSION,
        (uint8_t)((*(const volatile uint32_t*)0xe000ed90U >> 8) & 0xffU)};
    if (!port_bounds_match(&value) ||
        value.regions != configTOTAL_MPU_REGIONS) {
        return NX_ERROR_INVALID;
    }
    *layout = value;
    return NX_SUCCESS;
}
