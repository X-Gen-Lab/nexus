/**
 * \file            memory_init.c
 * \brief           Pre-RAM memory-plan validation and section initialization
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
    return first < last && other < other_last && first < other_last &&
           other < last;
}

/** \brief Validate one record against the complete descriptor control span. */
static bool region_valid(const nx_stm32_memory_region_t* region,
                         uintptr_t flash_start, uintptr_t flash_end,
                         uintptr_t plans_start, uintptr_t plans_end,
                         uintptr_t protected_start, uintptr_t protected_end) {
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
        overlap(region->start, region->end, plans_start, plans_end))
        return false;
    uintptr_t size = region->data_end - region->data_start;
    if (region->data_load < flash_start || region->data_load > flash_end ||
        size > flash_end - region->data_load)
        return false;
    return protected_start == protected_end ||
           (!overlap(region->data_start, region->data_end, protected_start,
                     protected_end) &&
            !overlap(region->bss_start, region->bss_end, protected_start,
                     protected_end) &&
            !overlap(region->noinit_start, region->noinit_end, protected_start,
                     protected_end));
}

bool nx_stm32_memory_plan_valid(const nx_stm32_memory_region_t* regions,
                                size_t count, uintptr_t flash_start,
                                uintptr_t flash_end, uintptr_t protected_start,
                                uintptr_t protected_end) {
    uintptr_t plans_start = (uintptr_t)regions;
    if (regions == NULL || count == 0U || count > 3U ||
        plans_start % _Alignof(nx_stm32_memory_region_t) != 0U ||
        count * sizeof(*regions) > UINTPTR_MAX - plans_start ||
        flash_start >= flash_end || ((flash_start | flash_end) & 3U) != 0U ||
        protected_start > protected_end ||
        ((protected_start | protected_end) & 3U) != 0U ||
        ((protected_start == 0U) != (protected_end == 0U)))
        return false;
    uintptr_t plans_end = plans_start + count * sizeof(*regions);
    for (size_t i = 0U; i < count; ++i) {
        if (!region_valid(&regions[i], flash_start, flash_end, plans_start,
                          plans_end, protected_start, protected_end))
            return false;
        for (size_t j = 0U; j < i; ++j) {
            if (overlap(regions[i].start, regions[i].end, regions[j].start,
                        regions[j].end))
                return false;
        }
    }
    return true;
}

/** \brief The same pure validator precedes every actual section write. */
bool nx_stm32_memory_initialize(const nx_stm32_memory_region_t* region,
                                uintptr_t flash_start, uintptr_t flash_end) {
    if (!nx_stm32_memory_plan_valid(region, 1U, flash_start, flash_end, 0U, 0U))
        return false;
    uintptr_t source = region->data_load;
    for (uintptr_t address = region->data_start; address < region->data_end;
         address += sizeof(uint32_t)) {
        *(volatile uint32_t*)address = *(const uint32_t*)source;
        source += sizeof(uint32_t);
    }
    for (uintptr_t address = region->bss_start; address < region->bss_end;
         address += sizeof(uint32_t))
        *(volatile uint32_t*)address = 0U;
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
