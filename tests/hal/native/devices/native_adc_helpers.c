/**
 * \file            native_adc_helpers.c
 * \brief           Native ADC test helpers implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "native_adc_helpers.h"
#include "hal/nx_factory.h"

/* Include platform-specific types */
#include "../../../../platforms/native/src/adc/nx_adc_types.h"

#include <string.h>

/*---------------------------------------------------------------------------*/
/* Constants                                                                 */
/*---------------------------------------------------------------------------*/

#define NX_ADC_MAX_INSTANCES 4

/*---------------------------------------------------------------------------*/
/* Internal Helper                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get ADC implementation structure
 * \details         Retrieves the implementation structure from the factory.
 */
static nx_adc_impl_t* get_adc_impl(uint8_t instance) {
    /* Validate parameters */
    if (instance >= NX_ADC_MAX_INSTANCES) {
        return NULL;
    }

    /* Get ADC instance from factory */
    nx_adc_t* adc = nx_factory_adc(instance);
    if (adc == NULL) {
        return NULL;
    }

    return (nx_adc_impl_t*)adc;
}

/*---------------------------------------------------------------------------*/
/* Test Helper Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get ADC device state
 */
nx_status_t native_adc_get_state(uint8_t instance, native_adc_state_t* state) {
    /* Validate parameters */
    if (state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Get implementation */
    nx_adc_impl_t* impl = get_adc_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Copy state information */
    state->initialized = impl->state->initialized;
    state->suspended = impl->state->suspended;
    state->clock_enabled = impl->state->clock_enabled;
    state->channel_count = impl->state->config.channel_count;
    state->resolution = impl->state->config.resolution;
    state->conversion_count = impl->state->stats.conversion_count;
    state->error_count = impl->state->stats.error_count;

    return NX_OK;
}

/**
 * \brief           Set analog input value for a channel
 * \details         Sets the simulated analog value that will be returned
 *                  when the ADC channel is read. This simulates an external
 *                  analog voltage input to the ADC.
 */
nx_status_t native_adc_set_analog_value(uint8_t instance, uint8_t channel,
                                        uint16_t value) {
    /* Get implementation */
    nx_adc_impl_t* impl = get_adc_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Validate channel index */
    if (channel >= NX_ADC_MAX_CHANNELS) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Set the simulated value for the channel */
    impl->channels[channel].simulated_value = value;

    return NX_OK;
}

/**
 * \brief           Reset specific ADC instance
 * \details         Resets the ADC instance to its initial state, clearing
 *                  all configuration, state, and statistics.
 */
nx_status_t native_adc_reset(uint8_t instance) {
    /* Get implementation */
    nx_adc_impl_t* impl = get_adc_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Reset state */
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->clock_enabled = false;

    /* Reset statistics */
    memset(&impl->state->stats, 0, sizeof(nx_adc_stats_t));

    /* Reset all channel simulated values */
    for (uint8_t i = 0; i < NX_ADC_MAX_CHANNELS; i++) {
        impl->channels[i].simulated_value = 0;
    }

    return NX_OK;
}

/**
 * \brief           Reset all ADC instances
 * \details         Iterates through all possible ADC instances and resets
 *                  each one to its initial state.
 */
void native_adc_reset_all(void) {
    for (uint8_t instance = 0; instance < NX_ADC_MAX_INSTANCES; instance++) {
        native_adc_reset(instance);
    }
}
