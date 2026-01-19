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
#include "nexus_config.h"
#include "nx_watchdog_helpers.h"
#include "nx_watchdog_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define NX_WATCHDOG_MAX_INSTANCES 4
#define DEVICE_TYPE               NX_WATCHDOG

/*---------------------------------------------------------------------------*/
/* Static Storage                                                            */
/*---------------------------------------------------------------------------*/

static nx_watchdog_state_t g_watchdog_states[NX_WATCHDOG_MAX_INSTANCES];
static nx_watchdog_impl_t g_watchdog_instances[NX_WATCHDOG_MAX_INSTANCES];
static uint8_t g_watchdog_instance_count = 0;

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

    /* Link to state */
    impl->state = &g_watchdog_states[index];
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

    /* Clear statistics */
    memset(&impl->state->stats, 0, sizeof(nx_watchdog_stats_t));

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

    if (g_watchdog_instance_count >= NX_WATCHDOG_MAX_INSTANCES) {
        return NULL;
    }

    uint8_t index = g_watchdog_instance_count++;
    nx_watchdog_impl_t* impl = &g_watchdog_instances[index];

    /* Initialize instance with platform configuration */
    watchdog_init_instance(impl, index, config);

    /* Store device pointer */
    impl->device = (nx_device_t*)dev;

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
#define NX_WATCHDOG_CONFIG(idx)                                                \
    static const nx_watchdog_platform_config_t watchdog_config_##idx = {       \
        .index = idx,                                                          \
        .default_timeout = NX_CONFIG_WATCHDOG##idx##_DEFAULT_TIMEOUT_MS,       \
    }

/* Register all enabled Watchdog instances */
#if defined(NX_CONFIG_INSTANCE_NX_WATCHDOG0)
NX_WATCHDOG_CONFIG(0);
static nx_device_config_state_t watchdog_kconfig_state_0 = {
    .init_res = 0,
    .initialized = false,
};
NX_DEVICE_REGISTER(DEVICE_TYPE, 0, "WATCHDOG0", &watchdog_config_0,
                   &watchdog_kconfig_state_0, nx_watchdog_device_init);
#endif

#if defined(NX_CONFIG_INSTANCE_NX_WATCHDOG1)
NX_WATCHDOG_CONFIG(1);
static nx_device_config_state_t watchdog_kconfig_state_1 = {
    .init_res = 0,
    .initialized = false,
};
NX_DEVICE_REGISTER(DEVICE_TYPE, 1, "WATCHDOG1", &watchdog_config_1,
                   &watchdog_kconfig_state_1, nx_watchdog_device_init);
#endif

/*---------------------------------------------------------------------------*/
/* Factory Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Watchdog instance by index
 */
nx_watchdog_t* nx_watchdog_native_get(uint8_t index) {
    if (index >= NX_WATCHDOG_MAX_INSTANCES) {
        return NULL;
    }

    /* Use device registration mechanism */
    char name[16];
    snprintf(name, sizeof(name), "WATCHDOG%d", index);
    return (nx_watchdog_t*)nx_device_get(name);
}

/**
 * \brief           Reset all Watchdog instances (for testing)
 */
void nx_watchdog_native_reset_all(void) {
    for (uint8_t i = 0; i < g_watchdog_instance_count; i++) {
        nx_watchdog_impl_t* impl = &g_watchdog_instances[i];
        if (impl->state && impl->state->initialized) {
            impl->lifecycle.deinit(&impl->lifecycle);
        }
        memset(&g_watchdog_states[i], 0, sizeof(nx_watchdog_state_t));
    }
    g_watchdog_instance_count = 0;
}

/**
 * \brief           Reset Watchdog instance (for testing)
 */
nx_status_t nx_watchdog_native_reset(uint8_t index) {
    if (index >= g_watchdog_instance_count) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_watchdog_impl_t* impl = &g_watchdog_instances[index];
    if (impl->state) {
        watchdog_reset_state(impl->state);
        return NX_OK;
    }

    return NX_ERR_NOT_FOUND;
}

/*---------------------------------------------------------------------------*/
/* Test Support Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Watchdog state (for testing)
 */
nx_status_t nx_watchdog_native_get_state(uint8_t index, bool* initialized,
                                         bool* suspended) {
    if (index >= g_watchdog_instance_count) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_watchdog_impl_t* impl = &g_watchdog_instances[index];
    if (impl->state) {
        if (initialized) {
            *initialized = impl->state->initialized;
        }
        if (suspended) {
            *suspended = impl->state->suspended;
        }
        return NX_OK;
    }

    return NX_ERR_NOT_FOUND;
}

/**
 * \brief           Get Watchdog device descriptor (for testing)
 */
nx_device_t* nx_watchdog_native_get_device(uint8_t index) {
    if (index >= g_watchdog_instance_count) {
        return NULL;
    }

    return g_watchdog_instances[index].device;
}

/**
 * \brief           Check if watchdog has timed out (for testing)
 */
bool nx_watchdog_native_has_timed_out(uint8_t index) {
    if (index >= g_watchdog_instance_count) {
        return false;
    }

    nx_watchdog_impl_t* impl = &g_watchdog_instances[index];
    if (impl->state == NULL || !impl->state->initialized) {
        return false;
    }

    return watchdog_check_timeout(impl->state);
}

/**
 * \brief           Simulate time passage (for testing)
 */
nx_status_t nx_watchdog_native_advance_time(uint8_t index,
                                            uint32_t milliseconds) {
    if (index >= g_watchdog_instance_count) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_watchdog_impl_t* impl = &g_watchdog_instances[index];
    if (impl->state == NULL || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Advance time by moving last feed time backwards */
    impl->state->last_feed_time_ms -= milliseconds;

    /* Check for timeout and trigger callback if needed */
    if (watchdog_check_timeout(impl->state)) {
        impl->state->stats.timeout_count++;
        if (impl->state->callback) {
            impl->state->callback(impl->state->user_data);
        }
    }

    return NX_OK;
}
