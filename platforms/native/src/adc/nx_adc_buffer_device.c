/**
 * \file            nx_adc_buffer_device.c
 * \brief           ADC Buffer device registration for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements ADC Buffer device registration for buffered
 *                  ADC operations with DMA support simulation.
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

#define NX_ADC_BUFFER_MAX_INSTANCES 4
#define DEVICE_TYPE                 NX_ADC_BUFFER

/*---------------------------------------------------------------------------*/
/* Static Storage                                                            */
/*---------------------------------------------------------------------------*/

static nx_adc_buffer_state_t g_adc_buffer_states[NX_ADC_BUFFER_MAX_INSTANCES];
static nx_adc_buffer_impl_t g_adc_buffer_instances[NX_ADC_BUFFER_MAX_INSTANCES];

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Base interface methods */
static void adc_buffer_trigger(nx_adc_buffer_t* self);
static void adc_buffer_register_callback(nx_adc_buffer_t* self,
                                         nx_adc_buffer_callback_t callback,
                                         void* user_data);
static uint32_t* adc_buffer_get_buffer(nx_adc_buffer_t* self);
static size_t adc_buffer_get_buffer_size(nx_adc_buffer_t* self);
static nx_lifecycle_t* adc_buffer_get_lifecycle(nx_adc_buffer_t* self);
static nx_power_t* adc_buffer_get_power(nx_adc_buffer_t* self);

/* Interface implementations (defined in separate files) */
extern void adc_buffer_init_lifecycle(nx_lifecycle_t* lifecycle);
extern void adc_buffer_init_power(nx_power_t* power);

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Simulate DMA transfer filling buffer with samples
 */
static void adc_buffer_simulate_dma_transfer(nx_adc_buffer_impl_t* impl) {
    if (!impl || !impl->state || !impl->state->buffer) {
        return;
    }

    /* Fill buffer with simulated interleaved samples */
    for (size_t i = 0; i < impl->state->buffer_size; i++) {
        uint8_t channel = i % impl->state->channel_count;
        /* Generate simulated ADC value (0-4095 for 12-bit ADC) */
        impl->state->buffer[i] = (uint32_t)(rand() % 4096) + (channel * 100);
    }

    impl->state->current_index = impl->state->buffer_size;
}

/*---------------------------------------------------------------------------*/
/* Base Interface Implementation                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Trigger buffered sampling
 */
static void adc_buffer_trigger(nx_adc_buffer_t* self) {
    nx_adc_buffer_impl_t* impl = adc_buffer_get_impl(self);
    if (!impl || !impl->state) {
        return;
    }
    if (!impl->state->initialized) {
        return;
    }

    /* Simulate DMA-based sampling */
    impl->state->sampling_active = true;
    impl->state->current_index = 0;

    /* Simulate DMA transfer */
    adc_buffer_simulate_dma_transfer(impl);

    /* Invoke callback if registered */
    if (impl->state->callback) {
        impl->state->callback(impl->state->buffer, impl->state->buffer_size,
                              impl->state->user_data);
    }

    impl->state->sampling_active = false;
}

/**
 * \brief           Register buffer-full callback
 */
static void adc_buffer_register_callback(nx_adc_buffer_t* self,
                                         nx_adc_buffer_callback_t callback,
                                         void* user_data) {
    nx_adc_buffer_impl_t* impl = adc_buffer_get_impl(self);
    if (!impl || !impl->state) {
        return;
    }

    impl->state->callback = callback;
    impl->state->user_data = user_data;
}

/**
 * \brief           Get sample buffer pointer
 */
static uint32_t* adc_buffer_get_buffer(nx_adc_buffer_t* self) {
    nx_adc_buffer_impl_t* impl = adc_buffer_get_impl(self);
    if (!impl || !impl->state) {
        return NULL;
    }
    return impl->state->buffer;
}

/**
 * \brief           Get buffer capacity
 */
