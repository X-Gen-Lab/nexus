/**
 * \file            nx_usb_interface.c
 * \brief           USB interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements USB communication interfaces (async/sync TX/RX)
 *                  and connection status management with event simulation.
 */

#include "hal/nx_types.h"
#include "nx_usb_helpers.h"
#include "nx_usb_types.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Async TX Interface                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Async send data (non-blocking)
 */
static nx_status_t usb_tx_async_send(nx_tx_async_t* self, const uint8_t* data,
                                     size_t len) {
    if (self == NULL || data == NULL || len == 0) {
        return NX_ERR_NULL_PTR;
    }

    nx_usb_impl_t* impl =
        (nx_usb_impl_t*)((uint8_t*)self - offsetof(nx_usb_impl_t, tx_async));
    nx_usb_state_t* state = impl->state;

    nx_status_t status = usb_validate_state(state);
    if (status != NX_OK) {
        return status;
    }

    if (!state->connected) {
        return NX_ERR_INVALID_STATE;
    }

    if (state->tx_busy) {
        return NX_ERR_BUSY;
    }

    size_t written = buffer_write(&state->tx_buf, data, len);
    if (written < len) {
        return NX_ERR_FULL;
    }

    state->stats.tx_count++;
    state->stats.tx_bytes += (uint32_t)written;

    return NX_OK;
}

/**
 * \brief           Get TX state
 */
