/**
 * \file            gd32_uart_dma_model.h
 * \brief           Host-only production DMA disable fault injection
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32_UART_DMA_MODEL_H
#define NEXUS_GD32_UART_DMA_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Model one channel-disable store, optionally retaining engine enable.
 */
void nx_gd32_dma_model_disable(uint32_t mask);
#ifdef __cplusplus
}
#endif
#define NX_GD32_DMA_DISABLE(mask) nx_gd32_dma_model_disable((uint32_t)(mask))
#endif
