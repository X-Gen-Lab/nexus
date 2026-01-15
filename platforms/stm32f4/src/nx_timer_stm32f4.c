/**
 * \file            nx_timer_stm32f4.c
 * \brief           STM32F4 Timer driver implementation
 * \author          Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_timer.h"
#include <stddef.h>
#include <string.h>

/* Maximum number of timers */
#define NX_TIMER_MAX_COUNT 14

/* Maximum PWM channels per timer */
#define NX_TIMER_MAX_PWM_CHANNELS 4

/**
 * \brief           Timer state structure (internal)
 */
typedef struct {
    uint8_t timer_index;          /**< Timer index */
    bool initialized;             /**< Initialization flag */
    bool running;                 /**< Running flag */
    nx_timer_config_t config;     /**< Current configuration */
    nx_timer_callback_t callback; /**< Timer callback */
    void* callback_ctx;           /**< Callback context */
    nx_pwm_config_t pwm_configs[NX_TIMER_MAX_PWM_CHANNELS]; /**< PWM configs */
    uint32_t overflow_count; /**< Overflow counter */
    uint32_t capture_count;  /**< Capture counter */
    uint32_t compare_count;  /**< Compare counter */
} nx_timer_state_t;

/**
 * \brief           Timer device implementation structure
 */
typedef struct {
    nx_timer_t base;               /**< Base timer interface */
    nx_lifecycle_t lifecycle;      /**< Lifecycle interface */
    nx_power_t power;              /**< Power interface */
    nx_diagnostic_t diagnostic;    /**< Diagnostic interface */
    nx_timer_state_t* timer_state; /**< Timer state pointer */
    nx_device_t* device;           /**< Device descriptor */
} nx_timer_impl_t;

/* Forward declarations - Timer operations */
static nx_status_t timer_start(nx_timer_t* self);
static nx_status_t timer_stop(nx_timer_t* self);
static nx_status_t timer_reset(nx_timer_t* self);
static uint32_t timer_get_counter(nx_timer_t* self);
static nx_status_t timer_set_counter(nx_timer_t* self, uint32_t value);
static nx_status_t timer_set_callback(nx_timer_t* self, nx_timer_callback_t cb,
                                      void* ctx);
static nx_status_t timer_clear_callback(nx_timer_t* self);
static nx_status_t timer_pwm_start(nx_timer_t* self, uint8_t channel);
static nx_status_t timer_pwm_stop(nx_timer_t* self, uint8_t channel);
static nx_status_t timer_pwm_set_duty_cycle(nx_timer_t* self, uint8_t channel,
                                            uint8_t duty_cycle);
static nx_status_t timer_pwm_get_config(nx_timer_t* self, uint8_t channel,
                                        nx_pwm_config_t* cfg);
static nx_status_t timer_pwm_set_config(nx_timer_t* self, uint8_t channel,
                                        const nx_pwm_config_t* cfg);
static nx_status_t timer_set_frequency(nx_timer_t* self, uint32_t frequency_hz);
static nx_status_t timer_set_period(nx_timer_t* self, uint32_t period_us);
static nx_status_t timer_get_config(nx_timer_t* self, nx_timer_config_t* cfg);
static nx_status_t timer_set_config(nx_timer_t* self,
                                    const nx_timer_config_t* cfg);
static nx_lifecycle_t* timer_get_lifecycle(nx_timer_t* self);
static nx_power_t* timer_get_power(nx_timer_t* self);
static nx_diagnostic_t* timer_get_diagnostic(nx_timer_t* self);
static nx_status_t timer_get_stats(nx_timer_t* self, nx_timer_stats_t* stats);
static nx_status_t timer_clear_stats(nx_timer_t* self);

/* Forward declarations - Lifecycle operations */
static nx_status_t timer_lifecycle_init(nx_lifecycle_t* self);
static nx_status_t timer_lifecycle_deinit(nx_lifecycle_t* self);
static nx_status_t timer_lifecycle_suspend(nx_lifecycle_t* self);
static nx_status_t timer_lifecycle_resume(nx_lifecycle_t* self);
static nx_device_state_t timer_lifecycle_get_state(nx_lifecycle_t* self);

/* Forward declarations - Power operations */
static nx_status_t timer_power_enable(nx_power_t* self);
static nx_status_t timer_power_disable(nx_power_t* self);
static bool timer_power_is_enabled(nx_power_t* self);

