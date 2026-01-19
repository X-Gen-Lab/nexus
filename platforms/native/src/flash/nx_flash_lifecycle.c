/**
 * \file            nx_flash_lifecycle.c
 * \brief           Flash lifecycle implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements lifecycle management (init, deinit, suspend,
 *                  resume, get_state) for Flash peripheral. Loads flash
 *                  contents from file on init if persistence is enabled.
 */

#include "nx_flash_helpers.h"
#include "nx_flash_types.h"

/*---------------------------------------------------------------------------*/
/* Lifecycle Implementation                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize Flash device
 */
static nx_status_t flash_lifecycle_init(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_flash_impl_t* impl =
        (nx_flash_impl_t*)((uint8_t*)self -
                           offsetof(nx_flash_impl_t, lifecycle));
    nx_flash_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    /* Load from file if exists */
    nx_status_t status = flash_load_from_file(state);
    if (status != NX_OK && status != NX_ERR_IO) {
        return status;
    }

    /* Mark as initialized and running */
    state->initialized = true;
    state->suspended = false;
    state->locked = true;

    return NX_OK;
}

/**
 * \brief           Deinitialize Flash device
 */
static nx_status_t flash_lifecycle_deinit(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_flash_impl_t* impl =
        (nx_flash_impl_t*)((uint8_t*)self -
                           offsetof(nx_flash_impl_t, lifecycle));
    nx_flash_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Save to file before deinit */
    flash_save_to_file(state);

    /* Mark as uninitialized */
    state->initialized = false;
    state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Suspend Flash device
 */
static nx_status_t flash_lifecycle_suspend(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_flash_impl_t* impl =
        (nx_flash_impl_t*)((uint8_t*)self -
                           offsetof(nx_flash_impl_t, lifecycle));
    nx_flash_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Save to file before suspend */
    flash_save_to_file(state);

    /* Mark as suspended */
    state->suspended = true;

    return NX_OK;
}

/**
 * \brief           Resume Flash device
 */
static nx_status_t flash_lifecycle_resume(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_flash_impl_t* impl =
        (nx_flash_impl_t*)((uint8_t*)self -
                           offsetof(nx_flash_impl_t, lifecycle));
    nx_flash_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (!state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Mark as running */
    state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Get Flash device state
 */
static nx_device_state_t flash_lifecycle_get_state(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_DEV_STATE_ERROR;
    }

    nx_flash_impl_t* impl =
        (nx_flash_impl_t*)((uint8_t*)self -
                           offsetof(nx_flash_impl_t, lifecycle));
    nx_flash_state_t* state = impl->state;

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
void flash_init_lifecycle(nx_lifecycle_t* lifecycle) {
    if (lifecycle == NULL) {
        return;
    }

    lifecycle->init = flash_lifecycle_init;
    lifecycle->deinit = flash_lifecycle_deinit;
    lifecycle->suspend = flash_lifecycle_suspend;
    lifecycle->resume = flash_lifecycle_resume;
    lifecycle->get_state = flash_lifecycle_get_state;
}
