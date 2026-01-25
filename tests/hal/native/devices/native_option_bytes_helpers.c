/**
 * \file            native_option_bytes_helpers.c
 * \brief           Native Option Bytes test helpers implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "native_option_bytes_helpers.h"
#include "../../../../platforms/native/src/option_bytes/nx_option_bytes_types.h"
#include "hal/nx_factory.h"
#include <string.h>

static nx_option_bytes_impl_t* get_option_bytes_impl(uint8_t index) {
    nx_option_bytes_t* opt = nx_factory_option_bytes(index);
    return opt ? (nx_option_bytes_impl_t*)opt : NULL;
}

nx_status_t native_option_bytes_get_state(uint8_t index, bool* initialized,
                                          bool* suspended) {
    nx_option_bytes_impl_t* impl = get_option_bytes_impl(index);
    if (!impl || !impl->state)
        return NX_ERR_INVALID_PARAM;
    if (initialized)
        *initialized = impl->state->initialized;
    if (suspended)
        *suspended = impl->state->suspended;
    return NX_OK;
}

nx_status_t native_option_bytes_set_write_protection(uint8_t index,
                                                     bool enabled) {
    nx_option_bytes_impl_t* impl = get_option_bytes_impl(index);
    if (!impl || !impl->state)
        return NX_ERR_INVALID_PARAM;
    impl->state->data.write_protected = enabled;
    return NX_OK;
}

nx_status_t native_option_bytes_has_pending_changes(uint8_t index,
                                                    bool* has_pending) {
    nx_option_bytes_impl_t* impl = get_option_bytes_impl(index);
    if (!impl || !impl->state || !has_pending)
        return NX_ERR_INVALID_PARAM;
    *has_pending = impl->state->pending.pending_changes;
    return NX_OK;
}

void native_option_bytes_reset(uint8_t index) {
    nx_option_bytes_impl_t* impl = get_option_bytes_impl(index);
    if (!impl || !impl->state)
        return;
    memset(impl->state, 0, sizeof(*impl->state));
    impl->state->index = index;
}

void native_option_bytes_reset_all(void) {
    for (uint8_t i = 0; i < 4; i++) {
        native_option_bytes_reset(i);
    }
}
