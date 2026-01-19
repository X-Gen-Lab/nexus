/**
 * \file            nx_timer_device.c
 * \brief           Timer device registration for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements Timer device registration using Kconfig-driven
 *                  configuration. Provides factory functions for test access
 *                  and manages Timer instance lifecycle.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_timer.h"
#include "nexus_config.h"
#include "nx_timer_helpers.h"
#include "nx_timer_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define NX_TIMER_MAX_INSTANCES 8
#define DEVICE_TYPE            NX_TIMER

/*---------------------------------------------------------------------------*/
/* Static Storage                                                            */
/*---------------------------------------------------------------------------*/

static nx_timer_state_t g_timer_states[NX_TIMER_MAX_INSTANCES];
static nx_timer_impl_t g_timer_instances[NX_TIMER_MAX_INSTANCES];

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Base interface methods */
static void timer_start(nx_timer_base_t* self);
static void timer_stop(nx_timer_base_t* self);
static void timer_set_period(nx_timer_base_t* self, uint16_t prescaler,
                             uint32_t period);
static uint32_t timer_get_count(nx_timer_base_t* self);
static nx_status_t timer_set_callback(nx_timer_base_t* self,
                                      nx_timer_callback_t callback,
                                      void* user_data);
static nx_lifecycle_t* timer_get_lifecycle(nx_timer_base_t* self);
static nx_power_t* timer_get_power(nx_timer_base_t* self);

/* Interface implementations (defined in separate files) */
extern void timer_init_lifecycle(nx_lifecycle_t* lifecycle);
extern void timer_init_power(nx_power_t* power);

/*---------------------------------------------------------------------------*/
/* Base Interface Implementation                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Start timer
 */
static void timer_start(nx_timer_base_t* self) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (impl && impl->state && impl->state->initialized) {
        impl->state->running = true;
        impl->state->counter = 0;
    }
}

/**
 * \brief           Stop timer
 */
static void timer_stop(nx_timer_base_t* self) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (impl && impl->state) {
        impl->state->running = false;
    }
}

/**
 * \brief           Set timer period
 */
static void timer_set_period(nx_timer_base_t* self, uint16_t prescaler,
                             uint32_t period) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (impl && impl->state) {
        impl->state->config.prescaler = prescaler;
        impl->state->config.period = period;
    }
}

/**
 * \brief           Get timer counter value
 */
static uint32_t timer_get_count(nx_timer_base_t* self) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (impl && impl->state) {
        return impl->state->counter;
    }
    return 0;
}

/**
 * \brief           Set timer callback
 */
static nx_status_t timer_set_callback(nx_timer_base_t* self,
                                      nx_timer_callback_t callback,
                                      void* user_data) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    impl->state->callback = callback;
    impl->state->user_data = user_data;
    return NX_OK;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* timer_get_lifecycle(nx_timer_base_t* self) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* timer_get_power(nx_timer_base_t* self) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    return impl ? &impl->power : NULL;
}

/*---------------------------------------------------------------------------*/
/* Instance Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize timer instance with platform configuration
 */
static void
timer_init_instance(nx_timer_impl_t* impl, uint8_t index,
                    const nx_timer_platform_config_t* platform_cfg) {
    /* Initialize base interface */
    impl->base.start = timer_start;
    impl->base.stop = timer_stop;
    impl->base.set_period = timer_set_period;
    impl->base.get_count = timer_get_count;
    impl->base.set_callback = timer_set_callback;
    impl->base.get_lifecycle = timer_get_lifecycle;
    impl->base.get_power = timer_get_power;

    /* Initialize interfaces (implemented in separate files) */
    timer_init_lifecycle(&impl->lifecycle);
    timer_init_power(&impl->power);

    /* Link to state */
    impl->state = &g_timer_states[index];
    impl->state->index = index;
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->running = false;
    impl->state->counter = 0;
    impl->state->callback = NULL;
    impl->state->user_data = NULL;

    /* Set configuration from Kconfig */
    if (platform_cfg != NULL) {
        impl->state->config.frequency = platform_cfg->frequency;
        impl->state->config.prescaler = 0;
        impl->state->config.period = 0;
        impl->state->config.channel_count = platform_cfg->channel_count;
    }
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
static void* nx_timer_device_init(const nx_device_t* dev) {
    const nx_timer_platform_config_t* config =
        (const nx_timer_platform_config_t*)dev->config;

    if (config == NULL || config->timer_index >= NX_TIMER_MAX_INSTANCES) {
        return NULL;
    }

    nx_timer_impl_t* impl = &g_timer_instances[config->timer_index];

    /* Initialize instance with platform configuration */
    timer_init_instance(impl, config->timer_index, config);

    /* Initialize lifecycle */
    nx_status_t status = impl->lifecycle.init(&impl->lifecycle);
    if (status != NX_OK) {
        return NULL;
    }

    return &impl->base;
}

/**
 * \brief           Configuration macro - reads from Kconfig
 */
#define NX_TIMER_CONFIG(index)                                                 \
    static const nx_timer_platform_config_t timer_config_##index = {           \
        .timer_index = index,                                                  \
        .frequency = CONFIG_TIMER##index##_FREQUENCY,                          \
        .channel_count = CONFIG_TIMER##index##_CHANNEL_COUNT,                  \
    }