/* Forward declarations - Diagnostic operations */
static nx_status_t timer_diagnostic_get_status(nx_diagnostic_t* self,
                                               void* status, size_t size);
static nx_status_t timer_diagnostic_get_statistics(nx_diagnostic_t* self,
                                                   void* stats, size_t size);
static nx_status_t timer_diagnostic_clear_statistics(nx_diagnostic_t* self);

/* Timer state storage */
static nx_timer_state_t g_timer_states[NX_TIMER_MAX_COUNT];

/* Timer implementation instances */
static nx_timer_impl_t g_timer_instances[NX_TIMER_MAX_COUNT];

/**
 * \brief           Get timer implementation from base interface
 */
static nx_timer_impl_t* timer_get_impl(nx_timer_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_timer_impl_t*)self;
}

/**
 * \brief           Get timer implementation from lifecycle interface
 */
static nx_timer_impl_t* timer_get_impl_from_lifecycle(nx_lifecycle_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_timer_impl_t*)((char*)self -
                              offsetof(nx_timer_impl_t, lifecycle));
}

/**
 * \brief           Get timer implementation from power interface
 */
static nx_timer_impl_t* timer_get_impl_from_power(nx_power_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_timer_impl_t*)((char*)self - offsetof(nx_timer_impl_t, power));
}

/**
 * \brief           Get timer implementation from diagnostic interface
 */
static nx_timer_impl_t* timer_get_impl_from_diagnostic(nx_diagnostic_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_timer_impl_t*)((char*)self -
                              offsetof(nx_timer_impl_t, diagnostic));
}

/* Hardware-specific functions (stubs for now) */
static void hw_timer_enable_clock(uint8_t timer_index) {
    (void)timer_index;
}

static void hw_timer_disable_clock(uint8_t timer_index) {
    (void)timer_index;
}

static void hw_timer_start(uint8_t timer_index) {
    (void)timer_index;
}

static void hw_timer_stop(uint8_t timer_index) {
    (void)timer_index;
}

static void hw_timer_reset(uint8_t timer_index) {
    (void)timer_index;
}

static uint32_t hw_timer_get_counter(uint8_t timer_index) {
    (void)timer_index;
    return 0;
}

static void hw_timer_set_counter(uint8_t timer_index, uint32_t value) {
    (void)timer_index;
    (void)value;
}

static void hw_timer_configure(uint8_t timer_index,
                               const nx_timer_config_t* cfg) {
    (void)timer_index;
    (void)cfg;
}

static void hw_pwm_start(uint8_t timer_index, uint8_t channel) {
    (void)timer_index;
    (void)channel;
}

static void hw_pwm_stop(uint8_t timer_index, uint8_t channel) {
    (void)timer_index;
    (void)channel;
}

static void hw_pwm_set_duty_cycle(uint8_t timer_index, uint8_t channel,
                                  uint8_t duty_cycle) {
    (void)timer_index;
    (void)channel;
    (void)duty_cycle;
}

static void hw_pwm_configure(uint8_t timer_index, uint8_t channel,
                             const nx_pwm_config_t* cfg) {
    (void)timer_index;
    (void)channel;
    (void)cfg;
}

/* Timer operations implementation */
static nx_status_t timer_start(nx_timer_t* self) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    hw_timer_start(impl->timer_state->timer_index);
    impl->timer_state->running = true;
    return NX_OK;
}

static nx_status_t timer_stop(nx_timer_t* self) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    hw_timer_stop(impl->timer_state->timer_index);
    impl->timer_state->running = false;
    return NX_OK;
}

static nx_status_t timer_reset(nx_timer_t* self) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    hw_timer_reset(impl->timer_state->timer_index);
    return NX_OK;
}

static uint32_t timer_get_counter(nx_timer_t* self) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state || !impl->timer_state->initialized) {
        return 0;
    }
    return hw_timer_get_counter(impl->timer_state->timer_index);
}

static nx_status_t timer_set_counter(nx_timer_t* self, uint32_t value) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    hw_timer_set_counter(impl->timer_state->timer_index, value);
    return NX_OK;
}

static nx_status_t timer_set_callback(nx_timer_t* self, nx_timer_callback_t cb,
                                      void* ctx) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    impl->timer_state->callback = cb;
    impl->timer_state->callback_ctx = ctx;
    return NX_OK;
}

static nx_status_t timer_clear_callback(nx_timer_t* self) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    impl->timer_state->callback = NULL;
    impl->timer_state->callback_ctx = NULL;
    return NX_OK;
}

