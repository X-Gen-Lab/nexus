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
#include "nexus_config.h"
#include "nx_adc_helpers.h"
#include "nx_adc_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define NX_ADC_MAX_INSTANCES 4
#define DEVICE_TYPE          NX_ADC

/*---------------------------------------------------------------------------*/
/* Static Storage                                                            */
/*---------------------------------------------------------------------------*/

static nx_adc_state_t g_adc_states[NX_ADC_MAX_INSTANCES];
static nx_adc_impl_t g_adc_instances[NX_ADC_MAX_INSTANCES];

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

        /* Update simulated channel values */
        for (int i = 0; i < NX_ADC_MAX_CHANNELS; i++) {
            impl->channels[i].simulated_value = (uint16_t)(rand() % 4096);
        }
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

    /* Link to state */
    impl->state = &g_adc_states[index];
    impl->state->index = index;
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->clock_enabled = false;
    memset(&impl->state->stats, 0, sizeof(nx_adc_stats_t));

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
static void* nx_adc_device_init(const nx_device_t* dev) {
    const nx_adc_platform_config_t* config =
        (const nx_adc_platform_config_t*)dev->config;

    if (config == NULL || config->adc_index >= NX_ADC_MAX_INSTANCES) {
        return NULL;
    }

    nx_adc_impl_t* impl = &g_adc_instances[config->adc_index];

    /* Initialize instance with platform configuration */
    adc_init_instance(impl, config->adc_index, config);

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
#define NX_ADC_CONFIG(index)                                                   \
    static const nx_adc_platform_config_t adc_config_##index = {               \
        .adc_index = index,                                                    \
        .channel_count = CONFIG_ADC##index##_CHANNEL_COUNT,                    \
        .resolution = CONFIG_ADC##index##_RESOLUTION,                          \
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
                       &adc_kconfig_state_##index, nx_adc_device_init)

/* Register all enabled ADC instances */
NX_TRAVERSE_EACH_INSTANCE(NX_ADC_DEVICE_REGISTER, DEVICE_TYPE);

/*---------------------------------------------------------------------------*/
/* Legacy Factory Functions (for backward compatibility)                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get ADC instance (legacy)
 */
nx_adc_t* nx_adc_native_get(uint8_t index) {
    if (index >= NX_ADC_MAX_INSTANCES) {
        return NULL;
    }

    /* Use device registration mechanism */
    char name[16];
    snprintf(name, sizeof(name), "ADC%d", index);
    return (nx_adc_t*)nx_device_get(name);
}

/**
 * \brief           Reset all ADC instances (for testing)
 */
void nx_adc_native_reset_all(void) {
    for (uint8_t i = 0; i < NX_ADC_MAX_INSTANCES; i++) {
        nx_adc_impl_t* impl = &g_adc_instances[i];
        if (impl->state && impl->state->initialized) {
            impl->lifecycle.deinit(&impl->lifecycle);
        }
        memset(&g_adc_states[i], 0, sizeof(nx_adc_state_t));
    }
}

/**
 * \brief           Set simulated ADC channel value (for testing)
 */
void nx_adc_native_set_simulated_value(uint8_t adc_index, uint8_t channel,
                                       uint16_t value) {
    if (adc_index >= NX_ADC_MAX_INSTANCES || channel >= NX_ADC_MAX_CHANNELS) {
        return;
    }

    nx_adc_impl_t* impl = &g_adc_instances[adc_index];
    impl->channels[channel].simulated_value = value;
}

/*---------------------------------------------------------------------------*/
/* Test Support Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Set simulated ADC value (for testing)
 */
nx_status_t nx_adc_native_set_value(uint8_t index, uint8_t channel,
                                    uint16_t value) {
    if (index >= NX_ADC_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }
    if (channel >= NX_ADC_MAX_CHANNELS) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_adc_impl_t* impl = &g_adc_instances[index];
    impl->channels[channel].simulated_value = value;
    return NX_OK;
}

/**
 * \brief           Get ADC state (for testing)
 */
nx_status_t nx_adc_native_get_state(uint8_t index, bool* initialized,
                                    bool* suspended) {
    if (index >= NX_ADC_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_adc_impl_t* impl = &g_adc_instances[index];
    if (!impl->state) {
        return NX_ERR_NULL_PTR;
    }

    if (initialized) {
        *initialized = impl->state->initialized;
    }
    if (suspended) {
        *suspended = impl->state->suspended;
    }

    return NX_OK;
}

/**
 * \brief           Reset ADC instance (for testing)
 */
nx_status_t nx_adc_native_reset(uint8_t index) {
    if (index >= NX_ADC_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_adc_impl_t* impl = &g_adc_instances[index];
    if (!impl->state) {
        return NX_ERR_NULL_PTR;
    }

    /* Reset all channel values */
    for (uint8_t i = 0; i < NX_ADC_MAX_CHANNELS; i++) {
        impl->channels[i].simulated_value = 0;
    }

    /* Reset state flags */
    impl->state->initialized = false;
    impl->state->suspended = false;

    return NX_OK;
}