/**
 * \brief           Device registration macro
 */
#define NX_TIMER_DEVICE_REGISTER(index)                                        \
    NX_TIMER_CONFIG(index);                                                    \
    static nx_device_config_state_t timer_kconfig_state_##index = {            \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, index, "TIMER" #index,                     \
                       &timer_config_##index, &timer_kconfig_state_##index,    \
                       nx_timer_device_init)

/* Register all enabled timer instances */
NX_TRAVERSE_EACH_INSTANCE(NX_TIMER_DEVICE_REGISTER, DEVICE_TYPE);

/*---------------------------------------------------------------------------*/
/* Legacy Factory Functions (for backward compatibility)                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get timer instance (legacy)
 */
nx_timer_base_t* nx_timer_native_get(uint8_t index) {
    if (index >= NX_TIMER_MAX_INSTANCES) {
        return NULL;
    }

    /* Use device registration mechanism */
    char name[16];
    snprintf(name, sizeof(name), "TIMER%d", index);
    return (nx_timer_base_t*)nx_device_get(name);
}

/**
 * \brief           Reset all timer instances (for testing)
 */
void nx_timer_native_reset_all(void) {
    for (uint8_t i = 0; i < NX_TIMER_MAX_INSTANCES; i++) {
        nx_timer_impl_t* impl = &g_timer_instances[i];
        if (impl->state && impl->state->initialized) {
            impl->lifecycle.deinit(&impl->lifecycle);
        }
        memset(&g_timer_states[i], 0, sizeof(nx_timer_state_t));
    }
}

/**
 * \brief           Trigger timer callback (for testing)
 */
void nx_timer_native_trigger_callback(uint8_t index) {
    if (index >= NX_TIMER_MAX_INSTANCES) {
        return;
    }

    nx_timer_impl_t* impl = &g_timer_instances[index];
    if (impl->state && impl->state->initialized && impl->state->callback) {
        impl->state->callback(impl->state->user_data);
    }
}

/**
 * \brief           Increment timer counter (for testing)
 */
void nx_timer_native_increment_counter(uint8_t index, uint32_t value) {
    if (index >= NX_TIMER_MAX_INSTANCES) {
        return;
    }

    nx_timer_impl_t* impl = &g_timer_instances[index];
    if (impl->state && impl->state->initialized && impl->state->running) {
        impl->state->counter += value;
    }
}

/*---------------------------------------------------------------------------*/
/* Test Support Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Timer state (for testing)
 */
nx_status_t nx_timer_native_get_state(uint8_t index, bool* initialized,
                                      bool* suspended, bool* running) {
    if (index >= NX_TIMER_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_timer_impl_t* impl = &g_timer_instances[index];
    if (!impl->state) {
        return NX_ERR_NULL_PTR;
    }

    if (initialized) {
        *initialized = impl->state->initialized;
    }
    if (suspended) {
        *suspended = impl->state->suspended;
    }
    if (running) {
        *running = impl->state->running;
    }

    return NX_OK;
}

/**
 * \brief           Get Timer counter value (for testing)
 */
nx_status_t nx_timer_native_get_counter(uint8_t index, uint32_t* counter) {
    if (index >= NX_TIMER_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }
    if (!counter) {
        return NX_ERR_NULL_PTR;
    }

    nx_timer_impl_t* impl = &g_timer_instances[index];
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    *counter = impl->state->counter;
    return NX_OK;
}

/**
 * \brief           Reset Timer instance (for testing)
 */
nx_status_t nx_timer_native_reset(uint8_t index) {
    if (index >= NX_TIMER_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_timer_impl_t* impl = &g_timer_instances[index];
    if (!impl->state) {
        return NX_ERR_NULL_PTR;
    }

    /* Reset state */
    impl->state->counter = 0;
    impl->state->running = false;
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->callback = NULL;
    impl->state->user_data = NULL;

    return NX_OK;
}
