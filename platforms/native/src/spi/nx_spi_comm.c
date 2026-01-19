/**
 * \file            nx_spi_comm.c
 * \brief           SPI communication interface implementations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements SPI communication operations shared between
 *                  sync and async interfaces.
 */

#include "hal/nx_status.h"
#include "nx_spi_helpers.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* TX Async Interface                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Send data asynchronously
 */
nx_status_t nx_spi_tx_async_send(nx_tx_async_t* self, const uint8_t* data,
                                 size_t len) {
    nx_spi_impl_t* impl =
        (nx_spi_impl_t*)((char*)self - offsetof(nx_spi_impl_t, tx_async));
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->locked) {
        return NX_ERR_LOCKED;
    }

    /* Simulate transfer */
    state->stats.tx_count += (uint32_t)len;
    printf("[SPI%d] TX: %zu bytes\n", state->index, len);

    return NX_OK;
}

/**
 * \brief           Get TX async state
 */
nx_status_t nx_spi_tx_async_get_state(nx_tx_async_t* self) {
    nx_spi_impl_t* impl =
        (nx_spi_impl_t*)((char*)self - offsetof(nx_spi_impl_t, tx_async));
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    return impl->state->initialized ? NX_OK : NX_ERR_NOT_INIT;
}

/*---------------------------------------------------------------------------*/
/* TX/RX Async Interface                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Send data asynchronously (TX/RX)
 */
nx_status_t nx_spi_tx_rx_async_send(nx_tx_rx_async_t* self, const uint8_t* data,
                                    size_t len) {
    nx_spi_impl_t* impl =
        (nx_spi_impl_t*)((char*)self - offsetof(nx_spi_impl_t, tx_rx_async));
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    state->stats.tx_count += (uint32_t)len;
    printf("[SPI%d] TX/RX: %zu bytes\n", state->index, len);

    return NX_OK;
}

/**
 * \brief           Receive data asynchronously
 */
nx_status_t nx_spi_tx_rx_async_receive(nx_tx_rx_async_t* self, uint8_t* data,
                                       size_t* len) {
    nx_spi_impl_t* impl =
        (nx_spi_impl_t*)((char*)self - offsetof(nx_spi_impl_t, tx_rx_async));
    if (!impl || !impl->state || !data || !len) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Simulate: fill with dummy data */
    memset(data, 0xFF, *len);
    state->stats.rx_count += (uint32_t)*len;

    return NX_OK;
}

/**
 * \brief           Get TX/RX async state
 */
nx_status_t nx_spi_tx_rx_async_get_state(nx_tx_rx_async_t* self) {
    nx_spi_impl_t* impl =
        (nx_spi_impl_t*)((char*)self - offsetof(nx_spi_impl_t, tx_rx_async));
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    return impl->state->initialized ? NX_OK : NX_ERR_NOT_INIT;
}

/*---------------------------------------------------------------------------*/
/* TX Sync Interface                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Send data synchronously
 */
nx_status_t nx_spi_tx_sync_send(nx_tx_sync_t* self, const uint8_t* data,
                                size_t len, uint32_t timeout_ms) {
    nx_spi_impl_t* impl =
        (nx_spi_impl_t*)((char*)self - offsetof(nx_spi_impl_t, tx_sync));
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    (void)timeout_ms;
    state->stats.tx_count += (uint32_t)len;
    printf("[SPI%d] TX Sync: %zu bytes\n", state->index, len);

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* TX/RX Sync Interface                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Send data synchronously (TX/RX)
 */
nx_status_t nx_spi_tx_rx_sync_send(nx_tx_rx_sync_t* self, const uint8_t* data,
                                   size_t len, uint32_t timeout_ms) {
    nx_spi_impl_t* impl =
        (nx_spi_impl_t*)((char*)self - offsetof(nx_spi_impl_t, tx_rx_sync));
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    (void)timeout_ms;
    state->stats.tx_count += (uint32_t)len;

    return NX_OK;
}

/**
 * \brief           Receive data synchronously
 */
nx_status_t nx_spi_tx_rx_sync_receive(nx_tx_rx_sync_t* self, uint8_t* data,
                                      size_t* len, uint32_t timeout_ms) {
    nx_spi_impl_t* impl =
        (nx_spi_impl_t*)((char*)self - offsetof(nx_spi_impl_t, tx_rx_sync));
    if (!impl || !impl->state || !data || !len) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    (void)timeout_ms;
    /* Simulate: fill with dummy data */
    memset(data, 0xFF, *len);
    state->stats.rx_count += (uint32_t)*len;

    return NX_OK;
}

/**
 * \brief           Transfer data synchronously (full duplex)
 */
nx_status_t nx_spi_tx_rx_sync_transfer(nx_tx_rx_sync_t* self, const uint8_t* tx,
                                       uint8_t* rx, size_t len,
                                       uint32_t timeout_ms) {
    nx_spi_impl_t* impl =
        (nx_spi_impl_t*)((char*)self - offsetof(nx_spi_impl_t, tx_rx_sync));
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_spi_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    (void)timeout_ms;

    /* Simulate transfer - echo TX data to RX */
    for (size_t i = 0; i < len; i++) {
        if (rx) {
            rx[i] = tx ? tx[i] : 0xFF;
        }
    }

    state->stats.tx_count += (uint32_t)len;
    state->stats.rx_count += (uint32_t)len;

    return NX_OK;
}
