/**
 * \file            nx_i2c_sync.c
 * \brief           I2C sync interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements I2C synchronous operations including blocking
 *                  read/write with timeout support.
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "nx_i2c_helpers.h"
#include "nx_i2c_types.h"
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/* TX Sync Interface Implementation                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Sync send implementation
 */
static nx_status_t tx_sync_send(nx_tx_sync_t* self, const uint8_t* data,
                                size_t len, uint32_t timeout_ms) {
    nx_i2c_impl_t* impl = NX_CONTAINER_OF(self, nx_i2c_impl_t, tx_sync);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (!data) {
        return NX_ERR_NULL_PTR;
    }

    /* Check if device handle is configured */
    if (!impl->state->current_device.in_use) {
        return NX_ERR_INVALID_PARAM;
    }

    (void)timeout_ms; /* Ignore timeout in simulation */

    /* Simulate: write to TX buffer */
    size_t written = i2c_buffer_write(&impl->state->tx_buf, data, len);
    if (written < len) {
        return NX_ERR_FULL;
    }

    /* Update statistics */
    impl->state->stats.tx_count += (uint32_t)len;

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* TX/RX Sync Interface Implementation                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Sync transfer implementation
 */
static nx_status_t tx_rx_sync_transfer(nx_tx_rx_sync_t* self,
                                       const uint8_t* tx_data, size_t tx_len,
                                       uint8_t* rx_data, size_t* rx_len,
                                       uint32_t timeout_ms) {
    nx_i2c_impl_t* impl = NX_CONTAINER_OF(self, nx_i2c_impl_t, tx_rx_sync);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (!rx_data || !rx_len) {
        return NX_ERR_NULL_PTR;
    }
    if (tx_len > 0 && !tx_data) {
        return NX_ERR_NULL_PTR;
    }

    /* Check if device handle is configured */
    if (!impl->state->current_device.in_use) {
        return NX_ERR_INVALID_PARAM;
    }

    (void)timeout_ms; /* Ignore timeout in simulation */

    /* Simulate: write TX data to buffer */
    if (tx_len > 0) {
        size_t written =
            i2c_buffer_write(&impl->state->tx_buf, tx_data, tx_len);
        if (written < tx_len) {
            return NX_ERR_FULL;
        }
    }

    /* Simulate: read RX data from buffer */
    size_t max_rx = *rx_len;
    size_t available = i2c_buffer_get_count(&impl->state->rx_buf);

    if (available > 0) {
        /* Read available data from RX buffer */
        size_t to_read = (available < max_rx) ? available : max_rx;
        size_t read_count =
            i2c_buffer_read(&impl->state->rx_buf, rx_data, to_read);
        *rx_len = read_count;
        /* Note: rx_count already updated by inject function, don't update again
         */
    } else if (tx_len > 0 && max_rx > 0) {
        /* If no data in RX buffer, echo TX data for simulation */
        size_t echo_len = (tx_len < max_rx) ? tx_len : max_rx;
        for (size_t i = 0; i < echo_len; i++) {
            rx_data[i] = tx_data[i];
        }
        *rx_len = echo_len;
        /* Update rx_count for echoed data */
        impl->state->stats.rx_count += (uint32_t)echo_len;
    } else {
        /* No TX data and no RX data available */
        *rx_len = 0;
        /* If this was a receive-only operation (tx_len == 0), return timeout */
        if (tx_len == 0 && max_rx > 0) {
            return NX_ERR_TIMEOUT;
        }
    }

    /* Update TX statistics */
    impl->state->stats.tx_count += (uint32_t)tx_len;

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize TX sync interface
 */
void i2c_init_tx_sync(nx_tx_sync_t* tx_sync) {
    tx_sync->send = tx_sync_send;
}

/**
 * \brief           Initialize TX/RX sync interface
 */
void i2c_init_tx_rx_sync(nx_tx_rx_sync_t* tx_rx_sync) {
    tx_rx_sync->tx_rx = tx_rx_sync_transfer;
}
