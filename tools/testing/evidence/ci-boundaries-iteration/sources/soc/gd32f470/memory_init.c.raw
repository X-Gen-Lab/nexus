/**
 * \file            memory_init.c
 * \brief           Bound-checked word initialization of selected data regions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "private/memory_init.h"

/** \brief Half-open integer spans avoid unrelated-object pointer ordering. */
static bool overlap(uintptr_t first, uintptr_t last, uintptr_t other,
                    uintptr_t other_last) {
    return first < other_last && other < last;
}

/** \brief Check every address before any source read or destination write. */
static bool valid(const nx_gd32_memory_region_t* region, uintptr_t flash_start,
                  uintptr_t flash_end, uintptr_t plans_start,
                  uintptr_t plans_end) {
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
        overlap(region->start, region->end, plans_start, plans_end)) {
        return false;
    }
    uintptr_t size = region->data_end - region->data_start;
    return region->data_load >= flash_start && region->data_load <= flash_end &&
           size <= flash_end - region->data_load;
}

/** \brief All plans are valid before the first destination is modified. */
bool nx_gd32_memory_initialize(const nx_gd32_memory_region_t* regions,
                               size_t count, uintptr_t flash_start,
                               uintptr_t flash_end) {
    if (regions == NULL || count == 0U || count > 2U ||
        flash_start >= flash_end || ((flash_start | flash_end) & 3U) != 0U) {
        return false;
    }
    uintptr_t plans_start = (uintptr_t)regions;
    size_t plans_size = count * sizeof(*regions);
    if (plans_start % _Alignof(nx_gd32_memory_region_t) != 0U ||
        plans_size > UINTPTR_MAX - plans_start) {
        return false;
    }
    uintptr_t plans_end = plans_start + plans_size;
    for (size_t index = 0U; index < count; ++index) {
        if (!valid(&regions[index], flash_start, flash_end, plans_start,
                   plans_end)) {
            return false;
        }
        if (index != 0U && overlap(regions[0].start, regions[0].end,
                                   regions[index].start, regions[index].end)) {
            return false;
        }
    }
    for (size_t index = 0U; index < count; ++index) {
        const nx_gd32_memory_region_t* region = &regions[index];
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
    }
    return true;
}
