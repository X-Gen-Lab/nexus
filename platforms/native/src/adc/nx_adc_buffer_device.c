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

#define DEVICE_TYPE NX_ADC_BUFFER

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

    /* Allocate and initialize state */
    impl->state =
        (nx_adc_buffer_state_t*)nx_mem_alloc(sizeof(nx_adc_buffer_state_t));
    if (!impl->state) {
        return;
    }
    memset(impl->state, 0, sizeof(nx_adc_buffer_state_t));

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
            (uint32_t*)nx_mem_alloc(aligned_size * sizeof(uint32_t));
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

    if (config == NULL) {
        return NULL;
    }

    /* Allocate implementation structure */
    nx_adc_buffer_impl_t* impl =
        (nx_adc_buffer_impl_t*)nx_mem_alloc(sizeof(nx_adc_buffer_impl_t));
    if (!impl) {
        return NULL;
    }
    memset(impl, 0, sizeof(nx_adc_buffer_impl_t));

    /* Initialize instance with platform configuration */
    adc_buffer_init_instance(impl, config->adc_index, config);

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
#define NX_ADC_BUFFER_CONFIG(index)                                            \
    static const nx_adc_buffer_platform_config_t adc_buffer_config_##index = { \
        .adc_index = index,                                                    \
        .channel_count = NX_CONFIG_ADC_BUFFER##index##_CHANNEL_COUNT,          \
        .buffer_size = NX_CONFIG_ADC_BUFFER##index##_BUFFER_SIZE,              \
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
        &adc_buffer_kconfig_state_##index, nx_adc_buffer_device_init);

/**
 * \brief           Register all enabled ADC buffer instances
 */
NX_TRAVERSE_EACH_INSTANCE(NX_ADC_BUFFER_DEVICE_REGISTER, DEVICE_TYPE);
