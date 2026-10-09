/**
 * \file            nx_uart_sync.c
 * \brief           UART sync interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements UART synchronous operations including blocking
 *                  TX/RX with timeout support.
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "nx_uart_helpers.h"
#include "nx_uart_types.h"
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/* TX Sync Interface Implementation                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Sync send implementation
 */
static nx_status_t tx_sync_send(nx_tx_sync_t* self, const uint8_t* data,
                                size_t len, uint32_t timeout_ms) {
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, tx_sync);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (!data || len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Ignore timeout in simulation */
    (void)timeout_ms;

    /* Write data to TX buffer */
    size_t written = uart_buffer_write(&impl->state->tx_buf, data, len);
    if (written < len) {
        /* Buffer full */
        return NX_ERR_NO_MEMORY;
    }

    /* Update statistics */
    impl->state->stats.tx_count += (uint32_t)written;

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* RX Sync Interface Implementation                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Sync receive implementation
 */
static nx_status_t rx_sync_receive(nx_rx_sync_t* self, uint8_t* data,
                                   size_t* len, uint32_t timeout_ms) {
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, rx_sync);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (!data || !len) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Ignore timeout in simulation */
    (void)timeout_ms;

    /* Check data availability */
    size_t available = buffer_get_count(&impl->state->rx_buf);
    if (available == 0) {
        *len = 0;
        return NX_ERR_TIMEOUT;
    }

    /* Read data from buffer */
    size_t to_read = (*len < available) ? *len : available;
    size_t read_count = uart_buffer_read(&impl->state->rx_buf, data, to_read);
    *len = read_count;

    return NX_OK;
}

/**
 * \brief           Sync receive all implementation
 */
static nx_status_t rx_sync_receive_all(nx_rx_sync_t* self, uint8_t* data,
                                       size_t* len, uint32_t timeout_ms) {
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, rx_sync);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (!data || !len) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Ignore timeout in simulation */
    (void)timeout_ms;

    size_t requested = *len;
    size_t available = buffer_get_count(&impl->state->rx_buf);

    /* Check if enough data is available */
    if (available < requested) {
        *len = 0;
        return NX_ERR_TIMEOUT;
    }

    /* Read requested amount */
    size_t read_count = uart_buffer_read(&impl->state->rx_buf, data, requested);
    *len = read_count;

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize TX sync interface
 */
void uart_init_tx_sync(nx_tx_sync_t* tx_sync) {
    tx_sync->send = tx_sync_send;
}

/**
 * \brief           Initialize RX sync interface
 */
void uart_init_rx_sync(nx_rx_sync_t* rx_sync) {
    rx_sync->receive = rx_sync_receive;
    rx_sync->receive_all = rx_sync_receive_all;
}
