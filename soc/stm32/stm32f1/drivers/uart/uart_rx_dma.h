/**
 * \file            uart_rx_dma.h
 * \brief           Private F103 finite USART2 RX DMA owner
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_UART_RX_DMA_H
#define NEXUS_STM32F1_UART_RX_DMA_H

#include "drivers/uart/instance.h"
#include "nexus/io/dma.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nx_stm32f1_uart_config_t uart;
    DMA_TypeDef* dma;
    DMA_Channel_TypeDef* channel;
    IRQn_Type irq;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
} nx_stm32f1_uart_rx_dma_config_t;

typedef struct {
    nx_stm32f1_uart_t uart;
    const nx_stm32f1_uart_rx_dma_config_t* config;
    nx_stream_t* stream;
    nx_stream_fill_t fill;
    uint32_t boundary;
    uint8_t previous_priority;
    bool gate_added;
    bool lane_owned;
    bool vector_owned;
    bool live;
    bool pending;
    bool fault;
    bool closing;
} nx_stm32f1_uart_rx_dma_t;

nx_result_t
nx_stm32f1_uart_rx_dma_admit(const nx_stm32f1_uart_rx_dma_t* state,
                             const nx_stm32f1_uart_rx_dma_config_t* config);
nx_result_t
nx_stm32f1_uart_rx_dma_prepare(nx_stm32f1_uart_rx_dma_t* state,
                               const nx_stm32f1_uart_rx_dma_config_t* config);
nx_result_t nx_stm32f1_uart_rx_dma_close(nx_stm32f1_uart_rx_dma_t* state);
void nx_stm32f1_uart_rx_dma_irq(nx_stm32f1_uart_rx_dma_t* state);

extern const nx_uart_ops_t nx_stm32f1_uart_rx_dma_ops;

#ifdef __cplusplus
}
#endif
#endif
