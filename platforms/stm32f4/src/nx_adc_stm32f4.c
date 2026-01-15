/**
 * \file            nx_adc_stm32f4.c
 * \brief           STM32F4 ADC driver implementation
 * \author          Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_adc.h"
#include "hal/resource/nx_dma_manager.h"
#include <stddef.h>
#include <string.h>

/* Maximum number of ADC instances */
#define NX_ADC_MAX_COUNT 3

/* Maximum number of ADC channels */
#define NX_ADC_MAX_CHANNELS 16

/* ADC buffer size for DMA */
#define NX_ADC_DMA_BUFFER_SIZE 256

/**
 * \brief           ADC state structure (internal)
 */
typedef struct {
    uint8_t adc_index;                           /**< ADC index */
    bool initialized;                            /**< Initialization flag */
    bool busy;                                   /**< Busy flag */
    nx_adc_config_t config;                      /**< Current configuration */
    nx_adc_callback_t callback;                  /**< Conversion callback */
    void* callback_ctx;                          /**< Callback context */
    uint32_t conversion_count;                   /**< Conversion counter */
    uint32_t overrun_count;                      /**< Overrun counter */
    uint32_t dma_error_count;                    /**< DMA error counter */
    uint32_t vref_mv;                            /**< Reference voltage in mV */
    uint16_t dma_buffer[NX_ADC_DMA_BUFFER_SIZE]; /**< DMA buffer */
    nx_dma_channel_t* dma_channel;               /**< DMA channel handle */
} nx_adc_state_t;

/**
 * \brief           ADC device implementation structure
 */
typedef struct {
    nx_adc_t base;              /**< Base ADC interface */
    nx_lifecycle_t lifecycle;   /**< Lifecycle interface */
    nx_power_t power;           /**< Power interface */
    nx_diagnostic_t diagnostic; /**< Diagnostic interface */
    nx_adc_state_t* adc_state;  /**< ADC state pointer */
    nx_device_t* device;        /**< Device descriptor */
} nx_adc_impl_t;

/* Forward declarations - ADC operations */
static nx_status_t adc_read(nx_adc_t* self, uint8_t channel, uint16_t* value);
static nx_status_t adc_read_voltage(nx_adc_t* self, uint8_t channel,
                                    uint32_t* voltage_mv);
static nx_status_t adc_read_multi(nx_adc_t* self, uint8_t* channels,
                                  uint8_t count, uint16_t* values);
static nx_status_t adc_start_continuous(nx_adc_t* self);
static nx_status_t adc_stop_continuous(nx_adc_t* self);
static nx_status_t adc_get_buffer(nx_adc_t* self, uint16_t* buffer,
                                  size_t* count);
static nx_status_t adc_set_callback(nx_adc_t* self, nx_adc_callback_t cb,
                                    void* ctx);
static nx_status_t adc_clear_callback(nx_adc_t* self);
static nx_status_t adc_calibrate(nx_adc_t* self);
static nx_status_t adc_set_reference_voltage(nx_adc_t* self, uint32_t vref_mv);
static nx_status_t adc_set_resolution(nx_adc_t* self,
                                      nx_adc_resolution_t resolution);
static nx_status_t adc_set_sampling_time(nx_adc_t* self,
                                         nx_adc_sampling_time_t time);
static nx_status_t adc_get_config(nx_adc_t* self, nx_adc_config_t* cfg);
static nx_status_t adc_set_config(nx_adc_t* self, const nx_adc_config_t* cfg);
static nx_lifecycle_t* adc_get_lifecycle(nx_adc_t* self);
static nx_power_t* adc_get_power(nx_adc_t* self);
static nx_diagnostic_t* adc_get_diagnostic(nx_adc_t* self);
static nx_status_t adc_get_stats(nx_adc_t* self, nx_adc_stats_t* stats);
static nx_status_t adc_clear_stats(nx_adc_t* self);

/* Forward declarations - Lifecycle operations */
static nx_status_t adc_lifecycle_init(nx_lifecycle_t* self);
static nx_status_t adc_lifecycle_deinit(nx_lifecycle_t* self);
static nx_status_t adc_lifecycle_suspend(nx_lifecycle_t* self);
static nx_status_t adc_lifecycle_resume(nx_lifecycle_t* self);
static nx_device_state_t adc_lifecycle_get_state(nx_lifecycle_t* self);

