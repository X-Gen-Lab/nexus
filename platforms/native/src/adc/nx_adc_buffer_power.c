/**
 * \file            nx_adc_buffer_power.c
 * \brief           ADC Buffer power interface implementation for Native
 *                  platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements ADC Buffer power management operations. In Native
 *                  platform simulation, power is always enabled.
 */

#include "nx_adc_helpers.h"
#include "nx_adc_types.h"
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* Power Operations                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enable ADC buffer power
 */
static nx_status_t adc_buffer_power_enable(nx_power_t* self) {
    nx_adc_buffer_impl_t* impl =
        NX_CONTAINER_OF(self, nx_adc_buffer_impl_t, power);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    impl->state->clock_enabled = true;
    return NX_OK;
}

/**
 * \brief           Disable ADC buffer power
 */
static nx_status_t adc_buffer_power_disable(nx_power_t* self) {
    nx_adc_buffer_impl_t* impl =
        NX_CONTAINER_OF(self, nx_adc_buffer_impl_t, power);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    impl->state->clock_enabled = false;
    return NX_OK;
}

/**
 * \brief           Check if ADC buffer power is enabled
 */
static bool adc_buffer_power_is_enabled(nx_power_t* self) {
    nx_adc_buffer_impl_t* impl =
        NX_CONTAINER_OF(self, nx_adc_buffer_impl_t, power);
    if (!impl || !impl->state) {
        return false;
    }
    return impl->state->clock_enabled;
}

/**
 * \brief           Set power state change callback
 */
static nx_status_t adc_buffer_power_set_callback(nx_power_t* self,
                                                 nx_power_callback_t callback,
                                                 void* user_data) {
    (void)self;
    (void)callback;
    (void)user_data;
    /* Callback not implemented in simulation */
    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize power interface
 */
void adc_buffer_init_power(nx_power_t* power) {
    power->enable = adc_buffer_power_enable;
    power->disable = adc_buffer_power_disable;
    power->is_enabled = adc_buffer_power_is_enabled;
    power->set_callback = adc_buffer_power_set_callback;
}
