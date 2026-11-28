/**
 * \file            uart_duplex.h
 * \brief           Private F103 USART2 concurrent finite DMA composition.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_UART_DUPLEX_H
#define NEXUS_STM32F1_UART_DUPLEX_H

#include "drivers/uart/uart_dma.h"
#include "drivers/uart/uart_rx_dma.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nx_stm32f1_uart_dma_config_t tx;
    DMA_Channel_TypeDef* rx_channel;
    IRQn_Type rx_irq;
} nx_stm32f1_uart_duplex_config_t;

typedef struct {
    nx_stm32f1_uart_dma_t tx;
    nx_stm32f1_uart_rx_engine_t rx;
} nx_stm32f1_uart_duplex_t;

nx_result_t
nx_stm32f1_uart_duplex_admit(const nx_stm32f1_uart_duplex_t* state,
                             const nx_stm32f1_uart_duplex_config_t* config);
nx_result_t
nx_stm32f1_uart_duplex_prepare(nx_stm32f1_uart_duplex_t* state,
                               const nx_stm32f1_uart_duplex_config_t* config);
nx_result_t nx_stm32f1_uart_duplex_close(nx_stm32f1_uart_duplex_t* state);
nx_result_t
nx_stm32f1_uart_duplex_validate(void* context,
                                const nx_uart_tx_request_t* request);
void nx_stm32f1_uart_duplex_usart_irq(nx_stm32f1_uart_duplex_t* state);
void nx_stm32f1_uart_duplex_tx_irq(nx_stm32f1_uart_duplex_t* state);
void nx_stm32f1_uart_duplex_rx_irq(nx_stm32f1_uart_duplex_t* state);
extern const nx_uart_ops_t nx_stm32f1_uart_duplex_ops;

#ifdef __cplusplus
}
#endif
#endif