/* Forward declarations - Power operations */
static nx_status_t adc_power_enable(nx_power_t* self);
static nx_status_t adc_power_disable(nx_power_t* self);
static bool adc_power_is_enabled(nx_power_t* self);

/* Forward declarations - Diagnostic operations */
static nx_status_t adc_diagnostic_get_status(nx_diagnostic_t* self,
                                             void* status, size_t size);
static nx_status_t adc_diagnostic_get_statistics(nx_diagnostic_t* self,
                                                 void* stats, size_t size);
static nx_status_t adc_diagnostic_clear_statistics(nx_diagnostic_t* self);

/* ADC state storage */
static nx_adc_state_t g_adc_states[NX_ADC_MAX_COUNT];

/* ADC implementation instances */
static nx_adc_impl_t g_adc_instances[NX_ADC_MAX_COUNT];

/**
 * \brief           Get ADC implementation from base interface
 */
static nx_adc_impl_t* adc_get_impl(nx_adc_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_adc_impl_t*)self;
}

/**
 * \brief           Get ADC implementation from lifecycle interface
 */
static nx_adc_impl_t* adc_get_impl_from_lifecycle(nx_lifecycle_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_adc_impl_t*)((char*)self - offsetof(nx_adc_impl_t, lifecycle));
}

/**
 * \brief           Get ADC implementation from power interface
 */
static nx_adc_impl_t* adc_get_impl_from_power(nx_power_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_adc_impl_t*)((char*)self - offsetof(nx_adc_impl_t, power));
}

/**
 * \brief           Get ADC implementation from diagnostic interface
 */
static nx_adc_impl_t* adc_get_impl_from_diagnostic(nx_diagnostic_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_adc_impl_t*)((char*)self - offsetof(nx_adc_impl_t, diagnostic));
}

/* Hardware-specific functions (stubs for now) */
static void hw_adc_enable_clock(uint8_t adc_index) {
    (void)adc_index;
}

static void hw_adc_disable_clock(uint8_t adc_index) {
    (void)adc_index;
}

static void hw_adc_configure(uint8_t adc_index, const nx_adc_config_t* cfg) {
    (void)adc_index;
    (void)cfg;
}

static uint16_t hw_adc_read_channel(uint8_t adc_index, uint8_t channel) {
    (void)adc_index;
    (void)channel;
    return 0;
}

static void hw_adc_start_continuous(uint8_t adc_index) {
    (void)adc_index;
}

static void hw_adc_stop_continuous(uint8_t adc_index) {
    (void)adc_index;
}

static void hw_adc_calibrate(uint8_t adc_index) {
    (void)adc_index;
}

/* ADC operations implementation */
static nx_status_t adc_read(nx_adc_t* self, uint8_t channel, uint16_t* value) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl || !impl->adc_state || !value) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (channel >= NX_ADC_MAX_CHANNELS) {
        return NX_ERR_INVALID_PARAM;
    }
    *value = hw_adc_read_channel(impl->adc_state->adc_index, channel);
    impl->adc_state->conversion_count++;
    return NX_OK;
}

static nx_status_t adc_read_voltage(nx_adc_t* self, uint8_t channel,
                                    uint32_t* voltage_mv) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl || !impl->adc_state || !voltage_mv) {
        return NX_ERR_NULL_PTR;
    }
    uint16_t raw_value;
    nx_status_t status = adc_read(self, channel, &raw_value);
    if (status != NX_OK) {
        return status;
    }
    /* Convert raw value to voltage based on resolution and reference voltage */
    uint32_t max_value =
        (1 << (6 + impl->adc_state->config.resolution * 2)) - 1;
    *voltage_mv = (raw_value * impl->adc_state->vref_mv) / max_value;
    return NX_OK;
}

static nx_status_t adc_read_multi(nx_adc_t* self, uint8_t* channels,
                                  uint8_t count, uint16_t* values) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl || !impl->adc_state || !channels || !values) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    for (uint8_t i = 0; i < count; i++) {
        if (channels[i] >= NX_ADC_MAX_CHANNELS) {
            return NX_ERR_INVALID_PARAM;
        }
        values[i] =
            hw_adc_read_channel(impl->adc_state->adc_index, channels[i]);
    }
    impl->adc_state->conversion_count += count;
    return NX_OK;
}

static nx_status_t adc_start_continuous(nx_adc_t* self) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl || !impl->adc_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (impl->adc_state->busy) {
        return NX_ERR_BUSY;
    }
    hw_adc_start_continuous(impl->adc_state->adc_index);
    impl->adc_state->busy = true;
    return NX_OK;
}

