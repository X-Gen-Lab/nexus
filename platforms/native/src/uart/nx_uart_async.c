/**
 * \file            nx_uart_async.c
 * \brief           UART async interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements UART asynchronous operations including TX/RX
 *                  with callback-based completion notification.
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "nx_uart_helpers.h"
#include "nx_uart_types.h"
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/* TX Async Interface Implementation                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Async send implementation
 */
static nx_status_t tx_async_send(nx_tx_async_t* self, const uint8_t* data,
                                 size_t len) {
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, tx_async);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (!data) {
        return NX_ERR_NULL_PTR;
    }
    if (impl->state->tx_busy) {
        return NX_ERR_BUSY;
    }

    /* Simulate: write to stdout */
    fwrite(data, 1, len, stdout);
    fflush(stdout);

    /* Update statistics */
    impl->state->stats.tx_count += (uint32_t)len;

    return NX_OK;
}

/**
 * \brief           Get TX state implementation
 */
static nx_status_t tx_async_get_state(nx_tx_async_t* self) {
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, tx_async);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    return impl->state->tx_busy ? NX_ERR_BUSY : NX_OK;
}

/*---------------------------------------------------------------------------*/
/* RX Async Interface Implementation                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Async receive implementation
 */
static nx_status_t rx_async_receive(nx_rx_async_t* self, uint8_t* data,
                                    size_t* len) {
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, rx_async);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (!data || !len) {
        return NX_ERR_NULL_PTR;
    }

    /* Check data availability */
    size_t available = buffer_get_count(&impl->state->rx_buf);
    if (available == 0) {
        *len = 0;
        return NX_ERR_NO_DATA;
    }

    /* Read data from buffer */
    size_t to_read = (*len < available) ? *len : available;
    size_t read_count = buffer_read(&impl->state->rx_buf, data, to_read);
    *len = read_count;

    /* Update statistics */
    impl->state->stats.rx_count += (uint32_t)read_count;

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize TX async interface
 */
void uart_init_tx_async(nx_tx_async_t* tx_async) {
    tx_async->send = tx_async_send;
    tx_async->get_state = tx_async_get_state;
}

/**
 * \brief           Initialize RX async interface
 */
void uart_init_rx_async(nx_rx_async_t* rx_async) {
    rx_async->receive = rx_async_receive;
}
