/**
 * \file            nx_watchdog_device.c
 * \brief           Watchdog device registration for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements Watchdog device registration using Kconfig-driven
 *                  configuration. Provides factory functions for test access
 *                  and manages Watchdog instance lifecycle.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_watchdog.h"
#include "hal/system/nx_mem.h"
#include "nexus_config.h"
#include "nx_watchdog_helpers.h"
#include "nx_watchdog_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE NX_WATCHDOG

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Interface implementations (defined in separate files) */
extern void nx_watchdog_interface_init(nx_watchdog_impl_t* impl);
extern void nx_watchdog_lifecycle_init(nx_lifecycle_t* lifecycle);
extern void nx_watchdog_power_init(nx_power_t* power);

/*---------------------------------------------------------------------------*/
/* Instance Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize Watchdog instance with platform configuration
 */
static void
watchdog_init_instance(nx_watchdog_impl_t* impl, uint8_t index,
                       const nx_watchdog_platform_config_t* platform_cfg) {
    /* Initialize interfaces (implemented in separate files) */
    nx_watchdog_interface_init(impl);
    nx_watchdog_lifecycle_init(&impl->lifecycle);
    nx_watchdog_power_init(&impl->power);

    /* Allocate and initialize state */
    impl->state =
        (nx_watchdog_state_t*)nx_mem_alloc(sizeof(nx_watchdog_state_t));
    if (!impl->state) {
        return;
    }
    memset(impl->state, 0, sizeof(nx_watchdog_state_t));

    impl->state->index = index;
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->running = false;

    /* Set configuration from Kconfig */
    if (platform_cfg != NULL) {
        impl->state->config.timeout_ms = platform_cfg->default_timeout;
    } else {
        impl->state->config.timeout_ms = 5000; /* Default 5 seconds */
    }

    /* Clear callback */
    impl->state->callback = NULL;
    impl->state->user_data = NULL;

    /* Initialize last feed time */
    impl->state->last_feed_time_ms = 0;
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
static void* nx_watchdog_device_init(const nx_device_t* dev) {
    const nx_watchdog_platform_config_t* config =
        (const nx_watchdog_platform_config_t*)dev->config;

    /* Allocate implementation structure */
    nx_watchdog_impl_t* impl =
        (nx_watchdog_impl_t*)nx_mem_alloc(sizeof(nx_watchdog_impl_t));
    if (!impl) {
        return NULL;
    }
    memset(impl, 0, sizeof(nx_watchdog_impl_t));

    /* Initialize instance with platform configuration */
    uint8_t index = config ? config->watchdog_index : 0;
    watchdog_init_instance(impl, index, config);

    /* Check if state allocation succeeded */
    if (!impl->state) {
        nx_mem_free(impl);
        return NULL;
    }

    /* Store device pointer */
    impl->device = (nx_device_t*)dev;

    /* Device is created but not initialized - tests will call init() */
    return &impl->base;
}

/**
 * \brief           Configuration macro - reads from Kconfig
 */
#define NX_WATCHDOG_CONFIG(index)                                              \
    static const nx_watchdog_platform_config_t watchdog_config_##index = {     \
        .watchdog_index = index,                                               \
        .default_timeout = NX_CONFIG_WATCHDOG##index##_DEFAULT_TIMEOUT_MS,     \
    }

/**
 * \brief           Device registration macro
 */
#define NX_WATCHDOG_DEVICE_REGISTER(index)                                     \
    NX_WATCHDOG_CONFIG(index);                                                 \
    static nx_device_config_state_t watchdog_state_##index = {                 \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, index, "WATCHDOG" #index,                  \
                       &watchdog_config_##index, &watchdog_state_##index,      \
                       nx_watchdog_device_init);

/**
 * \brief           Register all enabled ADC instances
 */
NX_TRAVERSE_EACH_INSTANCE(NX_WATCHDOG_DEVICE_REGISTER, DEVICE_TYPE);
