/**
 * \file            nx_i2c_power.c
 * \brief           I2C power management interface for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements I2C power management operations. In Native
 *                  platform simulation, power is always enabled.
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "nx_i2c_helpers.h"
#include "nx_i2c_types.h"

/*---------------------------------------------------------------------------*/
/* Power Interface Implementation                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enable power implementation
 */
static nx_status_t i2c_power_enable(nx_power_t* self) {
    /* No-op in simulation */
    (void)self;
    return NX_OK;
}

/**
 * \brief           Disable power implementation
 */
static nx_status_t i2c_power_disable(nx_power_t* self) {
    /* No-op in simulation */
    (void)self;
    return NX_OK;
}

/**
 * \brief           Check if power is enabled implementation
 */
static bool i2c_power_is_enabled(nx_power_t* self) {
    nx_i2c_impl_t* impl = NX_CONTAINER_OF(self, nx_i2c_impl_t, power);

    /* Parameter validation */
    if (!impl->state) {
        return false;
    }

    /* Power is enabled if initialized and not suspended */
    return impl->state->initialized && !impl->state->suspended;
}

/**
 * \brief           Set power callback implementation
 */
static nx_status_t i2c_power_set_callback(nx_power_t* self,
                                          nx_power_callback_t callback,
                                          void* user_data) {
    /* No-op in simulation */
    (void)self;
    (void)callback;
    (void)user_data;
    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize power interface
 */
void i2c_init_power(nx_power_t* power) {
    power->enable = i2c_power_enable;
    power->disable = i2c_power_disable;
    power->is_enabled = i2c_power_is_enabled;
    power->set_callback = i2c_power_set_callback;
}
