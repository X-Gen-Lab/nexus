/**
 * \file            nx_timer_power.c
 * \brief           Timer power interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements Timer power management operations. In Native
 *                  platform simulation, power is always enabled.
 */

#include "hal/nx_status.h"
#include "nx_timer_helpers.h"
#include "nx_timer_types.h"

/*---------------------------------------------------------------------------*/
/* Power Interface Implementation                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enable power implementation
 */
static nx_status_t timer_power_enable(nx_power_t* self) {
    (void)self;
    return NX_OK; /* No operation in simulation */
}

/**
 * \brief           Disable power implementation
 */
static nx_status_t timer_power_disable(nx_power_t* self) {
    (void)self;
    return NX_OK; /* No operation in simulation */
}

/**
 * \brief           Check power enabled implementation
 */
static bool timer_power_is_enabled(nx_power_t* self) {
    nx_timer_impl_t* impl = NX_CONTAINER_OF(self, nx_timer_impl_t, power);

    if (!impl->state) {
        return false;
    }
    return impl->state->initialized && !impl->state->suspended;
}

/**
 * \brief           Set power callback implementation
 */
static nx_status_t timer_power_set_callback(nx_power_t* self,
                                            nx_power_callback_t callback,
                                            void* user_data) {
    (void)self;
    (void)callback;
    (void)user_data;
    return NX_OK; /* No operation in simulation */
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize power interface
 */
void timer_init_power(nx_power_t* power) {
    power->enable = timer_power_enable;
    power->disable = timer_power_disable;
    power->is_enabled = timer_power_is_enabled;
    power->set_callback = timer_power_set_callback;
}
