/**
 * \file            uart_dma.h
 * \brief           Private F103 USART2 finite TX DMA and independent IRQ RX.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_UART_DMA_H
#define NEXUS_STM32F1_UART_DMA_H

#include "drivers/uart/instance.h"
#include "nexus/io/dma.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Generated exact lane, vector, reachable regions and UART route. */
typedef struct {
    nx_stm32f1_uart_config_t uart;
    DMA_TypeDef* dma;
    DMA_Channel_TypeDef* channel;
    IRQn_Type irq;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
} nx_stm32f1_uart_dma_config_t;

/** \brief Independent DMA memory detachment and UART wire observation. */
typedef enum {
    NX_STM32F1_DMA_MOVING,
    NX_STM32F1_DMA_WITHDRAW,
    NX_STM32F1_DMA_WIRE,
    NX_STM32F1_DMA_COMPLETE
} nx_stm32f1_tx_phase_t;

/** \brief Extra request and hardware receipts exist only for the DMA mode. */
typedef struct {
    nx_stm32f1_uart_t uart;
    const nx_stm32f1_uart_dma_config_t* config;
    nx_uart_tx_request_t* active;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
    nx_stm32f1_tx_phase_t phase;
    uint8_t previous_priority;
    bool requested;
    bool lane_owned;
    bool gate_added;
    bool vector_owned;
    bool live;
    bool closing;
} nx_stm32f1_uart_dma_t;

nx_result_t
nx_stm32f1_uart_dma_admit(const nx_stm32f1_uart_dma_t* state,
                          const nx_stm32f1_uart_dma_config_t* config);
nx_result_t
nx_stm32f1_uart_dma_prepare(nx_stm32f1_uart_dma_t* state,
                            const nx_stm32f1_uart_dma_config_t* config);
nx_result_t nx_stm32f1_uart_dma_close(nx_stm32f1_uart_dma_t* state);
nx_result_t nx_stm32f1_uart_dma_validate(void* context,
                                         const nx_uart_tx_request_t* request);
void nx_stm32f1_uart_dma_irq(nx_stm32f1_uart_dma_t* state);
void nx_stm32f1_uart_dma_usart_irq(nx_stm32f1_uart_dma_t* state);
extern const nx_uart_ops_t nx_stm32f1_uart_dma_ops;

#if defined(NEXUS_STM32F1_UART_DUPLEX)
/** \brief Commit after the exact request and all peer aliases are checked. */
nx_result_t nx_stm32f1_uart_tx_engine_commit(nx_stm32f1_uart_dma_t* state,
                                             nx_uart_tx_request_t* request,
                                             bool admitted,
                                             const bool* peer_closing);
nx_result_t nx_stm32f1_uart_tx_engine_cancel(void* context,
                                             nx_uart_tx_request_t* request);
void nx_stm32f1_uart_tx_engine_service(void* context);
nx_result_t nx_stm32f1_uart_tx_engine_stop(void* context);
#endif

#ifdef __cplusplus
}
#endif
#endif
