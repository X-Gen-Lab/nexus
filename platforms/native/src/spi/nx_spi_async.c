/**
 * \file            nx_spi_async.c
 * \brief           SPI async interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements SPI asynchronous operations including transfer
 *                  with callback-based completion notification.
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "nx_spi_helpers.h"
#include "nx_spi_types.h"
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/* TX Async Interface Implementation                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Async send implementation
 */
static nx_status_t tx_async_send(nx_tx_async_t* self, const uint8_t* data,
                                 size_t len) {
    nx_spi_impl_t* impl = NX_CONTAINER_OF(self, nx_spi_impl_t, tx_async);

    /* Parameter validation */
    if (!data) {
        return NX_ERR_INVALID_PARAM;
    }
    if (len == 0) {
        return NX_ERR_INVALID_PARAM;
    }
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (impl->state->busy) {
        return NX_ERR_BUSY;
    }

    /* Check if device handle is configured */
    if (!impl->state->current_device.in_use) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Simulate: write to TX buffer */
    size_t written = spi_buffer_write(&impl->state->tx_buf, data, len);
    if (written < len) {
        return NX_ERR_FULL;
    }

    /* Update statistics */
    impl->state->stats.tx_count += (uint32_t)len;

    return NX_OK;
}

/**
 * \brief           Get TX state implementation
 */
static nx_status_t tx_async_get_state(nx_tx_async_t* self) {
    nx_spi_impl_t* impl = NX_CONTAINER_OF(self, nx_spi_impl_t, tx_async);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    return impl->state->busy ? NX_ERR_BUSY : NX_OK;
}

/*---------------------------------------------------------------------------*/
/* TX/RX Async Interface Implementation                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Async transfer implementation
 */
static nx_status_t tx_rx_async_transfer(nx_tx_rx_async_t* self,
                                        const uint8_t* tx_data, size_t tx_len,
                                        uint32_t timeout_ms) {
    nx_spi_impl_t* impl = NX_CONTAINER_OF(self, nx_spi_impl_t, tx_rx_async);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (!tx_data) {
        return NX_ERR_NULL_PTR;
    }
    if (impl->state->busy) {
        return NX_ERR_BUSY;
    }

    /* Check if device handle is configured */
    if (!impl->state->current_device.in_use) {
        return NX_ERR_INVALID_PARAM;
    }

    (void)timeout_ms; /* Ignore timeout in simulation */

    /* Simulate: write TX data to buffer */
    size_t written = spi_buffer_write(&impl->state->tx_buf, tx_data, tx_len);
    if (written < tx_len) {
        return NX_ERR_FULL;
    }

    /* Update statistics */
    impl->state->stats.tx_count += (uint32_t)tx_len;

    /* Simulate: prepare RX data (echo back for simulation) */
    uint8_t rx_data[256]; /* Temporary buffer for simulation */
    size_t rx_len = (tx_len < sizeof(rx_data)) ? tx_len : sizeof(rx_data);

    size_t available = spi_buffer_get_count(&impl->state->rx_buf);
    if (available >= rx_len) {
        size_t read_count =
            spi_buffer_read(&impl->state->rx_buf, rx_data, rx_len);
        if (read_count > 0) {
            impl->state->stats.rx_count += (uint32_t)read_count;
        }
    } else {
        /* If no data in RX buffer, echo TX data for simulation */
        for (size_t i = 0; i < rx_len; i++) {
            rx_data[i] = tx_data[i];
        }
        impl->state->stats.rx_count += (uint32_t)rx_len;
    }

    /* Invoke callback if registered */
    if (impl->state->current_device.callback) {
        impl->state->current_device.callback(
            impl->state->current_device.user_data, rx_data, rx_len);
    }

    return NX_OK;
}

/**
 * \brief           Get TX/RX state implementation
 */
static nx_status_t tx_rx_async_get_state(nx_tx_rx_async_t* self) {
    nx_spi_impl_t* impl = NX_CONTAINER_OF(self, nx_spi_impl_t, tx_rx_async);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    return impl->state->busy ? NX_ERR_BUSY : NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize TX async interface
 */
void spi_init_tx_async(nx_tx_async_t* tx_async) {
    tx_async->send = tx_async_send;
    tx_async->get_state = tx_async_get_state;
}

/**
 * \brief           Initialize TX/RX async interface
 */
void spi_init_tx_rx_async(nx_tx_rx_async_t* tx_rx_async) {
    tx_rx_async->tx_rx = tx_rx_async_transfer;
    tx_rx_async->get_state = tx_rx_async_get_state;
}