/* PWM operations */
static nx_status_t timer_pwm_start(nx_timer_t* self, uint8_t channel) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (channel >= NX_TIMER_MAX_PWM_CHANNELS) {
        return NX_ERR_INVALID_PARAM;
    }
    hw_pwm_start(impl->timer_state->timer_index, channel);
    return NX_OK;
}

static nx_status_t timer_pwm_stop(nx_timer_t* self, uint8_t channel) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (channel >= NX_TIMER_MAX_PWM_CHANNELS) {
        return NX_ERR_INVALID_PARAM;
    }
    hw_pwm_stop(impl->timer_state->timer_index, channel);
    return NX_OK;
}

static nx_status_t timer_pwm_set_duty_cycle(nx_timer_t* self, uint8_t channel,
                                            uint8_t duty_cycle) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (channel >= NX_TIMER_MAX_PWM_CHANNELS || duty_cycle > 100) {
        return NX_ERR_INVALID_PARAM;
    }
    hw_pwm_set_duty_cycle(impl->timer_state->timer_index, channel, duty_cycle);
    impl->timer_state->pwm_configs[channel].duty_cycle = duty_cycle;
    return NX_OK;
}

static nx_status_t timer_pwm_get_config(nx_timer_t* self, uint8_t channel,
                                        nx_pwm_config_t* cfg) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state || !cfg) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (channel >= NX_TIMER_MAX_PWM_CHANNELS) {
        return NX_ERR_INVALID_PARAM;
    }
    memcpy(cfg, &impl->timer_state->pwm_configs[channel],
           sizeof(nx_pwm_config_t));
    return NX_OK;
}

static nx_status_t timer_pwm_set_config(nx_timer_t* self, uint8_t channel,
                                        const nx_pwm_config_t* cfg) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state || !cfg) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (channel >= NX_TIMER_MAX_PWM_CHANNELS) {
        return NX_ERR_INVALID_PARAM;
    }
    hw_pwm_configure(impl->timer_state->timer_index, channel, cfg);
    memcpy(&impl->timer_state->pwm_configs[channel], cfg,
           sizeof(nx_pwm_config_t));
    return NX_OK;
}

/* Runtime configuration */
static nx_status_t timer_set_frequency(nx_timer_t* self,
                                       uint32_t frequency_hz) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    impl->timer_state->config.frequency_hz = frequency_hz;
    hw_timer_configure(impl->timer_state->timer_index,
                       &impl->timer_state->config);
    return NX_OK;
}

static nx_status_t timer_set_period(nx_timer_t* self, uint32_t period_us) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    impl->timer_state->config.period_us = period_us;
    hw_timer_configure(impl->timer_state->timer_index,
                       &impl->timer_state->config);
    return NX_OK;
}

static nx_status_t timer_get_config(nx_timer_t* self, nx_timer_config_t* cfg) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state || !cfg) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    memcpy(cfg, &impl->timer_state->config, sizeof(nx_timer_config_t));
    return NX_OK;
}

static nx_status_t timer_set_config(nx_timer_t* self,
                                    const nx_timer_config_t* cfg) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state || !cfg) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    hw_timer_configure(impl->timer_state->timer_index, cfg);
    memcpy(&impl->timer_state->config, cfg, sizeof(nx_timer_config_t));
    return NX_OK;
}

/* Base interface getters */
static nx_lifecycle_t* timer_get_lifecycle(nx_timer_t* self) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

static nx_power_t* timer_get_power(nx_timer_t* self) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    return impl ? &impl->power : NULL;
}

static nx_diagnostic_t* timer_get_diagnostic(nx_timer_t* self) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    return impl ? &impl->diagnostic : NULL;
}

/* Diagnostics */
static nx_status_t timer_get_stats(nx_timer_t* self, nx_timer_stats_t* stats) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state || !stats) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    stats->running = impl->timer_state->running;
    stats->overflow_count = impl->timer_state->overflow_count;
    stats->capture_count = impl->timer_state->capture_count;
    stats->compare_count = impl->timer_state->compare_count;
    return NX_OK;
}

