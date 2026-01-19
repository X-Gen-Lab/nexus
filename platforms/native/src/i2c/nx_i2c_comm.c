/**
 * \file            nx_i2c_comm.c
 * \brief           I2C communication interface implementations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements I2C communication operations shared between
 *                  sync and async interfaces.
 */

#include "hal/nx_status.h"
#include "nx_i2c_helpers.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* TX Async Interface                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Send data asynchronously (I2C master transmit)
 */
nx_status_t nx_i2c_tx_async_send(nx_tx_async_t* self, const uint8_t* data,
                                 size_t len) {
    nx_i2c_impl_t* impl =
        (nx_i2c_impl_t*)((char*)self - offsetof(nx_i2c_impl_t, tx_async));
    if (!impl || !impl->state || !data) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Simulate transmission */
    state->stats.tx_count += (uint32_t)len;
    printf("[I2C%d] TX to 0x%02X: %zu bytes\n", state->index,
           state->current_dev_addr, len);

    return NX_OK;
}

/**
 * \brief           Get TX async state
 */
nx_status_t nx_i2c_tx_async_get_state(nx_tx_async_t* self) {
    nx_i2c_impl_t* impl =
        (nx_i2c_impl_t*)((char*)self - offsetof(nx_i2c_impl_t, tx_async));
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
nx_status_t nx_i2c_tx_rx_async_send(nx_tx_rx_async_t* self, const uint8_t* data,
                                    size_t len) {
    nx_i2c_impl_t* impl =
        (nx_i2c_impl_t*)((char*)self - offsetof(nx_i2c_impl_t, tx_rx_async));
    if (!impl || !impl->state || !data) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    state->stats.tx_count += (uint32_t)len;
    printf("[I2C%d] TX/RX to 0x%02X: %zu bytes\n", state->index,
           state->current_dev_addr, len);

    return NX_OK;
}

/**
 * \brief           Receive data asynchronously
 */
nx_status_t nx_i2c_tx_rx_async_receive(nx_tx_rx_async_t* self, uint8_t* data,
                                       size_t* len) {
    nx_i2c_impl_t* impl =
        (nx_i2c_impl_t*)((char*)self - offsetof(nx_i2c_impl_t, tx_rx_async));
    if (!impl || !impl->state || !data || !len) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Simulate: fill with dummy data */
    for (size_t i = 0; i < *len; i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }
    state->stats.rx_count += (uint32_t)*len;

    return NX_OK;
}

/**
 * \brief           Get TX/RX async state
 */
nx_status_t nx_i2c_tx_rx_async_get_state(nx_tx_rx_async_t* self) {
    nx_i2c_impl_t* impl =
        (nx_i2c_impl_t*)((char*)self - offsetof(nx_i2c_impl_t, tx_rx_async));
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    return impl->state->initialized ? NX_OK : NX_ERR_NOT_INIT;
}

/*---------------------------------------------------------------------------*/
/* TX Sync Interface                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Send data synchronously (I2C master transmit)
 */
nx_status_t nx_i2c_tx_sync_send(nx_tx_sync_t* self, const uint8_t* data,
                                size_t len, uint32_t timeout_ms) {
    nx_i2c_impl_t* impl =
        (nx_i2c_impl_t*)((char*)self - offsetof(nx_i2c_impl_t, tx_sync));
    if (!impl || !impl->state || !data) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    (void)timeout_ms;
    state->stats.tx_count += (uint32_t)len;
    printf("[I2C%d] TX Sync to 0x%02X: %zu bytes\n", state->index,
           state->current_dev_addr, len);

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* TX/RX Sync Interface                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Send data synchronously (TX/RX)
 */
nx_status_t nx_i2c_tx_rx_sync_send(nx_tx_rx_sync_t* self, const uint8_t* data,
                                   size_t len, uint32_t timeout_ms) {
    nx_i2c_impl_t* impl =
        (nx_i2c_impl_t*)((char*)self - offsetof(nx_i2c_impl_t, tx_rx_sync));
    if (!impl || !impl->state || !data) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    (void)timeout_ms;
    state->stats.tx_count += (uint32_t)len;

    return NX_OK;
}

/**
 * \brief           Receive data synchronously (I2C master receive)
 */
nx_status_t nx_i2c_tx_rx_sync_receive(nx_tx_rx_sync_t* self, uint8_t* data,
                                      size_t* len, uint32_t timeout_ms) {
    nx_i2c_impl_t* impl =
        (nx_i2c_impl_t*)((char*)self - offsetof(nx_i2c_impl_t, tx_rx_sync));
    if (!impl || !impl->state || !data || !len) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    (void)timeout_ms;
    /* Simulate: fill with dummy data */
    for (size_t i = 0; i < *len; i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }
    state->stats.rx_count += (uint32_t)*len;
    printf("[I2C%d] RX Sync from 0x%02X: %zu bytes\n", state->index,
           state->current_dev_addr, *len);

    return NX_OK;
}

/**
 * \brief           Transfer data synchronously (combined TX/RX)
 */
nx_status_t nx_i2c_tx_rx_sync_transfer(nx_tx_rx_sync_t* self, const uint8_t* tx,
                                       uint8_t* rx, size_t len,
                                       uint32_t timeout_ms) {
    nx_i2c_impl_t* impl =
        (nx_i2c_impl_t*)((char*)self - offsetof(nx_i2c_impl_t, tx_rx_sync));
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_i2c_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    (void)timeout_ms;

    /* Simulate transfer */
    if (tx) {
        state->stats.tx_count += (uint32_t)len;
    }
    if (rx) {
        for (size_t i = 0; i < len; i++) {
            rx[i] = tx ? tx[i] : (uint8_t)(i & 0xFF);
        }
        state->stats.rx_count += (uint32_t)len;
    }

    return NX_OK;
}
