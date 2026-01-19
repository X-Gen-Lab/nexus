/**
 * \file            nx_adc_power.c
 * \brief           ADC power interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements ADC power management operations. In Native
 *                  platform simulation, power is always enabled.
 */

#include "nx_adc_helpers.h"
#include "nx_adc_types.h"
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get ADC implementation from power interface
 */
static nx_adc_impl_t* adc_get_impl_from_power(nx_power_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_adc_impl_t*)((char*)self - offsetof(nx_adc_impl_t, power));
}

/*---------------------------------------------------------------------------*/
/* Power Operations                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enable ADC power
 */
static nx_status_t adc_power_enable(nx_power_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    impl->state->clock_enabled = true;
    return NX_OK;
}

/**
 * \brief           Disable ADC power
 */
static nx_status_t adc_power_disable(nx_power_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    impl->state->clock_enabled = false;
    return NX_OK;
}

/**
 * \brief           Check if ADC power is enabled
 */
static bool adc_power_is_enabled(nx_power_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return false;
    }
    return impl->state->clock_enabled;
}

/**
 * \brief           Set power state change callback
 */
static nx_status_t adc_power_set_callback(nx_power_t* self,
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
void adc_init_power(nx_power_t* power) {
    power->enable = adc_power_enable;
    power->disable = adc_power_disable;
    power->is_enabled = adc_power_is_enabled;
    power->set_callback = adc_power_set_callback;
}
