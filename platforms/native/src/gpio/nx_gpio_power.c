/**
 * \file            nx_gpio_power.c
 * \brief           GPIO power management interface implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements GPIO power management operations. In Native
 *                  platform simulation, power is always enabled.
 */

#include "hal/nx_status.h"
#include "nx_gpio_helpers.h"
#include "nx_gpio_types.h"

/*---------------------------------------------------------------------------*/
/* GPIO Power Interface Implementation                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enable GPIO power
 */
static nx_status_t gpio_power_enable(nx_power_t* self) {
    /* In Native platform simulation, power is always enabled */
    (void)self;
    return NX_OK;
}

/**
 * \brief           Disable GPIO power
 */
static nx_status_t gpio_power_disable(nx_power_t* self) {
    /* In Native platform simulation, power is always enabled */
    (void)self;
    return NX_OK;
}

/**
 * \brief           Check if GPIO power is enabled
 */
static bool gpio_power_is_enabled(nx_power_t* self) {
    nx_gpio_read_write_impl_t* impl =
        (nx_gpio_read_write_impl_t*)((char*)self -
                                     offsetof(nx_gpio_read_write_impl_t,
                                              power));

    /* Parameter check */
    if (!impl || !impl->state) {
        return false;
    }

    /* Return power state based on initialization and suspend state */
    return impl->state->initialized && !impl->state->suspended;
}

/**
 * \brief           Set power callback
 */
static nx_status_t gpio_power_set_callback(nx_power_t* self,
                                           nx_power_callback_t callback,
                                           void* user_data) {
    /* In Native platform simulation, power callbacks are not supported */
    (void)self;
    (void)callback;
    (void)user_data;
    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize GPIO power interface
 */
void gpio_init_power(nx_power_t* power) {
    power->enable = gpio_power_enable;
    power->disable = gpio_power_disable;
    power->is_enabled = gpio_power_is_enabled;
    power->set_callback = gpio_power_set_callback;
}
