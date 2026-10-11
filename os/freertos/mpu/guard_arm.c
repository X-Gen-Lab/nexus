/**
 * \file            guard_arm.c
 * \brief           Read actual raw SVC privilege and linker bootstrap origin
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "guard.h"
#include "nexus/arch/arch.h"

extern uint8_t __nexus_mpu_start_svc_return[];
extern uint8_t __nexus_privileged_flash_start__[];
extern uint8_t __nexus_privileged_flash_end__[];
extern uint8_t __nexus_privileged_ram_start__[];
extern uint8_t __nexus_privileged_ram_end__[];

nx_freertos_mpu_start_facts_t nx_freertos_mpu_start_facts(void) {
    uint32_t control;
    __asm__ volatile("mrs %0, control" : "=r"(control));
    nx_freertos_mpu_start_facts_t facts = {
        control,
        nx_arch_exception_number(),
        (uintptr_t)__nexus_mpu_start_svc_return,
        (uintptr_t)__nexus_privileged_flash_start__,
        (uintptr_t)__nexus_privileged_flash_end__,
        (uintptr_t)__nexus_privileged_ram_start__,
        (uintptr_t)__nexus_privileged_ram_end__};
    return facts;
}
