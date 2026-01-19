/**
 * \file            nx_watchdog_interface.c
 * \brief           Watchdog interface implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements the watchdog interface functions including
 *                  start, stop, feed, timeout configuration, and callback
 *                  management for the Native platform.
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#include "hal/interface/nx_watchdog.h"
#include "hal/nx_status.h"
#include "nx_watchdog_helpers.h"
#include "nx_watchdog_types.h"
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/* Watchdog Interface Implementation                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Start watchdog timer
 */
static nx_status_t watchdog_start(nx_watchdog_t* self) {
    nx_watchdog_impl_t* impl = watchdog_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_watchdog_state_t* state = impl->state;

    /* Check if initialized */
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Check if already running */
    if (state->running) {
        return NX_ERR_BUSY;
    }

    /* Start watchdog */
    state->running = true;
    state->last_feed_time_ms = watchdog_get_system_time_ms();
    state->stats.start_count++;

    return NX_OK;
}

/**
 * \brief           Stop watchdog timer
 */
static nx_status_t watchdog_stop(nx_watchdog_t* self) {
    nx_watchdog_impl_t* impl = watchdog_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_watchdog_state_t* state = impl->state;

    /* Check if initialized */
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Check if running */
    if (!state->running) {
        return NX_ERR_INVALID_STATE;
    }

    /* Stop watchdog */
    state->running = false;
    state->stats.stop_count++;

    return NX_OK;
}

/**
 * \brief           Feed (refresh) watchdog timer
 */
static void watchdog_feed(nx_watchdog_t* self) {
    nx_watchdog_impl_t* impl = watchdog_get_impl(self);
    if (!impl || !impl->state) {
        return;
    }

    nx_watchdog_state_t* state = impl->state;

    /* Only feed if running */
    if (!state->running) {
        return;
    }

    /* Reset the timer */
    state->last_feed_time_ms = watchdog_get_system_time_ms();
    state->stats.feed_count++;
}

/**
 * \brief           Get watchdog timeout value
 */
static uint32_t watchdog_get_timeout(nx_watchdog_t* self) {
    nx_watchdog_impl_t* impl = watchdog_get_impl(self);
    if (!impl || !impl->state) {
        return 0;
    }

    return impl->state->config.timeout_ms;
}

/**
 * \brief           Set early warning callback
 */
static nx_status_t watchdog_set_callback(nx_watchdog_t* self,
                                         nx_watchdog_callback_t callback,
                                         void* user_data) {
    nx_watchdog_impl_t* impl = watchdog_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    nx_watchdog_state_t* state = impl->state;

    /* Check if initialized */
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Set callback */
    state->callback = callback;
    state->user_data = user_data;

    return NX_OK;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* watchdog_get_lifecycle(nx_watchdog_t* self) {
    nx_watchdog_impl_t* impl = watchdog_get_impl(self);
    if (!impl) {
        return NULL;
    }

    return &impl->lifecycle;
}

/*---------------------------------------------------------------------------*/
/* Watchdog Interface Initialization                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize watchdog interface
 */
void nx_watchdog_interface_init(nx_watchdog_impl_t* impl) {
    if (!impl) {
        return;
    }

    NX_INIT_WATCHDOG(&impl->base, watchdog_start, watchdog_stop, watchdog_feed,
                     watchdog_get_timeout, watchdog_set_callback,
                     watchdog_get_lifecycle);
}
