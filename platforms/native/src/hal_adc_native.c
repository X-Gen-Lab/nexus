/**
 * \file            hal_adc_native.c
 * \brief           Native Platform ADC HAL Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "hal/hal_adc.h"
#include "native_platform.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

#define MAX_ADC_CHANNELS      16
#define INTERNAL_TEMP_CHANNEL 16
#define INTERNAL_VREF_CHANNEL 17

/**
 * \brief           Simulated ADC state
 */
struct native_adc_state {
    bool initialized;
    hal_adc_config_t config;
    uint16_t simulated_values[MAX_ADC_CHANNELS];
    hal_adc_callback_t callback;
    void* callback_context;
};

/**
 * \brief           ADC state array
 */
static struct native_adc_state adc_state[HAL_ADC_MAX];

/**
 * \brief           Random seed initialized flag
 */
static bool random_initialized = false;

/*===========================================================================*/
/* Local functions                                                            */
/*===========================================================================*/

/**
 * \brief           Get max value for resolution
 */
static uint16_t get_max_value_for_resolution(hal_adc_resolution_t resolution) {
    switch (resolution) {
        case HAL_ADC_RES_6BIT:
            return 63;
        case HAL_ADC_RES_8BIT:
            return 255;
        case HAL_ADC_RES_10BIT:
            return 1023;
        case HAL_ADC_RES_12BIT:
            return 4095;
        default:
            return 4095;
    }
}

/**
 * \brief           Generate simulated ADC value
 */
static uint16_t generate_simulated_value(hal_adc_resolution_t resolution) {
    if (!random_initialized) {
        srand((unsigned int)time(NULL));
        random_initialized = true;
    }
    uint16_t max_val = get_max_value_for_resolution(resolution);
    return (uint16_t)(rand() % (max_val + 1));
}

/*===========================================================================*/
/* Public test helper functions                                               */
/*===========================================================================*/

void native_adc_reset_all(void) {
    memset(adc_state, 0, sizeof(adc_state));
}

native_adc_state_t* native_adc_get_state(int instance) {
    if (instance < 0 || instance >= HAL_ADC_MAX) {
        return NULL;
    }
    return (native_adc_state_t*)&adc_state[instance];
}

bool native_adc_is_initialized(int instance) {
    if (instance < 0 || instance >= HAL_ADC_MAX) {
        return false;
    }
    return adc_state[instance].initialized;
}

hal_adc_resolution_t native_adc_get_resolution(int instance) {
    if (instance < 0 || instance >= HAL_ADC_MAX) {
        return HAL_ADC_RES_12BIT;
    }
    return adc_state[instance].config.resolution;
}

bool native_adc_set_simulated_value(int instance, uint8_t channel,
                                    uint16_t value) {
    if (instance < 0 || instance >= HAL_ADC_MAX) {
        return false;
    }
    if (channel >= MAX_ADC_CHANNELS) {
        return false;
    }
    adc_state[instance].simulated_values[channel] = value;
    return true;
}

uint16_t native_adc_get_simulated_value(int instance, uint8_t channel) {
    if (instance < 0 || instance >= HAL_ADC_MAX) {
        return 0;
    }
    if (channel >= MAX_ADC_CHANNELS) {
        return 0;
    }
    return adc_state[instance].simulated_values[channel];
}

/*===========================================================================*/
/* Public HAL functions                                                       */
/*===========================================================================*/

hal_status_t hal_adc_init(hal_adc_instance_t instance,
                          const hal_adc_config_t* config) {
    if (instance >= HAL_ADC_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    struct native_adc_state* state = &adc_state[instance];

    if (state->initialized) {
        return HAL_ERROR_ALREADY_INIT;
    }

    state->config = *config;
    state->initialized = true;
    state->callback = NULL;
    state->callback_context = NULL;

    /* Initialize simulated values with random data */
    for (int i = 0; i < MAX_ADC_CHANNELS; i++) {
        state->simulated_values[i] =
            generate_simulated_value(config->resolution);
    }

    return HAL_OK;
}

hal_status_t hal_adc_deinit(hal_adc_instance_t instance) {
    if (instance >= HAL_ADC_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    struct native_adc_state* state = &adc_state[instance];

    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    memset(state, 0, sizeof(struct native_adc_state));
    return HAL_OK;
}

hal_status_t hal_adc_config_channel(hal_adc_instance_t instance,
                                    const hal_adc_channel_config_t* config) {
    if (instance >= HAL_ADC_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (config->channel >= MAX_ADC_CHANNELS) {
        return HAL_ERROR_INVALID_PARAM;
    }

    struct native_adc_state* state = &adc_state[instance];

    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* In native simulation, channel config is a no-op */
    return HAL_OK;
}

hal_status_t hal_adc_read(hal_adc_instance_t instance, uint8_t channel,
                          uint16_t* value, uint32_t timeout_ms) {
    (void)timeout_ms;

    if (instance >= HAL_ADC_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (value == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (channel >= MAX_ADC_CHANNELS) {
        return HAL_ERROR_INVALID_PARAM;
    }

    struct native_adc_state* state = &adc_state[instance];

    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    *value = state->simulated_values[channel];

    /* Invoke callback if registered */
    if (state->callback != NULL) {
        state->callback(instance, *value, state->callback_context);
    }

    return HAL_OK;
}

hal_status_t hal_adc_read_multi(hal_adc_instance_t instance,
                                const uint8_t* channels, uint16_t* values,
                                size_t count, uint32_t timeout_ms) {
    (void)timeout_ms;

    if (instance >= HAL_ADC_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (channels == NULL || values == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (count == 0) {
        return HAL_ERROR_INVALID_PARAM;
    }

    struct native_adc_state* state = &adc_state[instance];

    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    for (size_t i = 0; i < count; i++) {
        if (channels[i] >= MAX_ADC_CHANNELS) {
            return HAL_ERROR_INVALID_PARAM;
        }
        values[i] = state->simulated_values[channels[i]];
    }

    return HAL_OK;
}

uint32_t hal_adc_to_millivolts(hal_adc_instance_t instance, uint16_t raw_value,
                               uint32_t vref_mv) {
    if (instance >= HAL_ADC_MAX) {
        return 0;
    }

    struct native_adc_state* state = &adc_state[instance];

    if (!state->initialized) {
        return 0;
    }

    uint16_t max_value = get_max_value_for_resolution(state->config.resolution);

    /* Formula: mv = raw * vref_mv / max_value */
    return (uint32_t)raw_value * vref_mv / max_value;
}

hal_status_t hal_adc_read_temperature(hal_adc_instance_t instance,
                                      int16_t* temp_c) {
    if (instance >= HAL_ADC_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (temp_c == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    struct native_adc_state* state = &adc_state[instance];

    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Return simulated room temperature (25°C) */
    *temp_c = 25;
    return HAL_OK;
}

hal_status_t hal_adc_read_vref(hal_adc_instance_t instance, uint16_t* vref_mv) {
    if (instance >= HAL_ADC_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (vref_mv == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    struct native_adc_state* state = &adc_state[instance];

    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Return simulated internal reference voltage (1.21V typical for STM32) */
    *vref_mv = 1210;
    return HAL_OK;
}

hal_status_t hal_adc_set_callback(hal_adc_instance_t instance,
                                  hal_adc_callback_t callback, void* context) {
    if (instance >= HAL_ADC_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    struct native_adc_state* state = &adc_state[instance];

    if (!state->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    state->callback = callback;
    state->callback_context = context;

    return HAL_OK;
}
