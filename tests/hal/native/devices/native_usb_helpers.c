/**
 * \file            native_usb_helpers.c
 * \brief           Native USB test helpers implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "native_usb_helpers.h"
#include "../../../../platforms/native/src/usb/nx_usb_helpers.h"
#include "../../../../platforms/native/src/usb/nx_usb_types.h"
#include "hal/nx_factory.h"
#include <string.h>

static nx_usb_impl_t* get_usb_impl(uint8_t index) {
    nx_usb_t* usb = nx_factory_usb(index);
    return usb ? (nx_usb_impl_t*)usb : NULL;
}

nx_status_t native_usb_get_state(uint8_t index, bool* initialized,
                                 bool* suspended) {
    nx_usb_impl_t* impl = get_usb_impl(index);
    if (!impl || !impl->state)
        return NX_ERR_INVALID_PARAM;
    if (initialized)
        *initialized = impl->state->initialized;
    if (suspended)
        *suspended = impl->state->suspended;
    return NX_OK;
}

nx_status_t native_usb_inject_rx(uint8_t index, const uint8_t* data,
                                 size_t len) {
    nx_usb_impl_t* impl = get_usb_impl(index);
    if (!impl || !impl->state || !data)
        return NX_ERR_INVALID_PARAM;

    /* Check if there's enough space in the RX buffer */
    nx_usb_buffer_t* rx_buf = &impl->state->rx_buf;
    if (rx_buf->count + len > rx_buf->size)
        return NX_ERR_NO_MEMORY;

    /* Copy data into the RX buffer */
    for (size_t i = 0; i < len; i++) {
        rx_buf->data[rx_buf->head] = data[i];
        rx_buf->head = (rx_buf->head + 1) % rx_buf->size;
        rx_buf->count++;
    }

    return NX_OK;
}

nx_status_t native_usb_simulate_connect(uint8_t index) {
    nx_usb_impl_t* impl = get_usb_impl(index);
    if (!impl || !impl->state)
        return NX_ERR_INVALID_PARAM;
    impl->state->connected = true;
    return NX_OK;
}

nx_status_t native_usb_simulate_disconnect(uint8_t index) {
    nx_usb_impl_t* impl = get_usb_impl(index);
    if (!impl || !impl->state)
        return NX_ERR_INVALID_PARAM;
    impl->state->connected = false;
    /* Reset buffers */
    impl->state->rx_buf.count = 0;
    impl->state->rx_buf.head = 0;
    impl->state->rx_buf.tail = 0;
    impl->state->tx_buf.count = 0;
    impl->state->tx_buf.head = 0;
    impl->state->tx_buf.tail = 0;
    return NX_OK;
}

nx_status_t native_usb_simulate_suspend(uint8_t index) {
    nx_usb_impl_t* impl = get_usb_impl(index);
    if (!impl || !impl->state)
        return NX_ERR_INVALID_PARAM;
    impl->state->suspended = true;
    return NX_OK;
}

nx_status_t native_usb_simulate_resume(uint8_t index) {
    nx_usb_impl_t* impl = get_usb_impl(index);
    if (!impl || !impl->state)
        return NX_ERR_INVALID_PARAM;
    impl->state->suspended = false;
    return NX_OK;
}

void native_usb_reset_all(void) {
    for (uint8_t i = 0; i < 4; i++) {
        nx_usb_impl_t* impl = get_usb_impl(i);
        if (!impl || !impl->state)
            continue;

        /* Save config, index, and buffer info before reset */
        uint8_t index = impl->state->index;
        nx_usb_config_t config = impl->state->config;
        uint8_t* tx_data = impl->state->tx_buf.data;
        size_t tx_size = impl->state->tx_buf.size;
        uint8_t* rx_data = impl->state->rx_buf.data;
        size_t rx_size = impl->state->rx_buf.size;

        /* Reset state */
        memset(impl->state, 0, sizeof(*impl->state));

        /* Restore config, index, and buffer info */
        impl->state->index = index;
        impl->state->config = config;
        impl->state->tx_buf.data = tx_data;
        impl->state->tx_buf.size = tx_size;
        impl->state->rx_buf.data = rx_data;
        impl->state->rx_buf.size = rx_size;

        /* Clear buffer contents */
        if (tx_data) {
            memset(tx_data, 0, tx_size);
        }
        if (rx_data) {
            memset(rx_data, 0, rx_size);
        }

        /* Reset power context */
        usb_reset_power_context(i);
    }
}
