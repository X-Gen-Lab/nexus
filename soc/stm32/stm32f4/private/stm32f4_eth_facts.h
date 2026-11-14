/**
 * \file            stm32f4_eth_facts.h
 * \brief           Selected physical DMA memory facts shared by both MAC faces
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_ETH_FACTS_H
#define NX_STM32F4_ETH_FACTS_H

#include "nexus/io/dma.h"
#include "nexus_config.h"

#if !defined(NEXUS_STM32_DMA_SRAM_ORIGIN) ||                                   \
    !defined(NEXUS_STM32_DMA_SRAM_BYTES) ||                                    \
    !defined(NEXUS_CPU_EXTERNAL_IRQ_COUNT)
#error "Ethernet requires the selected SoC memory and IRQ facts"
#endif

/** \brief Admit caller-authorized subregions against the selected silicon.
 * \param[in] regions: Stable readonly array, never a source of hardware facts.
 * \param[in] count: Nonzero array extent; arithmetic overflow is rejected.
 * \return SUCCESS, INVALID metadata or PERMISSION outside physical SRAM.
 * \note Bounded readonly cold admission, no allocation or resource effects.
 *       CPU-only CCM and external windows are not authorized by this profile.
 */
static inline nx_result_t
nx_stm32_eth_regions_admit(const nx_dma_memory_region_t* regions,
                           size_t count) {
    if (regions == NULL || count == 0U || count > SIZE_MAX / sizeof(*regions) ||
        (uintptr_t)regions > UINTPTR_MAX - count * sizeof(*regions))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < count; ++i) {
        const nx_dma_memory_region_t* region = &regions[i];
        if (region->begin < NEXUS_STM32_DMA_SRAM_ORIGIN || region->size == 0U ||
            region->size > NEXUS_STM32_DMA_SRAM_BYTES ||
            region->begin - NEXUS_STM32_DMA_SRAM_ORIGIN >
                NEXUS_STM32_DMA_SRAM_BYTES - region->size ||
            (region->permissions & ~3U) != 0U)
            return NX_ERROR_PERMISSION;
    }
    return NX_SUCCESS;
}

#endif /* NX_STM32F4_ETH_FACTS_H */
