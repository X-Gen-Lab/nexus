/**
 * \file            nx_option_bytes_interface.c
 * \brief           Option Bytes interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements the Option Bytes interface functions for
 *                  reading/writing user data, managing read protection,
 *                  and applying pending changes.
 */

#include "nx_option_bytes_helpers.h"
#include "nx_option_bytes_types.h"

/*---------------------------------------------------------------------------*/
/* Interface Implementation                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get user data bytes
 */
static nx_status_t option_bytes_get_user_data(nx_option_bytes_t* self,
                                              uint8_t* data, size_t len) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_option_bytes_impl_t* impl =
        (nx_option_bytes_impl_t*)((uint8_t*)self -
                                  offsetof(nx_option_bytes_impl_t, base));
    nx_option_bytes_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    return option_bytes_read_user_data(state, data, len);
}

/**
 * \brief           Set user data bytes
 */
static nx_status_t option_bytes_set_user_data(nx_option_bytes_t* self,
                                              const uint8_t* data, size_t len) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_option_bytes_impl_t* impl =
        (nx_option_bytes_impl_t*)((uint8_t*)self -
                                  offsetof(nx_option_bytes_impl_t, base));
    nx_option_bytes_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    return option_bytes_write_user_data(state, data, len);
}

/**
 * \brief           Get read protection level
 */
static uint8_t option_bytes_get_read_protection_impl(nx_option_bytes_t* self) {
    if (self == NULL) {
        return 0;
    }

    nx_option_bytes_impl_t* impl =
        (nx_option_bytes_impl_t*)((uint8_t*)self -
                                  offsetof(nx_option_bytes_impl_t, base));
    nx_option_bytes_state_t* state = impl->state;

    if (state == NULL || !state->initialized) {
        return 0;
    }

    return option_bytes_get_read_protection(state);
}

/**
 * \brief           Set read protection level
 */
static nx_status_t
option_bytes_set_read_protection_impl(nx_option_bytes_t* self, uint8_t level) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_option_bytes_impl_t* impl =
        (nx_option_bytes_impl_t*)((uint8_t*)self -
                                  offsetof(nx_option_bytes_impl_t, base));
    nx_option_bytes_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    return option_bytes_set_read_protection(state, level);
}

/**
 * \brief           Apply pending changes
 */
static nx_status_t option_bytes_apply_impl(nx_option_bytes_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_option_bytes_impl_t* impl =
        (nx_option_bytes_impl_t*)((uint8_t*)self -
                                  offsetof(nx_option_bytes_impl_t, base));
    nx_option_bytes_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    return option_bytes_apply(state);
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize Option Bytes interface
 */
void option_bytes_init_interface(nx_option_bytes_t* option_bytes) {
    if (option_bytes == NULL) {
        return;
    }

    option_bytes->get_user_data = option_bytes_get_user_data;
    option_bytes->set_user_data = option_bytes_set_user_data;
    option_bytes->get_read_protection = option_bytes_get_read_protection_impl;
    option_bytes->set_read_protection = option_bytes_set_read_protection_impl;
    option_bytes->apply = option_bytes_apply_impl;
}
