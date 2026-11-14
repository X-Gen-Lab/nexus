/**
 * \file            stm32f4_dma_route.h
 * \brief           Immutable resolved F4 stream identity without runtime state
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_DMA_ROUTE_H
#define NX_STM32F4_DMA_ROUTE_H

#include "stm32f4xx.h"
#include <stdbool.h>
#include <stddef.h>

/** \brief One configured DMA lane; channel is the unencoded selector 0--7.
 * \note The selected provider validates its peripheral request connection.
 *       This readonly record owns no stream, buffer, IRQ or lifecycle. Each
 *       mode retains its own completion and full-withdrawal contract.
 */
typedef struct {
    DMA_TypeDef* dma;
    DMA_Stream_TypeDef* stream;
    IRQn_Type irq;
    uint8_t lane;
    uint8_t channel;
} nx_stm32_dma_route_t;

/** \brief Check decoded F4 stream/vector identity without choosing a request.
 * \note This validates the DMA IP shape. The selected SoC binding separately
 *       admits its peripheral request connection; no allocation or MMIO read.
 *       Explicit register models may relocate register storage.
 */
static inline bool
nx_stm32_dma_route_valid(const nx_stm32_dma_route_t* selected) {
    if (selected == NULL || selected->dma == NULL || selected->stream == NULL ||
        selected->lane > 7U || selected->channel > 7U)
        return false;
#ifndef NEXUS_STM32_MODEL
    if (selected->dma != DMA1 && selected->dma != DMA2)
        return false;
    uintptr_t base = (uintptr_t)selected->dma;
    uintptr_t offset = DMA1_Stream0_BASE - DMA1_BASE;
    uintptr_t stride = DMA1_Stream1_BASE - DMA1_Stream0_BASE;
    uint32_t irq;
    if (selected->dma == DMA1) {
        irq = selected->lane < 7U ? (uint32_t)DMA1_Stream0_IRQn + selected->lane
                                  : (uint32_t)DMA1_Stream7_IRQn;
    } else {
        irq = selected->lane < 5U
                  ? (uint32_t)DMA2_Stream0_IRQn + selected->lane
                  : (uint32_t)DMA2_Stream5_IRQn + selected->lane - 5U;
    }
    return (uintptr_t)selected->stream ==
               base + offset + stride * selected->lane &&
           (uint32_t)selected->irq == irq;
#else
    return true;
#endif
}

#endif /* NX_STM32F4_DMA_ROUTE_H */
