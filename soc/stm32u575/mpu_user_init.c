/**
 * \file            mpu_user_init.c
 * \brief           Protected U575 user RAM reset copy and zero initialization
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_mpu_user_init.h"
#include <stddef.h>

/** \brief Empty spans may name the end, but never cross a reservation. */
static bool contains(nx_stm32u575_mpu_user_span_t span, uintptr_t address,
                     uintptr_t bytes) {
    return address >= span.begin && address <= span.end &&
           bytes <= span.end - address;
}

/** \brief Validate the complete trusted plan before dereferencing payload. */
static bool plan_valid(const nx_stm32u575_mpu_user_plan_t* plan) {
    uintptr_t address = (uintptr_t)plan;
    if (plan == NULL ||
        address % _Alignof(nx_stm32u575_mpu_user_plan_t) != 0U ||
        address > UINTPTR_MAX - sizeof(*plan)) {
        return false;
    }
    const nx_stm32u575_mpu_user_span_t spans[] = {
        plan->privileged_flash, plan->syscall_flash, plan->user_flash,
        plan->privileged_ram, plan->user_ram};
    for (size_t i = 0U; i < sizeof(spans) / sizeof(spans[0]); ++i) {
        if (spans[i].begin >= spans[i].end ||
            ((spans[i].begin | spans[i].end) & 31U) != 0U) {
            return false;
        }
        for (size_t j = 0U; j < i; ++j) {
            if (spans[i].begin < spans[j].end &&
                spans[j].begin < spans[i].end) {
                return false;
            }
        }
    }
    if (!contains(plan->privileged_ram, address, sizeof(*plan)) ||
        ((plan->data_load | plan->data_start | plan->data_end |
          plan->bss_start | plan->bss_end) &
         31U) != 0U ||
        plan->data_start > plan->data_end || plan->bss_start > plan->bss_end) {
        return false;
    }
    uintptr_t data_bytes = plan->data_end - plan->data_start;
    uintptr_t bss_bytes = plan->bss_end - plan->bss_start;
    if (!contains(plan->user_ram, plan->data_start, data_bytes) ||
        !contains(plan->user_ram, plan->bss_start, bss_bytes) ||
        !contains(plan->privileged_flash, plan->data_load, data_bytes)) {
        return false;
    }
    return data_bytes == 0U || bss_bytes == 0U ||
           plan->data_start >= plan->bss_end ||
           plan->bss_start >= plan->data_end;
}

bool nx_stm32u575_mpu_user_initialize(
    const nx_stm32u575_mpu_user_plan_t* plan) {
    if (!plan_valid(plan)) {
        return false;
    }
    uintptr_t source = plan->data_load;
    for (uintptr_t destination = plan->data_start; destination < plan->data_end;
         destination += sizeof(uint32_t)) {
        *(volatile uint32_t*)destination = *(const volatile uint32_t*)source;
        source += sizeof(uint32_t);
    }
    for (uintptr_t destination = plan->bss_start; destination < plan->bss_end;
         destination += sizeof(uint32_t)) {
        *(volatile uint32_t*)destination = 0U;
    }
    return true;
}

#ifndef NX_U575_MPU_USER_MODEL
extern uint8_t __nexus_privileged_flash_start__[];
extern uint8_t __nexus_privileged_flash_end__[];
extern uint8_t __nexus_syscall_flash_start__[];
extern uint8_t __nexus_syscall_flash_end__[];
extern uint8_t __nexus_user_flash_start__[];
extern uint8_t __nexus_user_flash_end__[];
extern uint8_t __nexus_privileged_ram_start__[];
extern uint8_t __nexus_privileged_ram_end__[];
extern uint8_t __nexus_user_ram_start__[];
extern uint8_t __nexus_user_ram_end__[];
extern uint8_t __nx_mpu_user_data_load[];
extern uint8_t __nx_mpu_user_data_start[];
extern uint8_t __nx_mpu_user_data_end[];
extern uint8_t __nx_mpu_user_bss_start[];
extern uint8_t __nx_mpu_user_bss_end[];

bool nx_stm32u575_mpu_user_boot(void) {
    const nx_stm32u575_mpu_user_plan_t plan = {
        {(uintptr_t)__nexus_privileged_flash_start__,
         (uintptr_t)__nexus_privileged_flash_end__},
        {(uintptr_t)__nexus_syscall_flash_start__,
         (uintptr_t)__nexus_syscall_flash_end__},
        {(uintptr_t)__nexus_user_flash_start__,
         (uintptr_t)__nexus_user_flash_end__},
        {(uintptr_t)__nexus_privileged_ram_start__,
         (uintptr_t)__nexus_privileged_ram_end__},
        {(uintptr_t)__nexus_user_ram_start__,
         (uintptr_t)__nexus_user_ram_end__},
        (uintptr_t)__nx_mpu_user_data_load,
        (uintptr_t)__nx_mpu_user_data_start,
        (uintptr_t)__nx_mpu_user_data_end,
        (uintptr_t)__nx_mpu_user_bss_start,
        (uintptr_t)__nx_mpu_user_bss_end};
    return nx_stm32u575_mpu_user_initialize(&plan);
}
#endif
