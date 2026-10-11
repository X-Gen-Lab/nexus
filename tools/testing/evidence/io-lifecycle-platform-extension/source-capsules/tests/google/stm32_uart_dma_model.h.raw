/**
 * \file            stm32_uart_dma_model.h
 * \brief           UART-only W1C fault boundary over shared DMA disable model
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_UART_DMA_MODEL_H
#define NEXUS_STM32_UART_DMA_MODEL_H
#include "stm32_dma_model.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Model W1C and a fault arriving after the observed status read. */
void nx_stm32_uart_dma_model_ack(volatile uint32_t* clear, uint32_t mask);
#ifdef __cplusplus
}
#endif
#define NX_STM32_UART_DMA_ACK(clear, mask)                                     \
    nx_stm32_uart_dma_model_ack((clear), (uint32_t)(mask))
#endif
