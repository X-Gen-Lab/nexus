/**
 * \file            nx_uart_power.c
 * \brief           UART power management interface for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements UART power management operations. In Native
 *                  platform simulation, power is always enabled.
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "nx_uart_helpers.h"
#include "nx_uart_types.h"

/*---------------------------------------------------------------------------*/
/* Power Interface Implementation                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enable power implementation
 */
static nx_status_t uart_power_enable(nx_power_t* self) {
    /* No-op in simulation */
    (void)self;
    return NX_OK;
}

/**
 * \brief           Disable power implementation
 */
static nx_status_t uart_power_disable(nx_power_t* self) {
    /* No-op in simulation */
    (void)self;
    return NX_OK;
}

/**
 * \brief           Check if power is enabled implementation
 */
static bool uart_power_is_enabled(nx_power_t* self) {
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, power);

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
static nx_status_t uart_power_set_callback(nx_power_t* self,
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
void uart_init_power(nx_power_t* power) {
    power->enable = uart_power_enable;
    power->disable = uart_power_disable;
    power->is_enabled = uart_power_is_enabled;
    power->set_callback = uart_power_set_callback;
}
