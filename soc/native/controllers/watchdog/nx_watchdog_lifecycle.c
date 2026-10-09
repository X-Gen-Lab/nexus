/**
 * \file            nx_watchdog_lifecycle.c
 * \brief           Watchdog lifecycle implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements lifecycle management (init, deinit, suspend,
 *                  resume, get_state) for Watchdog peripheral.
 */

#include "nx_watchdog_helpers.h"
#include "nx_watchdog_types.h"

/*---------------------------------------------------------------------------*/
/* Lifecycle Implementation                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize Watchdog device
 */
static nx_status_t watchdog_lifecycle_init(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_watchdog_impl_t* impl =
        (nx_watchdog_impl_t*)((uint8_t*)self -
                              offsetof(nx_watchdog_impl_t, lifecycle));
    nx_watchdog_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    /* If already initialized, return success (idempotent) */
    if (state->initialized) {
        return NX_OK;
    }

    /* Initialize watchdog state */
    state->running = false;
    state->last_feed_time_ms = 0;
    state->callback = NULL;
    state->user_data = NULL;

    /* Clear statistics */
    state->stats.start_count = 0;
    state->stats.stop_count = 0;
    state->stats.feed_count = 0;
    state->stats.timeout_count = 0;

    /* Mark as initialized and running */
    state->initialized = true;
    state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Deinitialize Watchdog device
 */
static nx_status_t watchdog_lifecycle_deinit(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_watchdog_impl_t* impl =
        (nx_watchdog_impl_t*)((uint8_t*)self -
                              offsetof(nx_watchdog_impl_t, lifecycle));
    nx_watchdog_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Stop watchdog if running */
    state->running = false;

    /* Clear callback */
    state->callback = NULL;
    state->user_data = NULL;

    /* Mark as uninitialized */
    state->initialized = false;
    state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Suspend Watchdog device
 */
static nx_status_t watchdog_lifecycle_suspend(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_watchdog_impl_t* impl =
        (nx_watchdog_impl_t*)((uint8_t*)self -
                              offsetof(nx_watchdog_impl_t, lifecycle));
    nx_watchdog_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Mark as suspended (preserve watchdog state) */
    state->suspended = true;

    return NX_OK;
}

/**
 * \brief           Resume Watchdog device
 */
static nx_status_t watchdog_lifecycle_resume(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_watchdog_impl_t* impl =
        (nx_watchdog_impl_t*)((uint8_t*)self -
                              offsetof(nx_watchdog_impl_t, lifecycle));
    nx_watchdog_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (!state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Mark as running (restore watchdog state) */
    state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Get Watchdog device state
 */
static nx_device_state_t watchdog_lifecycle_get_state(nx_lifecycle_t* self) {
    if (self == NULL) {
        return NX_DEV_STATE_ERROR;
    }

    nx_watchdog_impl_t* impl =
        (nx_watchdog_impl_t*)((uint8_t*)self -
                              offsetof(nx_watchdog_impl_t, lifecycle));
    nx_watchdog_state_t* state = impl->state;

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
void nx_watchdog_lifecycle_init(nx_lifecycle_t* lifecycle) {
    if (lifecycle == NULL) {
        return;
    }

    lifecycle->init = watchdog_lifecycle_init;
    lifecycle->deinit = watchdog_lifecycle_deinit;
    lifecycle->suspend = watchdog_lifecycle_suspend;
    lifecycle->resume = watchdog_lifecycle_resume;
    lifecycle->get_state = watchdog_lifecycle_get_state;
}
