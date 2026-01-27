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
#include "hal/system/nx_mem.h"
#include "nexus_config.h"
#include "nx_timer_helpers.h"
#include "nx_timer_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE NX_TIMER

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

    /* Allocate and initialize state */
    impl->state = (nx_timer_state_t*)nx_mem_alloc(sizeof(nx_timer_state_t));
    if (!impl->state) {
        return;
    }
    memset(impl->state, 0, sizeof(nx_timer_state_t));

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

    if (config == NULL) {
        return NULL;
    }

    /* Allocate implementation structure */
    nx_timer_impl_t* impl =
        (nx_timer_impl_t*)nx_mem_alloc(sizeof(nx_timer_impl_t));
    if (!impl) {
        return NULL;
    }
    memset(impl, 0, sizeof(nx_timer_impl_t));

    /* Initialize instance with platform configuration */
    timer_init_instance(impl, config->timer_index, config);

    /* Check if state allocation succeeded */
    if (!impl->state) {
        nx_mem_free(impl);
        return NULL;
    }

    /* Device is created but not initialized - tests will call init() */
    return &impl->base;
}

/**
 * \brief           Configuration macro - reads from Kconfig
 */
#define NX_TIMER_CONFIG(index)                                                 \
    static const nx_timer_platform_config_t timer_config_##index = {           \
        .timer_index = index,                                                  \
        .frequency = NX_CONFIG_TIMER##index##_FREQUENCY,                       \
        .channel_count = NX_CONFIG_TIMER##index##_CHANNEL_COUNT,               \
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
                       nx_timer_device_init);

/**
 * \brief           Register all enabled timer instances
 */
NX_TRAVERSE_EACH_INSTANCE(NX_TIMER_DEVICE_REGISTER, DEVICE_TYPE)
