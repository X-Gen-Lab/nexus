/**
 * \file            hal_timer_native.c
 * \brief           Native Platform Timer HAL Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This implementation simulates timer functionality for testing purposes
 * on the native platform. It uses internal state tracking to simulate
 * timer behavior without actual hardware timers.
 */

#include "hal/hal_timer.h"
#include "native_platform.h"
#include <stdbool.h>
#include <string.h>

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

#define MAX_TIMER_INSTANCES HAL_TIMER_MAX
#define MAX_PWM_CHANNELS    HAL_TIMER_CH_MAX

/**
 * \brief           PWM channel state structure
 */
typedef struct {
    bool initialized;    /**< Channel initialized flag */
    bool running;        /**< Channel running flag */
    uint32_t frequency;  /**< PWM frequency in Hz */
    uint16_t duty_cycle; /**< Duty cycle (0-10000) */
} pwm_channel_state_t;

/**
 * \brief           Timer state structure
 */
typedef struct {
    bool initialized;              /**< Timer initialized flag */
    bool running;                  /**< Timer running flag */
    hal_timer_config_t config;     /**< Timer configuration */
    uint32_t count;                /**< Current counter value */
    uint32_t callback_count;       /**< Number of callback invocations */
    hal_timer_callback_t callback; /**< Timer callback function */
    void* callback_context;        /**< Callback context */
    pwm_channel_state_t pwm[MAX_PWM_CHANNELS]; /**< PWM channel states */
} timer_state_t;

static timer_state_t timer_state[MAX_TIMER_INSTANCES];

/*===========================================================================*/
/* Public functions - Test helpers                                            */
/*===========================================================================*/

void native_timer_reset_all(void) {
    memset(timer_state, 0, sizeof(timer_state));
}

native_timer_state_t* native_timer_get_state(int instance) {
    if (instance < 0 || instance >= MAX_TIMER_INSTANCES) {
        return NULL;
    }
    return (native_timer_state_t*)&timer_state[instance];
}

bool native_timer_is_initialized(int instance) {
    if (instance < 0 || instance >= MAX_TIMER_INSTANCES) {
        return false;
    }
    return timer_state[instance].initialized;
}

bool native_timer_is_running(int instance) {
    if (instance < 0 || instance >= MAX_TIMER_INSTANCES) {
        return false;
    }
    return timer_state[instance].running;
}

uint32_t native_timer_get_period_us(int instance) {
    if (instance < 0 || instance >= MAX_TIMER_INSTANCES) {
        return 0;
    }
    if (!timer_state[instance].initialized) {
        return 0;
    }
    return timer_state[instance].config.period_us;
}

hal_timer_mode_t native_timer_get_mode(int instance) {
    if (instance < 0 || instance >= MAX_TIMER_INSTANCES) {
        return HAL_TIMER_MODE_ONESHOT;
    }
    return timer_state[instance].config.mode;
}

uint32_t native_timer_get_callback_count(int instance) {
    if (instance < 0 || instance >= MAX_TIMER_INSTANCES) {
        return 0;
    }
    return timer_state[instance].callback_count;
}

bool native_timer_simulate_period_elapsed(int instance) {
    if (instance < 0 || instance >= MAX_TIMER_INSTANCES) {
        return false;
    }

    timer_state_t* state = &timer_state[instance];
    if (!state->initialized || !state->running) {
        return false;
    }

    /* Invoke callback if registered */
    if (state->callback != NULL) {
        state->callback((hal_timer_instance_t)instance,
                        state->callback_context);
        state->callback_count++;
    }

    /* Handle mode-specific behavior */
    if (state->config.mode == HAL_TIMER_MODE_ONESHOT) {
        state->running = false;
    }
    /* For PERIODIC mode, timer keeps running */

    return true;
}

bool native_pwm_is_initialized(int instance, int channel) {
    if (instance < 0 || instance >= MAX_TIMER_INSTANCES) {
        return false;
    }
    if (channel < 0 || channel >= MAX_PWM_CHANNELS) {
        return false;
    }
    return timer_state[instance].pwm[channel].initialized;
}

bool native_pwm_is_running(int instance, int channel) {
    if (instance < 0 || instance >= MAX_TIMER_INSTANCES) {
        return false;
    }
    if (channel < 0 || channel >= MAX_PWM_CHANNELS) {
        return false;
    }
    return timer_state[instance].pwm[channel].running;
}

uint32_t native_pwm_get_frequency(int instance, int channel) {
    if (instance < 0 || instance >= MAX_TIMER_INSTANCES) {
        return 0;
    }
    if (channel < 0 || channel >= MAX_PWM_CHANNELS) {
        return 0;
    }
    return timer_state[instance].pwm[channel].frequency;
}

uint16_t native_pwm_get_duty_cycle(int instance, int channel) {
    if (instance < 0 || instance >= MAX_TIMER_INSTANCES) {
        return 0;
    }
    if (channel < 0 || channel >= MAX_PWM_CHANNELS) {
        return 0;
    }
    return timer_state[instance].pwm[channel].duty_cycle;
}

