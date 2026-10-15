/**
 * \file            uart_multi_controller_model.h
 * \brief           Selected DMA W1C and refused-disable hardware boundaries
 * \author          Nexus Team
 */
#ifndef NEXUS_UART_MULTI_CONTROLLER_MODEL_H
#define NEXUS_UART_MULTI_CONTROLLER_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Apply the exact clear mask to the corresponding status bank. */
void nx_uart_multi_dma_ack(volatile uint32_t* clear, uint32_t mask);
/** \brief Preserve a selected EN fault without changing another DMA engine. */
void nx_uart_multi_dma_disable(volatile uint32_t* control, uint32_t mask);
#ifdef __cplusplus
}
#endif
#define NX_STM32_DMA_DISABLE(stream, mask)                                     \
    nx_uart_multi_dma_disable(&(stream)->CR, (mask))
#define NX_STM32_UART_DMA_ACK(clear, mask)                                     \
    nx_uart_multi_dma_ack((clear), (mask))
#define NX_GD32_UART_DMA_DISABLE(control, mask)                                \
    nx_uart_multi_dma_disable((control), (mask))
#define NX_GD32_UART_DMA_ACK(clear, mask) nx_uart_multi_dma_ack((clear), (mask))
#endif
