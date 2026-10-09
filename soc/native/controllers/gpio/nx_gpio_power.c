/**
 * \file            nx_gpio_power.c
 * \brief           GPIO power management interface implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Simulated power follows lifecycle suspend/resume. The
 *                  simulator does not implement power-change callbacks.
 */

#include "hal/nx_status.h"
#include "nx_gpio_helpers.h"
#include "nx_gpio_types.h"

static nx_gpio_read_write_impl_t* gpio_power_get_impl(nx_power_t* self) {
    return self ? NX_CONTAINER_OF(self, nx_gpio_read_write_impl_t, power)
                : NULL;
}

/*---------------------------------------------------------------------------*/
/* GPIO Power Interface Implementation                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enable GPIO power
 */
static nx_status_t gpio_power_enable(nx_power_t* self) {
    nx_gpio_read_write_impl_t* impl = gpio_power_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    return impl->state->suspended ? impl->lifecycle.resume(&impl->lifecycle)
                                  : NX_OK;
}

/**
 * \brief           Disable GPIO power
 */
static nx_status_t gpio_power_disable(nx_power_t* self) {
    nx_gpio_read_write_impl_t* impl = gpio_power_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }
    return impl->state->suspended ? NX_OK
                                  : impl->lifecycle.suspend(&impl->lifecycle);
}

/**
 * \brief           Check if GPIO power is enabled
 */
static bool gpio_power_is_enabled(nx_power_t* self) {
    nx_gpio_read_write_impl_t* impl = gpio_power_get_impl(self);

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
    nx_gpio_read_write_impl_t* impl = gpio_power_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }
    /* A callback cannot be accepted unless the platform can deliver it. */
    (void)user_data;
    return callback ? NX_ERR_NOT_SUPPORTED : NX_OK;
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