/*===========================================================================*/
/* Public functions - HAL API                                                 */
/*===========================================================================*/

hal_status_t hal_timer_init(hal_timer_instance_t instance,
                            const hal_timer_config_t* config) {
    if (instance >= MAX_TIMER_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (config->period_us == 0) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer_state_t* state = &timer_state[instance];

    if (state->initialized) {
        return HAL_ERROR_ALREADY_INIT;
    }

    state->config = *config;
    state->count = 0;
    state->callback_count = 0;
    state->callback = NULL;
    state->callback_context = NULL;
    state->running = false;
    state->initialized = true;

    /* Reset PWM channels */
    memset(state->pwm, 0, sizeof(state->pwm));

    return HAL_OK;
}

hal_status_t hal_timer_deinit(hal_timer_instance_t instance) {
    if (instance >= MAX_TIMER_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer_state_t* state = &timer_state[instance];

    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    state->initialized = false;
    state->running = false;
    state->callback = NULL;
    state->callback_context = NULL;
    state->count = 0;
    state->callback_count = 0;
    memset(state->pwm, 0, sizeof(state->pwm));

    return HAL_OK;
}

hal_status_t hal_timer_start(hal_timer_instance_t instance) {
    if (instance >= MAX_TIMER_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer_state_t* state = &timer_state[instance];

    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    if (state->running) {
        return HAL_ERROR_INVALID_STATE;
    }

    state->running = true;
    state->count = 0;

    return HAL_OK;
}

hal_status_t hal_timer_stop(hal_timer_instance_t instance) {
    if (instance >= MAX_TIMER_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer_state_t* state = &timer_state[instance];

    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    state->running = false;
    /* Preserve count value as per requirements */

    return HAL_OK;
}

hal_status_t hal_timer_get_count(hal_timer_instance_t instance,
                                 uint32_t* count) {
    if (instance >= MAX_TIMER_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (count == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    timer_state_t* state = &timer_state[instance];

    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    *count = state->count;

    return HAL_OK;
}

hal_status_t hal_timer_set_count(hal_timer_instance_t instance,
                                 uint32_t count) {
    if (instance >= MAX_TIMER_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer_state_t* state = &timer_state[instance];

    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    state->count = count;

    return HAL_OK;
}

hal_status_t hal_timer_set_callback(hal_timer_instance_t instance,
                                    hal_timer_callback_t callback,
                                    void* context) {
    if (instance >= MAX_TIMER_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer_state_t* state = &timer_state[instance];

    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    state->callback = callback;
    state->callback_context = context;

    return HAL_OK;
}

/*===========================================================================*/
/* Public functions - PWM API                                                 */
/*===========================================================================*/

hal_status_t hal_pwm_init(hal_timer_instance_t instance,
                          hal_timer_channel_t channel,
                          const hal_pwm_config_t* config) {
    if (instance >= MAX_TIMER_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (channel >= MAX_PWM_CHANNELS) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (config->frequency == 0) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (config->duty_cycle > 10000) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer_state_t* state = &timer_state[instance];
    pwm_channel_state_t* pwm = &state->pwm[channel];

    if (pwm->initialized) {
        return HAL_ERROR_ALREADY_INIT;
    }

    pwm->frequency = config->frequency;
    pwm->duty_cycle = config->duty_cycle;
    pwm->running = false;
    pwm->initialized = true;

    return HAL_OK;
}

hal_status_t hal_pwm_start(hal_timer_instance_t instance,
                           hal_timer_channel_t channel) {
    if (instance >= MAX_TIMER_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (channel >= MAX_PWM_CHANNELS) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer_state_t* state = &timer_state[instance];
    pwm_channel_state_t* pwm = &state->pwm[channel];

    if (!pwm->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    if (pwm->running) {
        return HAL_ERROR_INVALID_STATE;
    }

    pwm->running = true;

    return HAL_OK;
}

hal_status_t hal_pwm_stop(hal_timer_instance_t instance,
                          hal_timer_channel_t channel) {
    if (instance >= MAX_TIMER_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (channel >= MAX_PWM_CHANNELS) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer_state_t* state = &timer_state[instance];
    pwm_channel_state_t* pwm = &state->pwm[channel];

    if (!pwm->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    pwm->running = false;

    return HAL_OK;
}

hal_status_t hal_pwm_set_duty(hal_timer_instance_t instance,
                              hal_timer_channel_t channel,
                              uint16_t duty_cycle) {
    if (instance >= MAX_TIMER_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (channel >= MAX_PWM_CHANNELS) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (duty_cycle > 10000) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer_state_t* state = &timer_state[instance];
    pwm_channel_state_t* pwm = &state->pwm[channel];

    if (!pwm->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    pwm->duty_cycle = duty_cycle;

    return HAL_OK;
}
