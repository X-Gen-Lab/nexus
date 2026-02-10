/**
 * \file            stm32_spi_async.c
 * \brief           STM32 SPI asynchronous interface implementation
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#include "stm32_spi.h"
#include "stm32_spi_types.h"

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get implementation from TX async interface
 */
static inline stm32_spi_impl_t* spi_tx_async_get_impl(nx_tx_async_t* self) {
    return self ? NX_CONTAINER_OF(self, stm32_spi_impl_t, tx_async) : NULL;
}

/**
 * \brief           Get implementation from TX/RX async interface
 */
static inline stm32_spi_impl_t*
spi_tx_rx_async_get_impl(nx_tx_rx_async_t* self) {
    return self ? NX_CONTAINER_OF(self, stm32_spi_impl_t, tx_rx_async) : NULL;
}

/*---------------------------------------------------------------------------*/
/* TX Async Interface Implementation                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Transmit data asynchronously
 */
static nx_status_t spi_tx_async_transmit(nx_tx_async_t* self,
                                         const uint8_t* data, size_t len) {
    stm32_spi_impl_t* impl = spi_tx_async_get_impl(self);
    if (!impl || !impl->state || !data || len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INITIALIZED;
    }

    if (impl->state->busy) {
        return NX_ERR_BUSY;
    }

    impl->state->busy = true;

    /* Use DMA for async transfer */
    HAL_StatusTypeDef status =
        HAL_SPI_Transmit_DMA(&impl->hspi, (uint8_t*)data, len);

    if (status != HAL_OK) {
        impl->state->busy = false;
        return NX_ERR_IO;
    }

    return NX_OK;
}

/**
 * \brief           Transmit data asynchronously (zero-copy)
 */
static nx_status_t spi_tx_async_transmit_zerocopy(nx_tx_async_t* self,
                                                  const uint8_t* data,
                                                  size_t len) {
    /* Same as regular transmit for SPI (data must remain valid) */
    return spi_tx_async_transmit(self, data, len);
}

/*---------------------------------------------------------------------------*/
/* TX/RX Async Interface Implementation                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Transmit and receive data asynchronously
 */
static nx_status_t spi_tx_rx_async_transmit_receive(nx_tx_rx_async_t* self,
                                                    const uint8_t* tx_data,
                                                    uint8_t* rx_data,
                                                    size_t len) {
    stm32_spi_impl_t* impl = spi_tx_rx_async_get_impl(self);
    if (!impl || !impl->state || !tx_data || !rx_data || len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INITIALIZED;
    }

    if (impl->state->busy) {
        return NX_ERR_BUSY;
    }

    impl->state->busy = true;

    /* Use DMA for async transfer */
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive_DMA(
        &impl->hspi, (uint8_t*)tx_data, rx_data, len);

    if (status != HAL_OK) {
        impl->state->busy = false;
        return NX_ERR_IO;
    }

    return NX_OK;
}

/**
 * \brief           Transmit and receive data asynchronously (zero-copy)
 */
static nx_status_t
spi_tx_rx_async_transmit_receive_zerocopy(nx_tx_rx_async_t* self,
                                          const uint8_t* tx_data,
                                          uint8_t* rx_data, size_t len) {
    /* Same as regular transmit_receive for SPI (data must remain valid) */
    return spi_tx_rx_async_transmit_receive(self, tx_data, rx_data, len);
}

/**
 * \brief           Receive data asynchronously
 */
static nx_status_t spi_tx_rx_async_receive(nx_tx_rx_async_t* self,
                                           uint8_t* data, size_t len) {
    stm32_spi_impl_t* impl = spi_tx_rx_async_get_impl(self);
    if (!impl || !impl->state || !data || len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INITIALIZED;
    }

    if (impl->state->busy) {
        return NX_ERR_BUSY;
    }

    impl->state->busy = true;

    /* Use DMA for async transfer */
    HAL_StatusTypeDef status = HAL_SPI_Receive_DMA(&impl->hspi, data, len);

    if (status != HAL_OK) {
        impl->state->busy = false;
        return NX_ERR_IO;
    }

    return NX_OK;
}

/**
 * \brief           Receive data asynchronously (zero-copy)
 */
static nx_status_t spi_tx_rx_async_receive_zerocopy(nx_tx_rx_async_t* self,
                                                    uint8_t* data, size_t len) {
    /* Same as regular receive for SPI (data must remain valid) */
    return spi_tx_rx_async_receive(self, data, len);
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize TX async interface
 */
void spi_init_tx_async(nx_tx_async_t* tx_async) {
    NX_INIT_TX_ASYNC(tx_async, spi_tx_async_transmit,
                     spi_tx_async_transmit_zerocopy);
}

/**
 * \brief           Initialize TX/RX async interface
 */
void spi_init_tx_rx_async(nx_tx_rx_async_t* tx_rx_async) {
    NX_INIT_TX_RX_ASYNC(tx_rx_async, spi_tx_rx_async_transmit_receive,
                        spi_tx_rx_async_transmit_receive_zerocopy,
                        spi_tx_rx_async_receive,
                        spi_tx_rx_async_receive_zerocopy);
}
