/**
 * \file            nx_dac_power.c
 * \brief           DAC power interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements DAC power management operations. In Native
 *                  platform simulation, power is always enabled.
 */

#include "nx_dac_helpers.h"
#include "nx_dac_types.h"
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get DAC implementation from power interface
 */
static nx_dac_impl_t* dac_get_impl_from_power(nx_power_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_dac_impl_t*)((char*)self - offsetof(nx_dac_impl_t, power));
}

/*---------------------------------------------------------------------------*/
/* Power Operations                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enable DAC power
 */
static nx_status_t dac_power_enable(nx_power_t* self) {
    nx_dac_impl_t* impl = dac_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    impl->state->clock_enabled = true;
    return NX_OK;
}

/**
 * \brief           Disable DAC power
 */
static nx_status_t dac_power_disable(nx_power_t* self) {
    nx_dac_impl_t* impl = dac_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    impl->state->clock_enabled = false;
    return NX_OK;
}

/**
 * \brief           Check if DAC power is enabled
 */
static bool dac_power_is_enabled(nx_power_t* self) {
    nx_dac_impl_t* impl = dac_get_impl_from_power(self);
    if (!impl || !impl->state) {
        return false;
    }
    return impl->state->clock_enabled;
}

/**
 * \brief           Set power state change callback
 */
static nx_status_t dac_power_set_callback(nx_power_t* self,
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
void dac_init_power(nx_power_t* power) {
    power->enable = dac_power_enable;
    power->disable = dac_power_disable;
    power->is_enabled = dac_power_is_enabled;
    power->set_callback = dac_power_set_callback;
}
