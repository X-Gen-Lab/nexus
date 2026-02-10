/**
 * \file            stm32_uart_power.c
 * \brief           STM32 UART power interface implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-03
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements UART power management operations.
 *                  Requirement 8.4: Return current power state.
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "stm32_uart_types.h"

/*---------------------------------------------------------------------------*/
/* Power Interface Implementation                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enable device power/clock
 */
static nx_status_t uart_power_enable(nx_power_t* self) {
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, power);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Resume from suspended state */
    if (impl->state->suspended) {
        impl->state->suspended = false;
    }

    return NX_OK;
}

/**
 * \brief           Disable device power/clock
 */
static nx_status_t uart_power_disable(nx_power_t* self) {
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, power);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Enter suspended state */
    impl->state->suspended = true;

    return NX_OK;
}

/**
 * \brief           Check if device power is enabled
 */
static bool uart_power_is_enabled(nx_power_t* self) {
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, power);

    /* Parameter validation */
    if (!impl->state || !impl->state->initialized) {
        return false;
    }

    return !impl->state->suspended;
}

/**
 * \brief           Set power state change callback
 */
static nx_status_t uart_power_set_callback(nx_power_t* self,
                                           nx_power_callback_t callback,
                                           void* user_data) {
    (void)self;
    (void)callback;
    (void)user_data;
    return NX_ERR_NOT_SUPPORTED;
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