static nx_status_t timer_clear_stats(nx_timer_t* self) {
    nx_timer_impl_t* impl = timer_get_impl(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    impl->timer_state->overflow_count = 0;
    impl->timer_state->capture_count = 0;
    impl->timer_state->compare_count = 0;
    return NX_OK;
}

/* Lifecycle operations */
static nx_status_t timer_lifecycle_init(nx_lifecycle_t* self) {
    nx_timer_impl_t* impl = timer_get_impl_from_lifecycle(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    if (impl->timer_state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }
    hw_timer_enable_clock(impl->timer_state->timer_index);
    hw_timer_configure(impl->timer_state->timer_index,
                       &impl->timer_state->config);
    impl->timer_state->initialized = true;
    return NX_OK;
}

static nx_status_t timer_lifecycle_deinit(nx_lifecycle_t* self) {
    nx_timer_impl_t* impl = timer_get_impl_from_lifecycle(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    hw_timer_stop(impl->timer_state->timer_index);
    hw_timer_disable_clock(impl->timer_state->timer_index);
    impl->timer_state->initialized = false;
    impl->timer_state->running = false;
    return NX_OK;
}

static nx_status_t timer_lifecycle_suspend(nx_lifecycle_t* self) {
    nx_timer_impl_t* impl = timer_get_impl_from_lifecycle(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    if (impl->timer_state->running) {
        hw_timer_stop(impl->timer_state->timer_index);
    }
    hw_timer_disable_clock(impl->timer_state->timer_index);
    return NX_OK;
}

static nx_status_t timer_lifecycle_resume(nx_lifecycle_t* self) {
    nx_timer_impl_t* impl = timer_get_impl_from_lifecycle(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->timer_state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    hw_timer_enable_clock(impl->timer_state->timer_index);
    hw_timer_configure(impl->timer_state->timer_index,
                       &impl->timer_state->config);
    if (impl->timer_state->running) {
        hw_timer_start(impl->timer_state->timer_index);
    }
    return NX_OK;
}

static nx_device_state_t timer_lifecycle_get_state(nx_lifecycle_t* self) {
    nx_timer_impl_t* impl = timer_get_impl_from_lifecycle(self);
    if (!impl || !impl->timer_state) {
        return NX_DEV_STATE_ERROR;
    }
    if (!impl->timer_state->initialized) {
        return NX_DEV_STATE_UNINITIALIZED;
    }
    return impl->timer_state->running ? NX_DEV_STATE_RUNNING
                                      : NX_DEV_STATE_INITIALIZED;
}

/* Power operations */
static nx_status_t timer_power_enable(nx_power_t* self) {
    nx_timer_impl_t* impl = timer_get_impl_from_power(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    hw_timer_enable_clock(impl->timer_state->timer_index);
    return NX_OK;
}

static nx_status_t timer_power_disable(nx_power_t* self) {
    nx_timer_impl_t* impl = timer_get_impl_from_power(self);
    if (!impl || !impl->timer_state) {
        return NX_ERR_NULL_PTR;
    }
    hw_timer_disable_clock(impl->timer_state->timer_index);
    return NX_OK;
}

static bool timer_power_is_enabled(nx_power_t* self) {
    nx_timer_impl_t* impl = timer_get_impl_from_power(self);
    if (!impl || !impl->timer_state) {
        return false;
    }
    return impl->timer_state->initialized;
}

/* Diagnostic operations */
static nx_status_t timer_diagnostic_get_status(nx_diagnostic_t* self,
                                               void* status, size_t size) {
    nx_timer_impl_t* impl = timer_get_impl_from_diagnostic(self);
    if (!impl || !impl->timer_state || !status) {
        return NX_ERR_NULL_PTR;
    }
    if (size < sizeof(nx_timer_stats_t)) {
        return NX_ERR_DATA_SIZE;
    }
    return timer_get_stats(&impl->base, (nx_timer_stats_t*)status);
}

static nx_status_t timer_diagnostic_get_statistics(nx_diagnostic_t* self,
                                                   void* stats, size_t size) {
    return timer_diagnostic_get_status(self, stats, size);
}

static nx_status_t timer_diagnostic_clear_statistics(nx_diagnostic_t* self) {
    nx_timer_impl_t* impl = timer_get_impl_from_diagnostic(self);
    if (!impl) {
        return NX_ERR_NULL_PTR;
    }
    return timer_clear_stats(&impl->base);
}

/**
 * \brief           Initialize timer instance
 */
static void timer_init_instance(nx_timer_impl_t* impl, uint8_t timer_index) {
    /* Initialize base interface */
    impl->base.start = timer_start;
    impl->base.stop = timer_stop;
    impl->base.reset = timer_reset;
    impl->base.get_counter = timer_get_counter;
    impl->base.set_counter = timer_set_counter;
    impl->base.set_callback = timer_set_callback;
    impl->base.clear_callback = timer_clear_callback;
    impl->base.pwm_start = timer_pwm_start;
    impl->base.pwm_stop = timer_pwm_stop;
    impl->base.pwm_set_duty_cycle = timer_pwm_set_duty_cycle;
    impl->base.pwm_get_config = timer_pwm_get_config;
    impl->base.pwm_set_config = timer_pwm_set_config;
    impl->base.set_frequency = timer_set_frequency;
    impl->base.set_period = timer_set_period;
    impl->base.get_config = timer_get_config;
    impl->base.set_config = timer_set_config;
    impl->base.get_lifecycle = timer_get_lifecycle;
    impl->base.get_power = timer_get_power;
    impl->base.get_diagnostic = timer_get_diagnostic;
    impl->base.get_stats = timer_get_stats;
    impl->base.clear_stats = timer_clear_stats;

    /* Initialize lifecycle interface */
    impl->lifecycle.init = timer_lifecycle_init;
    impl->lifecycle.deinit = timer_lifecycle_deinit;
    impl->lifecycle.suspend = timer_lifecycle_suspend;
    impl->lifecycle.resume = timer_lifecycle_resume;
    impl->lifecycle.get_state = timer_lifecycle_get_state;

    /* Initialize power interface */
    impl->power.enable = timer_power_enable;
    impl->power.disable = timer_power_disable;
    impl->power.is_enabled = timer_power_is_enabled;

    /* Initialize diagnostic interface */
    impl->diagnostic.get_status = timer_diagnostic_get_status;
    impl->diagnostic.get_statistics = timer_diagnostic_get_statistics;
    impl->diagnostic.clear_statistics = timer_diagnostic_clear_statistics;

    /* Link to timer state */
    impl->timer_state = &g_timer_states[timer_index];
    impl->timer_state->timer_index = timer_index;
    impl->timer_state->initialized = false;
    impl->timer_state->running = false;
    impl->timer_state->callback = NULL;
    impl->timer_state->callback_ctx = NULL;
    impl->timer_state->overflow_count = 0;
    impl->timer_state->capture_count = 0;
    impl->timer_state->compare_count = 0;

    /* Set default configuration */
    impl->timer_state->config.mode = NX_TIMER_MODE_PERIODIC;
    impl->timer_state->config.frequency_hz = 1000;
    impl->timer_state->config.period_us = 1000;
    impl->timer_state->config.auto_reload = true;
    impl->timer_state->config.prescaler = 0;
}

/**
 * \brief           Get timer instance (factory function)
 * \param[in]       timer_index: Timer index (0-13)
 * \return          Timer interface pointer, NULL on failure
 */
nx_timer_t* nx_timer_stm32f4_get(uint8_t timer_index) {
    if (timer_index >= NX_TIMER_MAX_COUNT) {
        return NULL;
    }

    nx_timer_impl_t* impl = &g_timer_instances[timer_index];

    /* Initialize instance if not already done */
    if (!impl->timer_state) {
        timer_init_instance(impl, timer_index);
    }

    return &impl->base;
}

/**
 * \brief           Get timer instance with configuration
 * \param[in]       timer_index: Timer index (0-13)
 * \param[in]       cfg: Timer configuration
 * \return          Timer interface pointer, NULL on failure
 */
nx_timer_t* nx_timer_stm32f4_get_with_config(uint8_t timer_index,
                                             const nx_timer_config_t* cfg) {
    nx_timer_t* timer = nx_timer_stm32f4_get(timer_index);

    if (!timer || !cfg) {
        return NULL;
    }

    /* Apply configuration */
    nx_timer_impl_t* impl = timer_get_impl(timer);
    if (impl && impl->timer_state) {
        memcpy(&impl->timer_state->config, cfg, sizeof(nx_timer_config_t));
    }

    return timer;
}

/**
 * \brief           Get Timer device descriptor
 * \param[in]       index: Timer index
 * \return          Device descriptor pointer, NULL on failure
 */
nx_device_t* nx_timer_stm32f4_get_device(uint8_t index) {
    if (index >= NX_TIMER_MAX_COUNT) {
        return NULL;
    }

    nx_timer_impl_t* impl = &g_timer_instances[index];
    return impl->device;
}
