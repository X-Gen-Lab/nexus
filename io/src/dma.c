/**
 * \file            dma.c
 *
 * \brief           Overflow-safe DMA buffer and independent drain validation
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/dma.h"

/** \brief Validate domains before selecting one, rejecting wrapped metadata. */
nx_result_t nx_dma_buffer_validate(const nx_dma_memory_region_t* regions,
                                   size_t count, const void* data,
                                   size_t length, size_t alignment,
                                   nx_dma_direction_t direction) {
    uintptr_t begin = (uintptr_t)data;
    if (regions == NULL || count == 0U || data == NULL || length == 0U ||
        alignment == 0U || (alignment & (alignment - 1U)) != 0U ||
        begin % alignment != 0U || length > UINTPTR_MAX - begin ||
        (direction != NX_DMA_TO_DEVICE && direction != NX_DMA_FROM_DEVICE)) {
        return NX_ERROR_INVALID;
    }
    bool permitted = false;
    uint32_t permission = direction == NX_DMA_TO_DEVICE ? NX_DMA_MEMORY_READ
                                                        : NX_DMA_MEMORY_WRITE;
    for (size_t i = 0U; i < count; ++i) {
        const nx_dma_memory_region_t* region = &regions[i];
        if (region->size == 0U || region->size > UINTPTR_MAX - region->begin ||
            (region->permissions &
             ~(NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE)) != 0U) {
            return NX_ERROR_INVALID;
        }
        if ((region->permissions & permission) != 0U &&
            begin >= region->begin && begin - region->begin < region->size &&
            length <= region->size - (begin - region->begin)) {
            permitted = true;
        }
    }
    return permitted ? NX_SUCCESS : NX_ERROR_PERMISSION;
}

/** \brief A request selector cannot turn one physical stream into two engines.
 */
nx_result_t nx_dma_resources_validate(const nx_dma_resource_t* resources,
                                      size_t count) {
    if (resources == NULL && count != 0U) {
        return NX_ERROR_INVALID;
    }
    for (size_t i = 0U; i < count; ++i) {
        if (resources[i].controller == 0U || resources[i].controller > 2U ||
            resources[i].stream > 7U || resources[i].channel > 7U) {
            return NX_ERROR_INVALID;
        }
        for (size_t j = 0U; j < i; ++j) {
            if (resources[i].controller == resources[j].controller &&
                resources[i].stream == resources[j].stream) {
                return NX_ERROR_BUSY;
            }
        }
    }
    return NX_SUCCESS;
}

/** \brief Counter progress is deliberately independent from peripheral drain.
 */
nx_result_t nx_dma_drain_check(const nx_dma_drain_facts_t* facts) {
    if (facts == NULL || facts->length == 0U ||
        facts->remaining > facts->length) {
        return NX_ERROR_INVALID;
    }
    return facts->engine_disabled && facts->irq_detached &&
                   facts->peripheral_idle
               ? NX_SUCCESS
               : NX_ERROR_BUSY;
}
