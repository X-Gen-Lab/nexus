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
#include "hal/system/nx_mem.h"
#include "nexus_config.h"
#include "nx_rtc_helpers.h"
#include "nx_rtc_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE NX_RTC

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

    /* Allocate and initialize state */
    impl->state = (nx_rtc_state_t*)nx_mem_alloc(sizeof(nx_rtc_state_t));
    if (!impl->state) {
        return;
    }
    memset(impl->state, 0, sizeof(nx_rtc_state_t));

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

    /* Initialize start timestamp */
    impl->state->start_timestamp_ms = rtc_get_system_time_ms();
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
NX_UNUSED static void* nx_rtc_device_init(const nx_device_t* dev) {
    const nx_rtc_platform_config_t* config =
        (const nx_rtc_platform_config_t*)dev->config;

    if (config == NULL) {
        return NULL;
    }

    /* Allocate implementation structure */
    nx_rtc_impl_t* impl = (nx_rtc_impl_t*)nx_mem_alloc(sizeof(nx_rtc_impl_t));
    if (!impl) {
        return NULL;
    }
    memset(impl, 0, sizeof(nx_rtc_impl_t));

    /* Initialize instance with platform configuration */
    rtc_init_instance(impl, config->rtc_index, config);

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
#define NX_RTC_CONFIG(index)                                                   \
    static const nx_rtc_platform_config_t rtc_config_##index = {               \
        .rtc_index = index,                                                    \
        .enable_alarm = NX_CONFIG_RTC##index##_ENABLE_ALARM,                   \
        .alarm_count = NX_CONFIG_RTC##index##_ALARM_COUNT,                     \
    }

/**
 * \brief           Device registration macro
 */
#define NX_RTC_DEVICE_REGISTER(index)                                          \
    NX_RTC_CONFIG(index);                                                      \
    static nx_device_config_state_t rtc_kconfig_state_##index = {              \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, index, "RTC" #index, &rtc_config_##index,  \
                       &rtc_kconfig_state_##index, nx_rtc_device_init);

/**
 * \brief           Register all enabled RTC instances
 */
NX_TRAVERSE_EACH_INSTANCE(NX_RTC_DEVICE_REGISTER, DEVICE_TYPE)
