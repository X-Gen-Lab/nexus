/**
 * \file            nx_option_bytes_lifecycle.c
 * \brief           Option Bytes lifecycle implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements lifecycle management (init, deinit, suspend,
 *                  resume, get_state) for Option Bytes peripheral.
 */

#include "nx_option_bytes_helpers.h"
#include "nx_option_bytes_types.h"

/*---------------------------------------------------------------------------*/
/* Lifecycle Implementation                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize Option Bytes device
 */
static nx_status_t option_bytes_lifecycle_init(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_option_bytes_impl_t* impl =
        (nx_option_bytes_impl_t*)((uint8_t*)self -
                                  offsetof(nx_option_bytes_impl_t, lifecycle));
    nx_option_bytes_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    /* Initialize option bytes data with defaults */
    state->data.read_protection = 0; /* No protection by default */
    state->data.write_protected = false;
    state->data.pending_changes = false;

    /* Clear user data */
    for (size_t i = 0; i < NX_OPTION_BYTES_USER_DATA_SIZE; i++) {
        state->data.user_data[i] = 0xFF; /* Erased state */
    }

    /* Initialize pending buffer */
    state->pending.read_protection = 0;
    state->pending.write_protected = false;
    state->pending.pending_changes = false;
    for (size_t i = 0; i < NX_OPTION_BYTES_USER_DATA_SIZE; i++) {
        state->pending.user_data[i] = 0xFF;
    }

    /* Mark as initialized and running */
    state->initialized = true;
    state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Deinitialize Option Bytes device
 */
static nx_status_t option_bytes_lifecycle_deinit(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_option_bytes_impl_t* impl =
        (nx_option_bytes_impl_t*)((uint8_t*)self -
                                  offsetof(nx_option_bytes_impl_t, lifecycle));
    nx_option_bytes_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Mark as uninitialized */
    state->initialized = false;
    state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Suspend Option Bytes device
 */
static nx_status_t option_bytes_lifecycle_suspend(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_option_bytes_impl_t* impl =
        (nx_option_bytes_impl_t*)((uint8_t*)self -
                                  offsetof(nx_option_bytes_impl_t, lifecycle));
    nx_option_bytes_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Mark as suspended (preserve option bytes state) */
    state->suspended = true;

    return NX_OK;
}

/**
 * \brief           Resume Option Bytes device
 */
static nx_status_t option_bytes_lifecycle_resume(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_option_bytes_impl_t* impl =
        (nx_option_bytes_impl_t*)((uint8_t*)self -
                                  offsetof(nx_option_bytes_impl_t, lifecycle));
    nx_option_bytes_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (!state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Mark as running (restore option bytes state) */
    state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Get Option Bytes device state
 */
static nx_device_state_t
option_bytes_lifecycle_get_state(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_DEV_STATE_ERROR;
    }

    nx_option_bytes_impl_t* impl =
        (nx_option_bytes_impl_t*)((uint8_t*)self -
                                  offsetof(nx_option_bytes_impl_t, lifecycle));
    nx_option_bytes_state_t* state = impl->state;

    if (state == NULL) {
        return NX_DEV_STATE_ERROR;
    }

    if (!state->initialized) {
        return NX_DEV_STATE_UNINITIALIZED;
    }

    if (state->suspended) {
        return NX_DEV_STATE_SUSPENDED;
    }

    return NX_DEV_STATE_RUNNING;
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Interface Initialization                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize lifecycle interface
 */
void option_bytes_init_lifecycle(nx_lifecycle_t* lifecycle) {
    if (lifecycle == NULL) {
        return;
    }

    lifecycle->init = option_bytes_lifecycle_init;
    lifecycle->deinit = option_bytes_lifecycle_deinit;
    lifecycle->suspend = option_bytes_lifecycle_suspend;
    lifecycle->resume = option_bytes_lifecycle_resume;
    lifecycle->get_state = option_bytes_lifecycle_get_state;
}