static nx_status_t adc_stop_continuous(nx_adc_t* self) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl || !impl->adc_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    hw_adc_stop_continuous(impl->adc_state->adc_index);
    impl->adc_state->busy = false;
    return NX_OK;
}

static nx_status_t adc_get_buffer(nx_adc_t* self, uint16_t* buffer,
                                  size_t* count) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl || !impl->adc_state || !buffer || !count) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    size_t copy_count =
        (*count < NX_ADC_DMA_BUFFER_SIZE) ? *count : NX_ADC_DMA_BUFFER_SIZE;
    memcpy(buffer, impl->adc_state->dma_buffer, copy_count * sizeof(uint16_t));
    *count = copy_count;
    return NX_OK;
}

static nx_status_t adc_set_callback(nx_adc_t* self, nx_adc_callback_t cb,
                                    void* ctx) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl || !impl->adc_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    impl->adc_state->callback = cb;
    impl->adc_state->callback_ctx = ctx;
    return NX_OK;
}

static nx_status_t adc_clear_callback(nx_adc_t* self) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl || !impl->adc_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    impl->adc_state->callback = NULL;
    impl->adc_state->callback_ctx = NULL;
    return NX_OK;
}

static nx_status_t adc_calibrate(nx_adc_t* self) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl || !impl->adc_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    hw_adc_calibrate(impl->adc_state->adc_index);
    return NX_OK;
}

static nx_status_t adc_set_reference_voltage(nx_adc_t* self, uint32_t vref_mv) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl || !impl->adc_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    impl->adc_state->vref_mv = vref_mv;
    return NX_OK;
}

static nx_status_t adc_set_resolution(nx_adc_t* self,
                                      nx_adc_resolution_t resolution) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl || !impl->adc_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    impl->adc_state->config.resolution = resolution;
    hw_adc_configure(impl->adc_state->adc_index, &impl->adc_state->config);
    return NX_OK;
}

static nx_status_t adc_set_sampling_time(nx_adc_t* self,
                                         nx_adc_sampling_time_t time) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl || !impl->adc_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    impl->adc_state->config.sampling_time = time;
    hw_adc_configure(impl->adc_state->adc_index, &impl->adc_state->config);
    return NX_OK;
}

static nx_status_t adc_get_config(nx_adc_t* self, nx_adc_config_t* cfg) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl || !impl->adc_state || !cfg) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    memcpy(cfg, &impl->adc_state->config, sizeof(nx_adc_config_t));
    return NX_OK;
}

static nx_status_t adc_set_config(nx_adc_t* self, const nx_adc_config_t* cfg) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl || !impl->adc_state || !cfg) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    hw_adc_configure(impl->adc_state->adc_index, cfg);
    memcpy(&impl->adc_state->config, cfg, sizeof(nx_adc_config_t));
    return NX_OK;
}

/* Base interface getters */
static nx_lifecycle_t* adc_get_lifecycle(nx_adc_t* self) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

static nx_power_t* adc_get_power(nx_adc_t* self) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    return impl ? &impl->power : NULL;
}

static nx_diagnostic_t* adc_get_diagnostic(nx_adc_t* self) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    return impl ? &impl->diagnostic : NULL;
}

/* Diagnostics */
static nx_status_t adc_get_stats(nx_adc_t* self, nx_adc_stats_t* stats) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl || !impl->adc_state || !stats) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    stats->busy = impl->adc_state->busy;
    stats->conversion_count = impl->adc_state->conversion_count;
    stats->overrun_count = impl->adc_state->overrun_count;
    stats->dma_error_count = impl->adc_state->dma_error_count;
    return NX_OK;
}

