/**
 * \file            stm32_uart.h
 * \brief           STM32 UART driver internal header
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef STM32_UART_H
#define STM32_UART_H

#include "stm32_uart_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Internal Header - Not for direct user access                             */
/* Users should use Nexus HAL interfaces (nx_uart_t) only                   */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Interface Initialization Functions                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize TX async interface
 * \param[in]       tx_async: TX async interface pointer
 */
void uart_init_tx_async(nx_tx_async_t* tx_async);

/**
 * \brief           Initialize RX async interface
 * \param[in]       rx_async: RX async interface pointer
 */
void uart_init_rx_async(nx_rx_async_t* rx_async);

/**
 * \brief           Initialize TX sync interface
 * \param[in]       tx_sync: TX sync interface pointer
 */
void uart_init_tx_sync(nx_tx_sync_t* tx_sync);

/**
 * \brief           Initialize RX sync interface
 * \param[in]       rx_sync: RX sync interface pointer
 */
void uart_init_rx_sync(nx_rx_sync_t* rx_sync);

/**
 * \brief           Initialize lifecycle interface
 * \param[in]       lifecycle: Lifecycle interface pointer
 */
void uart_init_lifecycle(nx_lifecycle_t* lifecycle);

/**
 * \brief           Initialize power interface
 * \param[in]       power: Power interface pointer
 */
void uart_init_power(nx_power_t* power);

/*---------------------------------------------------------------------------*/
/* ISR Management Functions                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Register UART interrupt handler
 * \param[in]       impl: UART implementation pointer
 * \return          Status code
 */
nx_status_t uart_register_isr(stm32_uart_impl_t* impl);

/**
 * \brief           Unregister UART interrupt handler
 * \param[in]       impl: UART implementation pointer
 * \return          Status code
 */
nx_status_t uart_unregister_isr(stm32_uart_impl_t* impl);

/*---------------------------------------------------------------------------*/
/* DMA Management Functions                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize DMA for UART
 * \param[in]       impl: UART implementation pointer
 * \return          Status code
 */
nx_status_t uart_dma_init(stm32_uart_impl_t* impl);

/**
 * \brief           Deinitialize DMA for UART
 * \param[in]       impl: UART implementation pointer
 * \return          Status code
 */
nx_status_t uart_dma_deinit(stm32_uart_impl_t* impl);

/**
 * \brief           Transmit data using DMA
 * \param[in]       impl: UART implementation pointer
 * \param[in]       data: Data to transmit
 * \param[in]       len: Data length
 * \return          Status code
 */
nx_status_t uart_dma_transmit(stm32_uart_impl_t* impl, const uint8_t* data,
                              size_t len);

/**
 * \brief           Receive data using DMA
 * \param[in]       impl: UART implementation pointer
 * \param[out]      data: Data buffer
 * \param[in]       len: Buffer length
 * \return          Status code
 */
nx_status_t uart_dma_receive(stm32_uart_impl_t* impl, uint8_t* data,
                             size_t len);

/**
 * \brief           Transmit data using DMA (zero-copy)
 * \param[in]       impl: UART implementation pointer
 * \param[in]       data: Data pointer (must remain valid)
 * \param[in]       len: Data length
 * \return          Status code
 */
nx_status_t uart_dma_transmit_zerocopy(stm32_uart_impl_t* impl,
                                       const uint8_t* data, size_t len);

/**
 * \brief           Receive data using DMA (zero-copy)
 * \param[in]       impl: UART implementation pointer
 * \param[out]      data: Data buffer pointer (must remain valid)
 * \param[in]       len: Buffer length
 * \return          Status code
 */
nx_status_t uart_dma_receive_zerocopy(stm32_uart_impl_t* impl, uint8_t* data,
                                      size_t len);

#ifdef __cplusplus
}
#endif

#endif /* STM32_UART_H */
