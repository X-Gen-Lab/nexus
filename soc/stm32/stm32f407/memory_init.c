/**
 * \file            memory_init.c
 * \brief           Pre-RAM checked initialization of selected CPU-only CCM
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "private/memory_init.h"
#include <stddef.h>

/** \brief Integer half-open spans avoid unrelated-object pointer ordering. */
static bool overlap(uintptr_t first, uintptr_t last, uintptr_t other,
                    uintptr_t other_last) {
    return first < other_last && other < last;
}

/** \brief Validate all control and section addresses before first mutation. */
bool nx_stm32_memory_initialize(const nx_stm32_memory_region_t* region,
                                uintptr_t flash_start, uintptr_t flash_end) {
    uintptr_t plans_start = (uintptr_t)region;
    if (region == NULL ||
        plans_start % _Alignof(nx_stm32_memory_region_t) != 0U ||
        sizeof(*region) > UINTPTR_MAX - plans_start ||
        flash_start >= flash_end || ((flash_start | flash_end) & 3U) != 0U) {
        return false;
    }
    uintptr_t alignment = region->start | region->end | region->data_load |
                          region->data_start | region->data_end |
                          region->bss_start | region->bss_end |
                          region->noinit_start | region->noinit_end;
    if ((alignment & 3U) != 0U || region->start >= region->end ||
        region->data_start < region->start ||
        region->data_start > region->data_end ||
        region->data_end > region->bss_start ||
        region->bss_start > region->bss_end ||
        region->bss_end > region->noinit_start ||
        region->noinit_start > region->noinit_end ||
        region->noinit_end > region->end ||
        overlap(region->start, region->end, flash_start, flash_end) ||
        overlap(region->start, region->end, plans_start,
                plans_start + sizeof(*region))) {
        return false;
    }
    uintptr_t size = region->data_end - region->data_start;
    if (region->data_load < flash_start || region->data_load > flash_end ||
        size > flash_end - region->data_load) {
        return false;
    }
    uintptr_t source = region->data_load;
    for (uintptr_t address = region->data_start; address < region->data_end;
         address += sizeof(uint32_t)) {
        *(volatile uint32_t*)address = *(const uint32_t*)source;
        source += sizeof(uint32_t);
    }
    for (uintptr_t address = region->bss_start; address < region->bss_end;
         address += sizeof(uint32_t)) {
        *(volatile uint32_t*)address = 0U;
    }
    return true;
}

#ifndef NEXUS_STM32_MEMORY_MODEL
extern uint32_t __nx_flash_start;
extern uint32_t __nx_flash_end;
extern uint32_t __nx_ccm_region_start;
extern uint32_t __nx_ccm_region_end;
extern uint32_t __nx_ccm_data_load;
extern uint32_t __nx_ccm_data_start;
extern uint32_t __nx_ccm_data_end;
extern uint32_t __nx_ccm_bss_start;
extern uint32_t __nx_ccm_bss_end;
extern uint32_t __nx_ccm_noinit_start;
extern uint32_t __nx_ccm_noinit_end;

/** \brief Flash symbols and current reset stack are valid before main RAM. */
bool nx_stm32_memory_boot(void) {
    const nx_stm32_memory_region_t region = {
        (uintptr_t)&__nx_ccm_region_start, (uintptr_t)&__nx_ccm_region_end,
        (uintptr_t)&__nx_ccm_data_load,    (uintptr_t)&__nx_ccm_data_start,
        (uintptr_t)&__nx_ccm_data_end,     (uintptr_t)&__nx_ccm_bss_start,
        (uintptr_t)&__nx_ccm_bss_end,      (uintptr_t)&__nx_ccm_noinit_start,
        (uintptr_t)&__nx_ccm_noinit_end};
    return nx_stm32_memory_initialize(&region, (uintptr_t)&__nx_flash_start,
                                      (uintptr_t)&__nx_flash_end);
}
#endif
