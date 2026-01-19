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
        return NX_ERR_INVALID_ARG;
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
        return NX_ERR_INVALID_ARG;
    if (len > sizeof(impl->state->rx_buffer) - impl->state->rx_count)
        return NX_ERR_NO_MEM;
    memcpy(&impl->state->rx_buffer[impl->state->rx_count], data, len);
    impl->state->rx_count += len;
    return NX_OK;
}

nx_status_t native_usb_simulate_connect(uint8_t index) {
    nx_usb_impl_t* impl = get_usb_impl(index);
    if (!impl || !impl->state)
        return NX_ERR_INVALID_ARG;
    impl->state->connected = true;
    return NX_OK;
}

nx_status_t native_usb_simulate_disconnect(uint8_t index) {
    nx_usb_impl_t* impl = get_usb_impl(index);
    if (!impl || !impl->state)
        return NX_ERR_INVALID_ARG;
    impl->state->connected = false;
    impl->state->rx_count = 0;
    impl->state->tx_count = 0;
    return NX_OK;
}

nx_status_t native_usb_simulate_suspend(uint8_t index) {
    nx_usb_impl_t* impl = get_usb_impl(index);
    if (!impl || !impl->state)
        return NX_ERR_INVALID_ARG;
    impl->state->suspended = true;
    return NX_OK;
}

nx_status_t native_usb_simulate_resume(uint8_t index) {
    nx_usb_impl_t* impl = get_usb_impl(index);
    if (!impl || !impl->state)
        return NX_ERR_INVALID_ARG;
    impl->state->suspended = false;
    return NX_OK;
}

void native_usb_reset_all(void) {
    for (uint8_t i = 0; i < 4; i++) {
        nx_usb_impl_t* impl = get_usb_impl(i);
        if (!impl || !impl->state)
            continue;
        memset(impl->state, 0, sizeof(*impl->state));
        impl->state->index = i;
    }
}