static size_t adc_buffer_get_buffer_size(nx_adc_buffer_t* self) {
    nx_adc_buffer_impl_t* impl = adc_buffer_get_impl(self);
    if (!impl || !impl->state) {
        return 0;
    }
    return impl->state->buffer_size;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* adc_buffer_get_lifecycle(nx_adc_buffer_t* self) {
    nx_adc_buffer_impl_t* impl = adc_buffer_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power management interface
 */
static nx_power_t* adc_buffer_get_power(nx_adc_buffer_t* self) {
    nx_adc_buffer_impl_t* impl = adc_buffer_get_impl(self);
    return impl ? &impl->power : NULL;
}

/*---------------------------------------------------------------------------*/
/* Instance Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize ADC buffer instance with platform configuration
 */
static void
adc_buffer_init_instance(nx_adc_buffer_impl_t* impl, uint8_t index,
                         const nx_adc_buffer_platform_config_t* platform_cfg) {
    /* Initialize base interface */
    impl->base.trigger = adc_buffer_trigger;
    impl->base.register_callback = adc_buffer_register_callback;
    impl->base.get_buffer = adc_buffer_get_buffer;
    impl->base.get_buffer_size = adc_buffer_get_buffer_size;
    impl->base.get_lifecycle = adc_buffer_get_lifecycle;
    impl->base.get_power = adc_buffer_get_power;

    /* Initialize interfaces (implemented in separate files) */
    adc_buffer_init_lifecycle(&impl->lifecycle);
    adc_buffer_init_power(&impl->power);

    /* Link to state */
    impl->state = &g_adc_buffer_states[index];
    impl->state->index = index;
    impl->state->initialized = false;
    impl->state->clock_enabled = false;
    impl->state->sampling_active = false;
    impl->state->current_index = 0;
    impl->state->callback = NULL;
    impl->state->user_data = NULL;

    /* Set configuration from Kconfig */
    if (platform_cfg != NULL) {
        impl->state->channel_count = platform_cfg->channel_count;

        /* Allocate buffer (ensure size is multiple of channel count) */
        size_t aligned_size =
            ((platform_cfg->buffer_size + platform_cfg->channel_count - 1) /
             platform_cfg->channel_count) *
            platform_cfg->channel_count;
        impl->state->buffer =
            (uint32_t*)malloc(aligned_size * sizeof(uint32_t));
        impl->state->buffer_size = aligned_size;

        if (impl->state->buffer) {
            memset(impl->state->buffer, 0, aligned_size * sizeof(uint32_t));
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
static void* nx_adc_buffer_device_init(const nx_device_t* dev) {
    const nx_adc_buffer_platform_config_t* config =
        (const nx_adc_buffer_platform_config_t*)dev->config;

    if (config == NULL || config->adc_index >= NX_ADC_BUFFER_MAX_INSTANCES) {
        return NULL;
    }

    nx_adc_buffer_impl_t* impl = &g_adc_buffer_instances[config->adc_index];

    /* Initialize instance with platform configuration */
    adc_buffer_init_instance(impl, config->adc_index, config);

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
#define NX_ADC_BUFFER_CONFIG(index)                                            \
    static const nx_adc_buffer_platform_config_t adc_buffer_config_##index = { \
        .adc_index = index,                                                    \
        .channel_count = CONFIG_ADC_BUFFER##index##_CHANNEL_COUNT,             \
        .buffer_size = CONFIG_ADC_BUFFER##index##_BUFFER_SIZE,                 \
    }

/**
 * \brief           Device registration macro
 */
#define NX_ADC_BUFFER_DEVICE_REGISTER(index)                                   \
    NX_ADC_BUFFER_CONFIG(index);                                               \
    static nx_device_config_state_t adc_buffer_kconfig_state_##index = {       \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(                                                        \
        DEVICE_TYPE, index, "ADC_BUFFER" #index, &adc_buffer_config_##index,   \
        &adc_buffer_kconfig_state_##index, nx_adc_buffer_device_init)

/* Register all enabled ADC buffer instances */
NX_TRAVERSE_EACH_INSTANCE(NX_ADC_BUFFER_DEVICE_REGISTER, DEVICE_TYPE);

/*---------------------------------------------------------------------------*/
/* Legacy Factory Functions (for backward compatibility)                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get ADC buffer instance (legacy)
 */
nx_adc_buffer_t* nx_adc_buffer_native_get(uint8_t index, size_t buffer_size) {
    if (index >= NX_ADC_BUFFER_MAX_INSTANCES) {
        return NULL;
    }

    /* Use device registration mechanism */
    char name[32];
    snprintf(name, sizeof(name), "ADC_BUFFER%d", index);

    (void)buffer_size; /* Buffer size is configured via Kconfig */

    return (nx_adc_buffer_t*)nx_device_get(name);
}

/**
 * \brief           Cleanup ADC buffer instance (for testing)
 */
void nx_adc_buffer_native_cleanup(uint8_t index) {
    if (index >= NX_ADC_BUFFER_MAX_INSTANCES) {
        return;
    }

    nx_adc_buffer_impl_t* impl = &g_adc_buffer_instances[index];
    if (impl->state) {
        if (impl->state->initialized) {
            impl->lifecycle.deinit(&impl->lifecycle);
        }
        if (impl->state->buffer) {
            free(impl->state->buffer);
            impl->state->buffer = NULL;
        }
    }
    memset(&g_adc_buffer_states[index], 0, sizeof(nx_adc_buffer_state_t));
}
