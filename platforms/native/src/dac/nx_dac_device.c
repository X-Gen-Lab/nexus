/**
 * \file            nx_dac_device.c
 * \brief           DAC device registration for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements DAC device registration using Kconfig-driven
 *                  configuration. Provides factory functions for test access
 *                  and manages DAC instance lifecycle.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_dac.h"
#include "nexus_config.h"
#include "nx_dac_helpers.h"
#include "nx_dac_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define NX_DAC_MAX_INSTANCES 4
#define DEVICE_TYPE          NX_DAC

/*---------------------------------------------------------------------------*/
/* Static Storage                                                            */
/*---------------------------------------------------------------------------*/

static nx_dac_state_t g_dac_states[NX_DAC_MAX_INSTANCES];
static nx_dac_impl_t g_dac_instances[NX_DAC_MAX_INSTANCES];

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Base interface methods */
static nx_dac_channel_t* dac_get_channel(nx_dac_t* self, uint8_t channel_index);
static void dac_trigger(nx_dac_t* self);
static nx_lifecycle_t* dac_get_lifecycle(nx_dac_t* self);
static nx_power_t* dac_get_power(nx_dac_t* self);

/* Channel interface methods */
static void dac_channel_set_value(nx_dac_channel_t* self, uint32_t value);
static void dac_channel_set_voltage_mv(nx_dac_channel_t* self,
                                       uint32_t voltage_mv);

/* Interface implementations (defined in separate files) */
extern void dac_init_lifecycle(nx_lifecycle_t* lifecycle);
extern void dac_init_power(nx_power_t* power);

/*---------------------------------------------------------------------------*/
/* Channel Interface Implementation                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Set DAC channel output value (raw)
 */
static void dac_channel_set_value(nx_dac_channel_t* self, uint32_t value) {
    nx_dac_channel_impl_t* channel = dac_get_channel_impl(self);
    if (!channel) {
        return;
    }

    /* Clamp to resolution */
    uint32_t max_value = (1U << channel->resolution_bits) - 1;
    if (value > max_value) {
        value = max_value;
    }

    channel->current_value = value;
}

/**
 * \brief           Set DAC channel output voltage in millivolts
 */
static void dac_channel_set_voltage_mv(nx_dac_channel_t* self,
                                       uint32_t voltage_mv) {
    nx_dac_channel_impl_t* channel = dac_get_channel_impl(self);
    if (!channel) {
        return;
    }

    /* Convert voltage to raw value */
    uint32_t max_value = (1U << channel->resolution_bits) - 1;
    uint32_t value = (voltage_mv * max_value) / channel->vref_mv;

    /* Clamp to max */
    if (value > max_value) {
        value = max_value;
    }

    channel->current_value = value;
}

/*---------------------------------------------------------------------------*/
/* Base Interface Implementation                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get DAC channel interface
 */
static nx_dac_channel_t* dac_get_channel(nx_dac_t* self,
                                         uint8_t channel_index) {
    nx_dac_impl_t* impl = dac_get_impl(self);
    if (!impl) {
        return NULL;
    }
    if (channel_index >= NX_DAC_MAX_CHANNELS) {
        return NULL;
    }
    return &impl->channels[channel_index].base;
}

/**
 * \brief           Trigger DAC output update
 */
