/**
 * \file            nx_rtc_device.c
 * \brief           RTC device registration for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements RTC device registration using Kconfig-driven
 *                  configuration. Provides factory functions for test access
 *                  and manages RTC instance lifecycle.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_rtc.h"
#include "nexus_config.h"
#include "nx_rtc_helpers.h"
#include "nx_rtc_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define NX_RTC_MAX_INSTANCES 4
#define DEVICE_TYPE          NX_RTC

/*---------------------------------------------------------------------------*/
/* Static Storage                                                            */
/*---------------------------------------------------------------------------*/

static nx_rtc_state_t g_rtc_states[NX_RTC_MAX_INSTANCES];
static nx_rtc_impl_t g_rtc_instances[NX_RTC_MAX_INSTANCES];
static uint8_t g_rtc_instance_count = 0;

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Interface implementations (defined in separate files) */
extern void rtc_init_interface(nx_rtc_t* rtc);
extern void rtc_init_lifecycle(nx_lifecycle_t* lifecycle);
extern void rtc_init_power(nx_power_t* power);

/*---------------------------------------------------------------------------*/
/* Instance Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize RTC instance with platform configuration
 */
static void rtc_init_instance(nx_rtc_impl_t* impl, uint8_t index,
                              const nx_rtc_platform_config_t* platform_cfg) {
    /* Initialize interfaces (implemented in separate files) */
    rtc_init_interface(&impl->base);
    rtc_init_lifecycle(&impl->lifecycle);
    rtc_init_power(&impl->power);

    /* Link to state */
    impl->state = &g_rtc_states[index];
    impl->state->index = index;
    impl->state->initialized = false;
    impl->state->suspended = false;

    /* Set configuration from Kconfig */
    if (platform_cfg != NULL) {
        impl->state->config.enable_alarm = platform_cfg->enable_alarm;
    }

    /* Initialize current time to Unix epoch */
    impl->state->current_time.year = 1970;
    impl->state->current_time.month = 1;
    impl->state->current_time.day = 1;
    impl->state->current_time.hour = 0;
    impl->state->current_time.minute = 0;
    impl->state->current_time.second = 0;

    /* Clear alarm */
    memset(&impl->state->alarm, 0, sizeof(nx_rtc_alarm_t));

    /* Clear statistics */
    memset(&impl->state->stats, 0, sizeof(nx_rtc_stats_t));

    /* Initialize start timestamp */
    impl->state->start_timestamp_ms = rtc_get_system_time_ms();
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
static void* nx_rtc_device_init(const nx_device_t* dev) {
    const nx_rtc_platform_config_t* config =
        (const nx_rtc_platform_config_t*)dev->config;

    if (config == NULL || g_rtc_instance_count >= NX_RTC_MAX_INSTANCES) {
        return NULL;
    }

    uint8_t index = g_rtc_instance_count++;
    nx_rtc_impl_t* impl = &g_rtc_instances[index];

    /* Initialize instance with platform configuration */
    rtc_init_instance(impl, index, config);

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
#define NX_RTC_CONFIG(idx)                                                     \
    static const nx_rtc_platform_config_t rtc_config_##idx = {                 \
        .index = idx,                                                          \
        .enable_alarm = NX_CONFIG_RTC##idx##_ENABLE_ALARM,                     \
        .alarm_count = NX_CONFIG_RTC##idx##_ALARM_COUNT,                       \
    }

/**
 * \brief           Device registration macro
 */
#define NX_RTC_DEVICE_REGISTER(idx)                                            \
    NX_RTC_CONFIG(idx)                                                         \
    static nx_device_config_state_t rtc_kconfig_state_##idx = {                \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, idx, "RTC" #idx, &rtc_config_##idx,        \
                       &rtc_kconfig_state_##idx, nx_rtc_device_init)

/* Register all enabled RTC instances */
#if defined(NX_CONFIG_INSTANCE_NX_RTC0)
NX_RTC_CONFIG(0);
static nx_device_config_state_t rtc_kconfig_state_0 = {
    .init_res = 0,
    .initialized = false,
};
NX_DEVICE_REGISTER(DEVICE_TYPE, 0, "RTC0", &rtc_config_0, &rtc_kconfig_state_0,
                   nx_rtc_device_init);
#endif
