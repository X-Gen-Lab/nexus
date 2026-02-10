/**
 * \file            stm32_uart_dma.h
 * \brief           STM32 UART DMA support header
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef STM32_UART_DMA_H
#define STM32_UART_DMA_H

#include "hal/nx_status.h"
#include "stm32_uart_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* DMA Initialization Functions                                              */
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

/*---------------------------------------------------------------------------*/
/* DMA Transfer Functions                                                    */
/*---------------------------------------------------------------------------*/

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
 * \param[out]      data: Buffer for received data
 * \param[in]       len: Maximum bytes to receive
 * \return          Status code
 */
nx_status_t uart_dma_receive(stm32_uart_impl_t* impl, uint8_t* data,
                             size_t len);

/*---------------------------------------------------------------------------*/
/* Zero-Copy DMA Functions                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Start zero-copy DMA transmission from circular buffer
 * \param[in]       impl: UART implementation pointer
 * \return          Status code
 * \details         Transmits data directly from circular buffer without copy
 */
nx_status_t uart_dma_transmit_zerocopy(stm32_uart_impl_t* impl);

/**
 * \brief           Start zero-copy DMA reception to circular buffer
 * \param[in]       impl: UART implementation pointer
 * \param[in]       len: Maximum bytes to receive
 * \return          Status code
 * \details         Receives data directly to circular buffer without copy
 */
nx_status_t uart_dma_receive_zerocopy(stm32_uart_impl_t* impl, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* STM32_UART_DMA_H */