static void dac_trigger(nx_dac_t* self) {
    nx_dac_impl_t* impl = dac_get_impl(self);
    if (impl && impl->state && impl->state->initialized) {
        /* In simulation, trigger is a no-op */
        /* In real hardware, this would update all channels simultaneously */
    }
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* dac_get_lifecycle(nx_dac_t* self) {
    nx_dac_impl_t* impl = dac_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* dac_get_power(nx_dac_t* self) {
    nx_dac_impl_t* impl = dac_get_impl(self);
    return impl ? &impl->power : NULL;
}

/*---------------------------------------------------------------------------*/
/* Instance Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize DAC instance with platform configuration
 */
static void dac_init_instance(nx_dac_impl_t* impl, uint8_t index,
                              const nx_dac_platform_config_t* platform_cfg) {
    /* Initialize base interface */
    impl->base.get_channel = dac_get_channel;
    impl->base.trigger = dac_trigger;
    impl->base.get_lifecycle = dac_get_lifecycle;
    impl->base.get_power = dac_get_power;

    /* Initialize interfaces (implemented in separate files) */
    dac_init_lifecycle(&impl->lifecycle);
    dac_init_power(&impl->power);

    /* Link to state */
    impl->state = &g_dac_states[index];
    impl->state->index = index;
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->clock_enabled = false;

    /* Set configuration from Kconfig */
    if (platform_cfg != NULL) {
        impl->state->config.channel_count = platform_cfg->channel_count;
        impl->state->config.resolution = platform_cfg->resolution;
        impl->state->config.vref_mv = platform_cfg->vref_mv;
    }

    /* Initialize channel interfaces */
    for (int i = 0; i < NX_DAC_MAX_CHANNELS; i++) {
        impl->channels[i].base.set_value = dac_channel_set_value;
        impl->channels[i].base.set_voltage_mv = dac_channel_set_voltage_mv;
        impl->channels[i].channel_index = (uint8_t)i;
        impl->channels[i].current_value = 0;
        impl->channels[i].vref_mv = platform_cfg ? platform_cfg->vref_mv : 3300;
        impl->channels[i].resolution_bits =
            platform_cfg ? platform_cfg->resolution : 12;
    }
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
static void* nx_dac_device_init(const nx_device_t* dev) {
    const nx_dac_platform_config_t* config =
        (const nx_dac_platform_config_t*)dev->config;

    if (config == NULL || config->dac_index >= NX_DAC_MAX_INSTANCES) {
        return NULL;
    }

    nx_dac_impl_t* impl = &g_dac_instances[config->dac_index];

    /* Initialize instance with platform configuration */
    dac_init_instance(impl, config->dac_index, config);

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
#define NX_DAC_CONFIG(index)                                                   \
    static const nx_dac_platform_config_t dac_config_##index = {               \
        .dac_index = index,                                                    \
        .channel_count = CONFIG_DAC##index##_CHANNEL_COUNT,                    \
        .resolution = CONFIG_DAC##index##_RESOLUTION,                          \
        .vref_mv = CONFIG_DAC##index##_VREF_MV,                                \
    }

/**
 * \brief           Device registration macro
 */
#define NX_DAC_DEVICE_REGISTER(index)                                          \
    NX_DAC_CONFIG(index);                                                      \
    static nx_device_config_state_t dac_kconfig_state_##index = {              \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, index, "DAC" #index, &dac_config_##index,  \
                       &dac_kconfig_state_##index, nx_dac_device_init)

/* Register all enabled DAC instances */
NX_TRAVERSE_EACH_INSTANCE(NX_DAC_DEVICE_REGISTER, DEVICE_TYPE);

/*---------------------------------------------------------------------------*/
/* Legacy Factory Functions (for backward compatibility)                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get DAC instance (legacy)
 */
nx_dac_t* nx_dac_native_get(uint8_t index) {
    if (index >= NX_DAC_MAX_INSTANCES) {
        return NULL;
    }

    /* Use device registration mechanism */
    char name[16];
    snprintf(name, sizeof(name), "DAC%d", index);
    return (nx_dac_t*)nx_device_get(name);
}

/**
 * \brief           Reset all DAC instances (for testing)
 */
void nx_dac_native_reset_all(void) {
    for (uint8_t i = 0; i < NX_DAC_MAX_INSTANCES; i++) {
        nx_dac_impl_t* impl = &g_dac_instances[i];
        if (impl->state && impl->state->initialized) {
            impl->lifecycle.deinit(&impl->lifecycle);
        }
        memset(&g_dac_states[i], 0, sizeof(nx_dac_state_t));
    }
}

/**
 * \brief           Get DAC channel value (for testing)
 */
uint32_t nx_dac_native_get_channel_value(uint8_t dac_index, uint8_t channel) {
    if (dac_index >= NX_DAC_MAX_INSTANCES || channel >= NX_DAC_MAX_CHANNELS) {
        return 0;
    }

    nx_dac_impl_t* impl = &g_dac_instances[dac_index];
    return impl->channels[channel].current_value;
}

/*---------------------------------------------------------------------------*/
/* Test Support Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get DAC output value (for testing)
 */
nx_status_t nx_dac_native_get_value(uint8_t index, uint8_t channel,
                                    uint16_t* value) {
    if (index >= NX_DAC_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }
    if (channel >= NX_DAC_MAX_CHANNELS) {
        return NX_ERR_INVALID_PARAM;
    }
    if (!value) {
        return NX_ERR_NULL_PTR;
    }

    nx_dac_impl_t* impl = &g_dac_instances[index];
    *value = (uint16_t)(impl->channels[channel].current_value & 0xFFFF);
    return NX_OK;
}

/**
 * \brief           Get DAC state (for testing)
 */
nx_status_t nx_dac_native_get_state(uint8_t index, bool* initialized,
                                    bool* suspended) {
    if (index >= NX_DAC_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_dac_impl_t* impl = &g_dac_instances[index];
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
 * \brief           Reset DAC instance (for testing)
 */
nx_status_t nx_dac_native_reset(uint8_t index) {
    if (index >= NX_DAC_MAX_INSTANCES) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_dac_impl_t* impl = &g_dac_instances[index];
    if (!impl->state) {
        return NX_ERR_NULL_PTR;
    }

    /* Reset all channel values */
    for (uint8_t i = 0; i < NX_DAC_MAX_CHANNELS; i++) {
        impl->channels[i].current_value = 0;
    }

    /* Reset state flags */
    impl->state->initialized = false;
    impl->state->suspended = false;

    return NX_OK;
}
