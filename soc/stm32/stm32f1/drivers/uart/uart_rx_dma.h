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

/** \brief One RX lane receipt without an extra USART owner. */
typedef struct {
    nx_stream_fill_t fill;
    nx_stream_t* stream;
    uint32_t boundary;
    uint8_t previous_priority;
    bool lane_owned;
    bool vector_owned;
    bool live;
    bool pending;
    bool fault;
    bool closing;
} nx_stm32f1_uart_rx_engine_t;

typedef struct {
    nx_stm32f1_uart_t uart;
    nx_stm32f1_uart_rx_engine_t rx;
    const nx_stm32f1_uart_rx_dma_config_t* config;
    bool gate_added;
} nx_stm32f1_uart_rx_dma_t;

typedef struct {
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    const void* owner;
    size_t owner_size;
    const void* config;
    size_t config_size;
#if defined(NEXUS_STM32F1_UART_DUPLEX)
    const nx_uart_tx_request_t* tx;
    const bool* peer_closing;
#endif
} nx_stm32f1_uart_rx_storage_t;

#if defined(NEXUS_STM32F1_UART_DUPLEX)
nx_result_t nx_stm32f1_uart_rx_engine_admit(void);
nx_result_t nx_stm32f1_uart_rx_engine_prepare(nx_stm32f1_uart_rx_engine_t* rx,
                                              uint8_t priority);
nx_result_t nx_stm32f1_uart_rx_engine_start(
    nx_stm32f1_uart_rx_engine_t* rx, nx_stream_t* stream,
    const nx_stm32f1_uart_rx_storage_t* storage, bool shared_vector);
void nx_stm32f1_uart_rx_engine_irq(nx_stm32f1_uart_rx_engine_t* rx,
                                   bool shared_vector);
void nx_stm32f1_uart_rx_engine_service(nx_stm32f1_uart_rx_engine_t* rx,
                                       bool shared_vector,
                                       const bool* peer_closing);
nx_result_t nx_stm32f1_uart_rx_engine_stop(nx_stm32f1_uart_rx_engine_t* rx,
                                           bool shared_vector);
nx_result_t nx_stm32f1_uart_rx_engine_close(nx_stm32f1_uart_rx_engine_t* rx,
                                            bool shared_vector);
bool nx_stm32f1_uart_stream_contains(const nx_stream_t* stream,
                                     const void* address, size_t bytes);
#endif

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
