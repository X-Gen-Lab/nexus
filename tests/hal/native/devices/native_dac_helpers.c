/**
 * \file            native_dac_helpers.c
 * \brief           Native DAC test helpers implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "native_dac_helpers.h"
#include "hal/nx_factory.h"

/* Include platform-specific types */
#include "../../../../platforms/native/src/dac/nx_dac_types.h"

#include <string.h>

/*---------------------------------------------------------------------------*/
/* Constants                                                                 */
/*---------------------------------------------------------------------------*/

#define NX_DAC_MAX_INSTANCES 4

/*---------------------------------------------------------------------------*/
/* Internal Helper                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get DAC implementation structure
 * \details         Retrieves the implementation structure from the factory.
 */
static nx_dac_impl_t* get_dac_impl(uint8_t instance) {
    /* Validate parameters */
    if (instance >= NX_DAC_MAX_INSTANCES) {
        return NULL;
    }

    /* Get DAC instance from factory */
    nx_dac_t* dac = nx_factory_dac(instance);
    if (dac == NULL) {
        return NULL;
    }

    return (nx_dac_impl_t*)dac;
}

/*---------------------------------------------------------------------------*/
/* Test Helper Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get DAC device state
 */
nx_status_t native_dac_get_state(uint8_t instance, native_dac_state_t* state) {
    /* Validate parameters */
    if (state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Get implementation */
    nx_dac_impl_t* impl = get_dac_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Copy state information */
    state->initialized = impl->state->initialized;
    state->suspended = impl->state->suspended;
    state->clock_enabled = impl->state->clock_enabled;
    state->channel_count = impl->state->config.channel_count;
    state->resolution = impl->state->config.resolution;
    state->vref_mv = impl->state->config.vref_mv;

    return NX_OK;
}

/**
 * \brief           Get DAC channel output value
 * \details         Returns the current output value set for the specified
 *                  DAC channel.
 */
uint32_t native_dac_get_output_value(uint8_t instance, uint8_t channel) {
    /* Get implementation */
    nx_dac_impl_t* impl = get_dac_impl(instance);
    if (impl == NULL) {
        return 0;
    }

    /* Validate channel index */
    if (channel >= NX_DAC_MAX_CHANNELS) {
        return 0;
    }

    /* Return channel value */
    return impl->channels[channel].current_value;
}

/**
 * \brief           Reset specific DAC instance
 * \details         Resets the DAC instance to its initial state, clearing
 *                  all configuration and state.
 */
nx_status_t native_dac_reset(uint8_t instance) {
    /* Get implementation */
    nx_dac_impl_t* impl = get_dac_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Reset state */
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->clock_enabled = false;

    /* Reset all channel values */
    for (uint8_t i = 0; i < NX_DAC_MAX_CHANNELS; i++) {
        impl->channels[i].current_value = 0;
    }

    return NX_OK;
}

/**
 * \brief           Reset all DAC instances
 * \details         Iterates through all possible DAC instances and resets
 *                  each one to its initial state.
 */
void native_dac_reset_all(void) {
    for (uint8_t instance = 0; instance < NX_DAC_MAX_INSTANCES; instance++) {
        native_dac_reset(instance);
    }
}
