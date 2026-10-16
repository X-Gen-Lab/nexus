/**
 * \file            uart_v1_channel_dma_model.h
 * \brief           External MMIO model for legacy USART channel DMA
 * \author          Nexus Team
 */
#ifndef NX_UART_V1_CHANNEL_DMA_MODEL_H
#define NX_UART_V1_CHANNEL_DMA_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_uart_v1_model_read(const volatile uint32_t* reg);
void nx_uart_v1_model_write(volatile uint32_t* reg, uint32_t value);
#define NX_UART_V1_READ(reg)         nx_uart_v1_model_read(reg)
#define NX_UART_V1_WRITE(reg, value) nx_uart_v1_model_write((reg), (value))
#ifdef __cplusplus
}
#endif
#endif
