/**
 * \file            nx_timer_lifecycle.c
 * \brief           Timer lifecycle interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements Timer lifecycle operations including init,
 *                  deinit, suspend, resume, and state query functions.
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "nx_timer_helpers.h"
#include "nx_timer_types.h"

/*---------------------------------------------------------------------------*/
/* Lifecycle Interface Implementation                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize implementation
 */
static nx_status_t timer_lifecycle_init(nx_lifecycle_t* self) {
    nx_timer_impl_t* impl = NX_CONTAINER_OF(self, nx_timer_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state) {
        return NX_ERR_NULL_PTR;
    }

    /* If already initialized, return success (idempotent) */
    if (impl->state->initialized) {
        return NX_OK;
    }

    /* Set state flags */
    impl->state->initialized = true;
    impl->state->suspended = false;
    impl->state->running = false;
    impl->state->counter = 0;

    return NX_OK;
}

/**
 * \brief           Deinitialize implementation
 */
static nx_status_t timer_lifecycle_deinit(nx_lifecycle_t* self) {
    nx_timer_impl_t* impl = NX_CONTAINER_OF(self, nx_timer_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Stop timer if running */
    impl->state->running = false;

    /* Clear state flag */
    impl->state->initialized = false;

    return NX_OK;
}

/**
 * \brief           Suspend implementation
 */
static nx_status_t timer_lifecycle_suspend(nx_lifecycle_t* self) {
    nx_timer_impl_t* impl = NX_CONTAINER_OF(self, nx_timer_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Check if already suspended */
    if (impl->state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Set suspend flag */
    impl->state->suspended = true;

    return NX_OK;
}

/**
 * \brief           Resume implementation
 */
static nx_status_t timer_lifecycle_resume(nx_lifecycle_t* self) {
    nx_timer_impl_t* impl = NX_CONTAINER_OF(self, nx_timer_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Check if not suspended */
    if (!impl->state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Clear suspend flag */
    impl->state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Get state implementation
 */
static nx_device_state_t timer_lifecycle_get_state(nx_lifecycle_t* self) {
    nx_timer_impl_t* impl = NX_CONTAINER_OF(self, nx_timer_impl_t, lifecycle);

    /* Parameter validation */
    if (!impl->state) {
        return NX_DEV_STATE_ERROR;
    }
    if (!impl->state->initialized) {
        return NX_DEV_STATE_UNINITIALIZED;
    }
    if (impl->state->suspended) {
        return NX_DEV_STATE_SUSPENDED;
    }

    return NX_DEV_STATE_RUNNING;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize lifecycle interface
 */
void timer_init_lifecycle(nx_lifecycle_t* lifecycle) {
    lifecycle->init = timer_lifecycle_init;
    lifecycle->deinit = timer_lifecycle_deinit;
    lifecycle->suspend = timer_lifecycle_suspend;
    lifecycle->resume = timer_lifecycle_resume;
    lifecycle->get_state = timer_lifecycle_get_state;
}