static nx_status_t usb_tx_async_get_state(nx_tx_async_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_usb_impl_t* impl =
        (nx_usb_impl_t*)((uint8_t*)self - offsetof(nx_usb_impl_t, tx_async));
    nx_usb_state_t* state = impl->state;

    if (state == NULL || !state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    return state->tx_busy ? NX_ERR_BUSY : NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Async RX Interface                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Async receive data (non-blocking)
 */
static nx_status_t usb_rx_async_receive(nx_rx_async_t* self, uint8_t* data,
                                        size_t* len) {
    if (self == NULL || data == NULL || len == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_usb_impl_t* impl =
        (nx_usb_impl_t*)((uint8_t*)self - offsetof(nx_usb_impl_t, rx_async));
    nx_usb_state_t* state = impl->state;

    nx_status_t status = usb_validate_state(state);
    if (status != NX_OK) {
        return status;
    }

    if (!state->connected) {
        return NX_ERR_INVALID_STATE;
    }

    size_t available = buffer_available(&state->rx_buf);
    if (available == 0) {
        *len = 0;
        return NX_ERR_NO_DATA;
    }

    size_t to_read = (*len < available) ? *len : available;
    size_t read = buffer_read(&state->rx_buf, data, to_read);

    *len = read;
    state->stats.rx_count++;
    state->stats.rx_bytes += (uint32_t)read;

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Sync TX Interface                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Sync send data (blocking)
 */
static nx_status_t usb_tx_sync_send(nx_tx_sync_t* self, const uint8_t* data,
                                    size_t len, uint32_t timeout_ms) {
    if (self == NULL || data == NULL || len == 0) {
        return NX_ERR_NULL_PTR;
    }

    /* For simulation, sync send is same as async send */
    (void)timeout_ms;

    nx_usb_impl_t* impl =
        (nx_usb_impl_t*)((uint8_t*)self - offsetof(nx_usb_impl_t, tx_sync));
    nx_usb_state_t* state = impl->state;

    nx_status_t status = usb_validate_state(state);
    if (status != NX_OK) {
        return status;
    }

    if (!state->connected) {
        return NX_ERR_INVALID_STATE;
    }

    size_t written = buffer_write(&state->tx_buf, data, len);
    if (written < len) {
        return NX_ERR_FULL;
    }

    state->stats.tx_count++;
    state->stats.tx_bytes += (uint32_t)written;

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Sync RX Interface                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Sync receive data (blocking)
 */
static nx_status_t usb_rx_sync_receive(nx_rx_sync_t* self, uint8_t* data,
                                       size_t* len, uint32_t timeout_ms) {
    if (self == NULL || data == NULL || len == NULL) {
        return NX_ERR_NULL_PTR;
    }

    /* For simulation, sync receive is same as async receive */
    (void)timeout_ms;

    nx_usb_impl_t* impl =
        (nx_usb_impl_t*)((uint8_t*)self - offsetof(nx_usb_impl_t, rx_sync));
    nx_usb_state_t* state = impl->state;

    nx_status_t status = usb_validate_state(state);
    if (status != NX_OK) {
        return status;
    }

    if (!state->connected) {
        return NX_ERR_INVALID_STATE;
    }

    size_t available = buffer_available(&state->rx_buf);
    if (available == 0) {
        *len = 0;
        return NX_ERR_TIMEOUT;
    }

    size_t to_read = (*len < available) ? *len : available;
    size_t read = buffer_read(&state->rx_buf, data, to_read);

    *len = read;
    state->stats.rx_count++;
    state->stats.rx_bytes += (uint32_t)read;

    return NX_OK;
}

/**
 * \brief           Sync receive specified length data (blocking)
 */
static nx_status_t usb_rx_sync_receive_all(nx_rx_sync_t* self, uint8_t* data,
                                           size_t* len, uint32_t timeout_ms) {
    if (self == NULL || data == NULL || len == NULL) {
        return NX_ERR_NULL_PTR;
    }

    /* For simulation, receive all is same as receive */
    return usb_rx_sync_receive(self, data, len, timeout_ms);
}

/*---------------------------------------------------------------------------*/
/* Connection Status                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Check USB connection status
 */
static bool usb_is_connected(nx_usb_t* self) {
    if (self == NULL) {
        return false;
    }

    nx_usb_impl_t* impl = usb_get_impl(self);
    nx_usb_state_t* state = usb_get_state(impl);

    if (state == NULL || !state->initialized) {
        return false;
    }

    return state->connected;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize TX async interface
 */
void usb_init_tx_async(nx_tx_async_t* tx_async) {
    NX_ASSERT(tx_async != NULL);
    NX_INIT_TX_ASYNC(tx_async, usb_tx_async_send, usb_tx_async_get_state);
}

/**
 * \brief           Initialize RX async interface
 */
void usb_init_rx_async(nx_rx_async_t* rx_async) {
    NX_ASSERT(rx_async != NULL);
    NX_INIT_RX_ASYNC(rx_async, usb_rx_async_receive);
}

/**
 * \brief           Initialize TX sync interface
 */
void usb_init_tx_sync(nx_tx_sync_t* tx_sync) {
    NX_ASSERT(tx_sync != NULL);
    NX_INIT_TX_SYNC(tx_sync, usb_tx_sync_send);
}

/**
 * \brief           Initialize RX sync interface
 */
void usb_init_rx_sync(nx_rx_sync_t* rx_sync) {
    NX_ASSERT(rx_sync != NULL);
    NX_INIT_RX_SYNC(rx_sync, usb_rx_sync_receive, usb_rx_sync_receive_all);
}

/**
 * \brief           Initialize USB base interface
 */
void usb_init_base(nx_usb_t* base) {
    NX_ASSERT(base != NULL);
    base->is_connected = usb_is_connected;
}

/*---------------------------------------------------------------------------*/
/* Event Simulation Functions                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Simulate USB connect event
 */
nx_status_t usb_simulate_connect(nx_usb_state_t* state) {
    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->connected) {
        return NX_ERR_INVALID_STATE;
    }

    state->connected = true;
    state->stats.connect_count++;

    return NX_OK;
}

/**
 * \brief           Simulate USB disconnect event
 */
nx_status_t usb_simulate_disconnect(nx_usb_state_t* state) {
    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (!state->connected) {
        return NX_ERR_INVALID_STATE;
    }

    state->connected = false;
    state->stats.disconnect_count++;

    /* Clear buffers on disconnect */
    buffer_clear(&state->tx_buf);
    buffer_clear(&state->rx_buf);

    return NX_OK;
}

/**
 * \brief           Simulate USB suspend event
 */
nx_status_t usb_simulate_suspend(nx_usb_state_t* state) {
    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    state->suspended = true;
    state->stats.suspend_count++;

    return NX_OK;
}

/**
 * \brief           Simulate USB resume event
 */
nx_status_t usb_simulate_resume(nx_usb_state_t* state) {
    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (!state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    state->suspended = false;
    state->stats.resume_count++;

    return NX_OK;
}
