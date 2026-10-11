/**
 * \file            stm32_dma_model.h
 *
 * \brief           Host-only refused DMA disable fault injection boundary
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_DMA_MODEL_H
#define NEXUS_STM32_DMA_MODEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/** \brief Apply the hardware-model disable write or retain the EN fault. */
void nx_stm32_dma_model_disable(void* raw, uint32_t mask);
#ifdef __cplusplus
}
#endif

#define NX_STM32_DMA_DISABLE(stream, mask)                                     \
    nx_stm32_dma_model_disable((stream), (mask))

#endif
