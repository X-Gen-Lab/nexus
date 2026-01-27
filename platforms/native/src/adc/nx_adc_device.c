/**
 * \file            nx_adc_device.c
 * \brief           ADC device registration for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements ADC device registration using Kconfig-driven
 *                  configuration. Provides factory functions for test access
 *                  and manages ADC instance lifecycle.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_adc.h"
#include "hal/system/nx_mem.h"
#include "nexus_config.h"
#include "nx_adc_helpers.h"
#include "nx_adc_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE NX_ADC

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Base interface methods */
static void adc_trigger(nx_adc_t* self);
static nx_adc_channel_t* adc_get_channel(nx_adc_t* self, uint8_t channel_index);
static nx_lifecycle_t* adc_get_lifecycle(nx_adc_t* self);
static nx_power_t* adc_get_power(nx_adc_t* self);
static nx_diagnostic_t* adc_get_diagnostic(nx_adc_t* self);

/* Channel interface method */
static uint32_t adc_channel_get_value(nx_adc_channel_t* self);

/* Interface implementations (defined in separate files) */
extern void adc_init_lifecycle(nx_lifecycle_t* lifecycle);
extern void adc_init_power(nx_power_t* power);
extern void adc_init_diagnostic(nx_diagnostic_t* diagnostic);

/*---------------------------------------------------------------------------*/
/* Channel Interface Implementation                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get ADC channel conversion value
 */
static uint32_t adc_channel_get_value(nx_adc_channel_t* self) {
    nx_adc_channel_impl_t* channel = adc_get_channel_impl(self);
    if (!channel) {
        return 0;
    }
    return (uint32_t)channel->simulated_value;
}

/*---------------------------------------------------------------------------*/
/* Base Interface Implementation                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Trigger ADC conversion
 */
static void adc_trigger(nx_adc_t* self) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (impl && impl->state && impl->state->initialized) {
        impl->state->stats.conversion_count++;
        /* Note: Channel values are set via native_adc_set_analog_value() */
        /* Do not randomize here - use the values set by test helpers */
    }
}

/**
 * \brief           Get ADC channel interface
 */
static nx_adc_channel_t* adc_get_channel(nx_adc_t* self,
                                         uint8_t channel_index) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl) {
        return NULL;
    }
    if (channel_index >= NX_ADC_MAX_CHANNELS) {
        return NULL;
    }
    return &impl->channels[channel_index].base;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* adc_get_lifecycle(nx_adc_t* self) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* adc_get_power(nx_adc_t* self) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    return impl ? &impl->power : NULL;
}

/**
 * \brief           Get diagnostic interface
 */
static nx_diagnostic_t* adc_get_diagnostic(nx_adc_t* self) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    return impl ? &impl->diagnostic : NULL;
}

/*---------------------------------------------------------------------------*/
/* Instance Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize ADC instance with platform configuration
 */
static void adc_init_instance(nx_adc_impl_t* impl, uint8_t index,
                              const nx_adc_platform_config_t* platform_cfg) {
    /* Initialize base interface */
    impl->base.trigger = adc_trigger;
    impl->base.get_channel = adc_get_channel;
    impl->base.get_lifecycle = adc_get_lifecycle;
    impl->base.get_power = adc_get_power;
    impl->base.get_diagnostic = adc_get_diagnostic;

    /* Initialize interfaces (implemented in separate files) */
    adc_init_lifecycle(&impl->lifecycle);
    adc_init_power(&impl->power);
    adc_init_diagnostic(&impl->diagnostic);

    /* Allocate and initialize state */
    impl->state = (nx_adc_state_t*)nx_mem_alloc(sizeof(nx_adc_state_t));
    if (!impl->state) {
        return;
    }
    memset(impl->state, 0, sizeof(nx_adc_state_t));

    impl->state->index = index;
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->clock_enabled = false;

    /* Set configuration from Kconfig */
    if (platform_cfg != NULL) {
        impl->state->config.channel_count = platform_cfg->channel_count;
        impl->state->config.resolution = platform_cfg->resolution;
    }

    /* Initialize channel interfaces */
    for (int i = 0; i < NX_ADC_MAX_CHANNELS; i++) {
        impl->channels[i].base.get_value = adc_channel_get_value;
        impl->channels[i].channel_index = (uint8_t)i;
        impl->channels[i].simulated_value = 0;
    }
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
NX_UNUSED static void* nx_adc_device_init(const nx_device_t* dev) {
    const nx_adc_platform_config_t* config =
        (const nx_adc_platform_config_t*)dev->config;

    if (config == NULL) {
        return NULL;
    }

    /* Allocate implementation structure */
    nx_adc_impl_t* impl = (nx_adc_impl_t*)nx_mem_alloc(sizeof(nx_adc_impl_t));
    if (!impl) {
        return NULL;
    }
    memset(impl, 0, sizeof(nx_adc_impl_t));

    /* Initialize instance with platform configuration */
    adc_init_instance(impl, config->adc_index, config);

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
#define NX_ADC_CONFIG(index)                                                   \
    static const nx_adc_platform_config_t adc_config_##index = {               \
        .adc_index = index,                                                    \
        .channel_count = NX_CONFIG_ADC##index##_CHANNEL_COUNT,                 \
        .resolution = NX_CONFIG_ADC##index##_RESOLUTION,                       \
    }

/**
 * \brief           Device registration macro
 */
#define NX_ADC_DEVICE_REGISTER(index)                                          \
    NX_ADC_CONFIG(index);                                                      \
    static nx_device_config_state_t adc_kconfig_state_##index = {              \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, index, "ADC" #index, &adc_config_##index,  \
                       &adc_kconfig_state_##index, nx_adc_device_init);

/**
 * \brief           Register all enabled ADC instances
 */
NX_TRAVERSE_EACH_INSTANCE(NX_ADC_DEVICE_REGISTER, DEVICE_TYPE)
