/**
 * \file            native_watchdog_helpers.c
 * \brief           Native Watchdog test helpers implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "native_watchdog_helpers.h"
#include "hal/nx_factory.h"

/* Include platform-specific types */
#include "../../../../platforms/native/src/watchdog/nx_watchdog_types.h"

#include <string.h>

/* External functions for time simulation */
extern uint64_t nx_get_time_ms(void);
extern void nx_advance_time_ms(uint32_t ms);
extern void nx_reset_time(void);

/*---------------------------------------------------------------------------*/
/* Internal Helper - Get Watchdog Implementation                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Watchdog implementation structure from device
 */
static nx_watchdog_impl_t* get_watchdog_impl(uint8_t index) {
    nx_watchdog_t* wdt = nx_factory_watchdog(index);
    if (wdt == NULL) {
        return NULL;
    }

    /* The impl structure contains the base as first member */
    return (nx_watchdog_impl_t*)wdt;
}

/*---------------------------------------------------------------------------*/
/* Test Helper Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Watchdog device state
 */
nx_status_t native_watchdog_get_state(uint8_t index, bool* initialized,
                                      bool* suspended) {
    nx_watchdog_impl_t* impl = get_watchdog_impl(index);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    if (initialized != NULL) {
        *initialized = impl->state->initialized;
    }

    if (suspended != NULL) {
        *suspended = impl->state->suspended;
    }

    return NX_OK;
}

/**
 * \brief           Check if watchdog has timed out
 */
bool native_watchdog_has_timed_out(uint8_t index) {
    nx_watchdog_impl_t* impl = get_watchdog_impl(index);
    if (impl == NULL || impl->state == NULL) {
        return false;
    }

    /* Check if watchdog is running */
    if (!impl->state->running) {
        return false;
    }

    /* Get current time (simulated) */

    uint64_t current_time = nx_get_time_ms();

    /* Check if timeout has occurred */
    uint64_t elapsed = current_time - impl->state->last_feed_time_ms;
    return elapsed >= impl->state->config.timeout_ms;
}

/**
 * \brief           Simulate time passage for testing
 */
nx_status_t native_watchdog_advance_time(uint8_t index, uint32_t milliseconds) {
    nx_watchdog_impl_t* impl = get_watchdog_impl(index);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Check if timeout will occur during this time advance */
    bool was_timed_out = false;
    if (impl->state->running) {
        uint64_t current_time = nx_get_time_ms();
        uint64_t elapsed_before = current_time - impl->state->last_feed_time_ms;
        was_timed_out = (elapsed_before >= impl->state->config.timeout_ms);
    }

    /* Advance simulated time */
    nx_advance_time_ms(milliseconds);

    /* Check if timeout occurred and invoke callback if needed */
    if (impl->state->running && impl->state->callback != NULL) {
        uint64_t current_time = nx_get_time_ms();
        uint64_t elapsed_after = current_time - impl->state->last_feed_time_ms;
        bool is_timed_out = (elapsed_after >= impl->state->config.timeout_ms);

        /* Invoke callback only once when transitioning from not-timed-out to
         * timed-out */
        if (is_timed_out && !was_timed_out) {
            impl->state->callback(impl->state->user_data);
            impl->state->stats.timeout_count++;
        }
    }

    return NX_OK;
}

/**
 * \brief           Reset all Watchdog instances to initial state
 */
void native_watchdog_reset_all(void) {
    /* Iterate through all possible Watchdog instances */
    for (uint8_t i = 0; i < 4; i++) {
        nx_watchdog_impl_t* impl = get_watchdog_impl(i);
        if (impl == NULL || impl->state == NULL) {
            continue;
        }

        /* Reset state */
        impl->state->initialized = false;
        impl->state->suspended = false;
        impl->state->running = false;
        impl->state->last_feed_time_ms = 0;
        impl->state->callback = NULL;
        impl->state->user_data = NULL;

        /* Clear statistics */
        memset(&impl->state->stats, 0, sizeof(impl->state->stats));
    }

    /* Reset simulated time */

    nx_reset_time();
}