static nx_status_t adc_clear_stats(nx_adc_t* self) {
    nx_adc_impl_t* impl = adc_get_impl(self);
    if (!impl || !impl->adc_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    impl->adc_state->conversion_count = 0;
    impl->adc_state->overrun_count = 0;
    impl->adc_state->dma_error_count = 0;
    return NX_OK;
}

/* Lifecycle operations */
static nx_status_t adc_lifecycle_init(nx_lifecycle_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_lifecycle(self);
    if (!impl || !impl->adc_state) {
        return NX_ERR_NULL_PTR;
    }
    if (impl->adc_state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }
    hw_adc_enable_clock(impl->adc_state->adc_index);
    hw_adc_configure(impl->adc_state->adc_index, &impl->adc_state->config);
    impl->adc_state->initialized = true;
    return NX_OK;
}

static nx_status_t adc_lifecycle_deinit(nx_lifecycle_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_lifecycle(self);
    if (!impl || !impl->adc_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (impl->adc_state->busy) {
        hw_adc_stop_continuous(impl->adc_state->adc_index);
    }
    if (impl->adc_state->dma_channel) {
        nx_dma_manager_t* dma_mgr = nx_dma_manager_get();
        if (dma_mgr) {
            dma_mgr->stop(impl->adc_state->dma_channel);
            dma_mgr->free(dma_mgr, impl->adc_state->dma_channel);
            impl->adc_state->dma_channel = NULL;
        }
    }
    hw_adc_disable_clock(impl->adc_state->adc_index);
    impl->adc_state->initialized = false;
    impl->adc_state->busy = false;
    return NX_OK;
}

static nx_status_t adc_lifecycle_suspend(nx_lifecycle_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_lifecycle(self);
    if (!impl || !impl->adc_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (impl->adc_state->busy) {
        hw_adc_stop_continuous(impl->adc_state->adc_index);
    }
    hw_adc_disable_clock(impl->adc_state->adc_index);
    return NX_OK;
}

static nx_status_t adc_lifecycle_resume(nx_lifecycle_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_lifecycle(self);
    if (!impl || !impl->adc_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->adc_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    hw_adc_enable_clock(impl->adc_state->adc_index);
    hw_adc_configure(impl->adc_state->adc_index, &impl->adc_state->config);
    if (impl->adc_state->busy) {
        hw_adc_start_continuous(impl->adc_state->adc_index);
    }
    return NX_OK;
}

static nx_device_state_t adc_lifecycle_get_state(nx_lifecycle_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_lifecycle(self);
    if (!impl || !impl->adc_state) {
        return NX_DEV_STATE_ERROR;
    }
    if (!impl->adc_state->initialized) {
        return NX_DEV_STATE_UNINITIALIZED;
    }
    return impl->adc_state->busy ? NX_DEV_STATE_RUNNING
                                 : NX_DEV_STATE_INITIALIZED;
}

/* Power operations */
static nx_status_t adc_power_enable(nx_power_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_power(self);
    if (!impl || !impl->adc_state) {
        return NX_ERR_NULL_PTR;
    }
    hw_adc_enable_clock(impl->adc_state->adc_index);
    return NX_OK;
}

static nx_status_t adc_power_disable(nx_power_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_power(self);
    if (!impl || !impl->adc_state) {
        return NX_ERR_NULL_PTR;
    }
    hw_adc_disable_clock(impl->adc_state->adc_index);
    return NX_OK;
}

static bool adc_power_is_enabled(nx_power_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_power(self);
    if (!impl || !impl->adc_state) {
        return false;
    }
    return impl->adc_state->initialized;
}

/* Diagnostic operations */
static nx_status_t adc_diagnostic_get_status(nx_diagnostic_t* self,
                                             void* status, size_t size) {
    nx_adc_impl_t* impl = adc_get_impl_from_diagnostic(self);
    if (!impl || !impl->adc_state || !status) {
        return NX_ERR_NULL_PTR;
    }
    if (size < sizeof(nx_adc_stats_t)) {
        return NX_ERR_DATA_SIZE;
    }
    return adc_get_stats(&impl->base, (nx_adc_stats_t*)status);
}

static nx_status_t adc_diagnostic_get_statistics(nx_diagnostic_t* self,
                                                 void* stats, size_t size) {
    return adc_diagnostic_get_status(self, stats, size);
}

static nx_status_t adc_diagnostic_clear_statistics(nx_diagnostic_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_diagnostic(self);
    if (!impl) {
        return NX_ERR_NULL_PTR;
    }
    return adc_clear_stats(&impl->base);
}

/**
 * \brief           Initialize ADC instance
 */
static void adc_init_instance(nx_adc_impl_t* impl, uint8_t adc_index) {
    /* Initialize base interface */
    impl->base.read = adc_read;
    impl->base.read_voltage = adc_read_voltage;
    impl->base.read_multi = adc_read_multi;
    impl->base.start_continuous = adc_start_continuous;
    impl->base.stop_continuous = adc_stop_continuous;
    impl->base.get_buffer = adc_get_buffer;
    impl->base.set_callback = adc_set_callback;
    impl->base.clear_callback = adc_clear_callback;
    impl->base.calibrate = adc_calibrate;
    impl->base.set_reference_voltage = adc_set_reference_voltage;
    impl->base.set_resolution = adc_set_resolution;
    impl->base.set_sampling_time = adc_set_sampling_time;
    impl->base.get_config = adc_get_config;
    impl->base.set_config = adc_set_config;
    impl->base.get_lifecycle = adc_get_lifecycle;
    impl->base.get_power = adc_get_power;
    impl->base.get_diagnostic = adc_get_diagnostic;
    impl->base.get_stats = adc_get_stats;
    impl->base.clear_stats = adc_clear_stats;

    /* Initialize lifecycle interface */
    impl->lifecycle.init = adc_lifecycle_init;
    impl->lifecycle.deinit = adc_lifecycle_deinit;
    impl->lifecycle.suspend = adc_lifecycle_suspend;
    impl->lifecycle.resume = adc_lifecycle_resume;
    impl->lifecycle.get_state = adc_lifecycle_get_state;

    /* Initialize power interface */
    impl->power.enable = adc_power_enable;
    impl->power.disable = adc_power_disable;
    impl->power.is_enabled = adc_power_is_enabled;

    /* Initialize diagnostic interface */
    impl->diagnostic.get_status = adc_diagnostic_get_status;
    impl->diagnostic.get_statistics = adc_diagnostic_get_statistics;
    impl->diagnostic.clear_statistics = adc_diagnostic_clear_statistics;

    /* Link to ADC state */
    impl->adc_state = &g_adc_states[adc_index];
    impl->adc_state->adc_index = adc_index;
    impl->adc_state->initialized = false;
    impl->adc_state->busy = false;
    impl->adc_state->callback = NULL;
    impl->adc_state->callback_ctx = NULL;
    impl->adc_state->conversion_count = 0;
    impl->adc_state->overrun_count = 0;
    impl->adc_state->dma_error_count = 0;
    impl->adc_state->vref_mv = 3300; /* Default 3.3V */
    impl->adc_state->dma_channel = NULL;

    /* Set default configuration */
    impl->adc_state->config.resolution = NX_ADC_RESOLUTION_12BIT;
    impl->adc_state->config.sampling_time = NX_ADC_SAMPLING_3_CYCLES;
    impl->adc_state->config.trigger = NX_ADC_TRIGGER_SOFTWARE;
    impl->adc_state->config.continuous_mode = false;
    impl->adc_state->config.dma_enable = false;
    impl->adc_state->config.channel_count = 0;
    impl->adc_state->config.channels = NULL;
}

/**
 * \brief           Get ADC instance (factory function)
 * \param[in]       adc_index: ADC index (0-2)
 * \return          ADC interface pointer, NULL on failure
 */
nx_adc_t* nx_adc_stm32f4_get(uint8_t adc_index) {
    if (adc_index >= NX_ADC_MAX_COUNT) {
        return NULL;
    }

    nx_adc_impl_t* impl = &g_adc_instances[adc_index];

    /* Initialize instance if not already done */
    if (!impl->adc_state) {
        adc_init_instance(impl, adc_index);
    }

    return &impl->base;
}

/**
 * \brief           Get ADC instance with configuration
 * \param[in]       adc_index: ADC index (0-2)
 * \param[in]       cfg: ADC configuration
 * \return          ADC interface pointer, NULL on failure
 */
nx_adc_t* nx_adc_stm32f4_get_with_config(uint8_t adc_index,
                                         const nx_adc_config_t* cfg) {
    nx_adc_t* adc = nx_adc_stm32f4_get(adc_index);

    if (!adc || !cfg) {
        return NULL;
    }

    /* Apply configuration */
    nx_adc_impl_t* impl = adc_get_impl(adc);
    if (impl && impl->adc_state) {
        memcpy(&impl->adc_state->config, cfg, sizeof(nx_adc_config_t));
    }

    return adc;
}

/**
 * \brief           Get ADC device descriptor
 * \param[in]       index: ADC index
 * \return          Device descriptor pointer, NULL on failure
 */
nx_device_t* nx_adc_stm32f4_get_device(uint8_t index) {
    if (index >= NX_ADC_MAX_COUNT) {
        return NULL;
    }

    nx_adc_impl_t* impl = &g_adc_instances[index];
    return impl->device;
}
