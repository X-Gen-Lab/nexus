/**
 * \file            nx_i2c_async.c
 * \brief           I2C async interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements I2C asynchronous operations including read/write
 *                  with callback-based completion notification.
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "nx_i2c_helpers.h"
#include "nx_i2c_types.h"
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/* TX Async Interface Implementation                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Async send implementation
 */
static nx_status_t tx_async_send(nx_tx_async_t* self, const uint8_t* data,
                                 size_t len) {
    nx_i2c_impl_t* impl = NX_CONTAINER_OF(self, nx_i2c_impl_t, tx_async);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (!data) {
        return NX_ERR_NULL_PTR;
    }
    if (impl->state->busy) {
        return NX_ERR_BUSY;
    }

    /* Check if device handle is configured */
    if (!impl->state->current_device.in_use) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Simulate: write to TX buffer */
    size_t written = i2c_buffer_write(&impl->state->tx_buf, data, len);
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
    nx_i2c_impl_t* impl = NX_CONTAINER_OF(self, nx_i2c_impl_t, tx_async);

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
    nx_i2c_impl_t* impl = NX_CONTAINER_OF(self, nx_i2c_impl_t, tx_rx_async);

    /* Parameter validation */
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

    (void)timeout_ms; /* Ignore timeout in simulation */

    /* Simulate: write TX data to buffer if provided */
    if (tx_data && tx_len > 0) {
        size_t written =
            i2c_buffer_write(&impl->state->tx_buf, tx_data, tx_len);
        if (written < tx_len) {
            return NX_ERR_FULL;
        }
        impl->state->stats.tx_count += (uint32_t)tx_len;
    }

    /* Simulate: prepare RX data */
    uint8_t rx_data[256]; /* Temporary buffer for simulation */
    size_t rx_len = 0;

    size_t available = i2c_buffer_get_count(&impl->state->rx_buf);
    if (available > 0) {
        /* Read available data from RX buffer */
        size_t to_read =
            (available < sizeof(rx_data)) ? available : sizeof(rx_data);
        rx_len = i2c_buffer_read(&impl->state->rx_buf, rx_data, to_read);
        /* Note: rx_count already updated by inject function, don't update again
         */
    } else if (tx_data && tx_len > 0) {
        /* If no data in RX buffer, echo TX data for simulation */
        rx_len = (tx_len < sizeof(rx_data)) ? tx_len : sizeof(rx_data);
        for (size_t i = 0; i < rx_len; i++) {
            rx_data[i] = tx_data[i];
        }
        /* Update rx_count for echoed data */
        impl->state->stats.rx_count += (uint32_t)rx_len;
    }

    /* Invoke callback if registered and there's RX data */
    if (impl->state->current_device.callback && rx_len > 0) {
        impl->state->current_device.callback(
            impl->state->current_device.user_data, rx_data, rx_len);
    }

    return NX_OK;
}

/**
 * \brief           Get TX/RX state implementation
 */
static nx_status_t tx_rx_async_get_state(nx_tx_rx_async_t* self) {
    nx_i2c_impl_t* impl = NX_CONTAINER_OF(self, nx_i2c_impl_t, tx_rx_async);

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
void i2c_init_tx_async(nx_tx_async_t* tx_async) {
    tx_async->send = tx_async_send;
    tx_async->get_state = tx_async_get_state;
}

/**
 * \brief           Initialize TX/RX async interface
 */
void i2c_init_tx_rx_async(nx_tx_rx_async_t* tx_rx_async) {
    tx_rx_async->tx_rx = tx_rx_async_transfer;
    tx_rx_async->get_state = tx_rx_async_get_state;
}
