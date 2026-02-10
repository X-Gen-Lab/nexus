/**
 * \file            stm32_spi.h
 * \brief           STM32 SPI driver internal header
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef STM32_SPI_H
#define STM32_SPI_H

#include "stm32_spi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Internal Header - Not for direct user access                             */
/* Users should use Nexus HAL interfaces (nx_spi_t) only                    */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Interface Initialization Functions                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize TX async interface
 * \param[in]       tx_async: TX async interface pointer
 */
void spi_init_tx_async(nx_tx_async_t* tx_async);

/**
 * \brief           Initialize TX/RX async interface
 * \param[in]       tx_rx_async: TX/RX async interface pointer
 */
void spi_init_tx_rx_async(nx_tx_rx_async_t* tx_rx_async);

/**
 * \brief           Initialize TX sync interface
 * \param[in]       tx_sync: TX sync interface pointer
 */
void spi_init_tx_sync(nx_tx_sync_t* tx_sync);

/**
 * \brief           Initialize TX/RX sync interface
 * \param[in]       tx_rx_sync: TX/RX sync interface pointer
 */
void spi_init_tx_rx_sync(nx_tx_rx_sync_t* tx_rx_sync);

/**
 * \brief           Initialize lifecycle interface
 * \param[in]       lifecycle: Lifecycle interface pointer
 */
void spi_init_lifecycle(nx_lifecycle_t* lifecycle);

/**
 * \brief           Initialize power interface
 * \param[in]       power: Power interface pointer
 */
void spi_init_power(nx_power_t* power);

/*---------------------------------------------------------------------------*/
/* DMA Management Functions                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize DMA for SPI
 * \param[in]       impl: SPI implementation pointer
 * \return          Status code
 */
nx_status_t spi_dma_init(stm32_spi_impl_t* impl);

/**
 * \brief           Deinitialize DMA for SPI
 * \param[in]       impl: SPI implementation pointer
 * \return          Status code
 */
nx_status_t spi_dma_deinit(stm32_spi_impl_t* impl);

/**
 * \brief           Transmit and receive data using DMA
 * \param[in]       impl: SPI implementation pointer
 * \param[in]       tx_data: Data to transmit
 * \param[out]      rx_data: Receive buffer
 * \param[in]       len: Data length
 * \return          Status code
 */
nx_status_t spi_dma_transmit_receive(stm32_spi_impl_t* impl,
                                     const uint8_t* tx_data, uint8_t* rx_data,
                                     size_t len);

#ifdef __cplusplus
}
#endif

#endif /* STM32_SPI_H */
