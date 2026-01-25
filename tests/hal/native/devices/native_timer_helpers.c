/**
 * \file            native_timer_helpers.c
 * \brief           Native Timer test helpers implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "native_timer_helpers.h"
#include "hal/nx_factory.h"

/* Include platform-specific types */
#include "../../../../platforms/native/src/timer/nx_timer_types.h"

#include <string.h>

/*---------------------------------------------------------------------------*/
/* Constants                                                                 */
/*---------------------------------------------------------------------------*/

#define NX_TIMER_MAX_INSTANCES 8

/*---------------------------------------------------------------------------*/
/* Internal Helper                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Timer implementation structure
 * \details         Retrieves the implementation structure from the factory.
 */
static nx_timer_impl_t* get_timer_impl(uint8_t instance) {
    /* Validate parameters */
    if (instance >= NX_TIMER_MAX_INSTANCES) {
        return NULL;
    }

    /* Get Timer instance from factory */
    nx_timer_base_t* timer = nx_factory_timer(instance);
    if (timer == NULL) {
        return NULL;
    }

    return (nx_timer_impl_t*)timer;
}

/*---------------------------------------------------------------------------*/
/* Test Helper Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Timer device state
 */
nx_status_t native_timer_get_state(uint8_t instance,
                                   native_timer_state_t* state) {
    /* Validate parameters */
    if (state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Get implementation */
    nx_timer_impl_t* impl = get_timer_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Copy state information */
    state->initialized = impl->state->initialized;
    state->suspended = impl->state->suspended;
    state->running = impl->state->running;
    state->frequency = impl->state->config.frequency;
    state->prescaler = impl->state->config.prescaler;
    state->period = impl->state->config.period;
    state->counter = impl->state->counter;
    state->channel_count = impl->state->config.channel_count;
    state->overflow_count = 0; /* Not tracked in current implementation */

    return NX_OK;
}

/**
 * \brief           Advance timer time
 * \details         Simulates time passage by advancing the timer counter.
 *                  If the counter reaches or exceeds the period, it overflows
 *                  and triggers the callback if configured.
 */
nx_status_t native_timer_advance_time(uint8_t instance, uint32_t ticks) {
    /* Get implementation */
    nx_timer_impl_t* impl = get_timer_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Only advance if timer is running */
    if (!impl->state->running) {
        return NX_OK;
    }

    /* Advance counter */
    impl->state->counter += ticks;

    /* Check for overflow */
    if (impl->state->config.period > 0) {
        while (impl->state->counter >= impl->state->config.period) {
            /* Overflow occurred */
            impl->state->counter -= impl->state->config.period;

            /* Trigger callback if configured */
            if (impl->state->callback != NULL) {
                impl->state->callback(impl->state->user_data);
            }
        }
    }

    return NX_OK;
}

/**
 * \brief           Reset specific Timer instance
 * \details         Resets the Timer instance to its initial state, clearing
 *                  all configuration, state, and statistics.
 */
nx_status_t native_timer_reset(uint8_t instance) {
    /* Get implementation */
    nx_timer_impl_t* impl = get_timer_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Reset state */
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->running = false;
    impl->state->counter = 0;
    impl->state->config.prescaler = 0;
    impl->state->config.period = 0;

    /* Reset callback */
    impl->state->callback = NULL;
    impl->state->user_data = NULL;

    return NX_OK;
}

/**
 * \brief           Reset all Timer instances
 * \details         Iterates through all possible Timer instances and resets
 *                  each one to its initial state.
 */
void native_timer_reset_all(void) {
    for (uint8_t instance = 0; instance < NX_TIMER_MAX_INSTANCES; instance++) {
        native_timer_reset(instance);
    }
}
