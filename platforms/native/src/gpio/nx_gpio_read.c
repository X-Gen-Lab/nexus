/**
 * \file            nx_gpio_read.c
 * \brief           GPIO read interface implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements GPIO read operations including pin state
 *                  reading and external interrupt registration.
 */

#include "hal/nx_status.h"
#include "nx_gpio_helpers.h"
#include "nx_gpio_types.h"

/*---------------------------------------------------------------------------*/
/* GPIO Read Interface Implementation                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Read GPIO pin state
 */
static uint8_t gpio_read(nx_gpio_read_t* self) {
    nx_gpio_read_impl_t* impl = gpio_read_get_impl(self);

    /* Parameter check */
    if (!impl || !impl->state) {
        return 0;
    }

    /* Check initialization */
    if (!impl->state->initialized) {
        return 0;
    }

    /* Update statistics */
    impl->state->stats.read_count++;

    /* Return current pin state */
    return impl->state->pin_state;
}

/**
 * \brief           Register external interrupt callback
 */
static nx_status_t gpio_register_exti(nx_gpio_read_t* self,
                                      nx_gpio_callback_t callback,
                                      void* user_data,
                                      nx_gpio_trigger_t trigger) {
    nx_gpio_read_impl_t* impl = gpio_read_get_impl(self);

    /* Parameter check */
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    /* Check initialization */
    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Register callback */
    impl->state->exti.callback = callback;
    impl->state->exti.user_data = user_data;
    impl->state->exti.trigger = trigger;
    impl->state->exti.enabled = (callback != NULL);

    return NX_OK;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* gpio_read_get_lifecycle(nx_gpio_read_t* self) {
    nx_gpio_read_impl_t* impl = gpio_read_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* gpio_read_get_power(nx_gpio_read_t* self) {
    nx_gpio_read_impl_t* impl = gpio_read_get_impl(self);
    return impl ? &impl->power : NULL;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize GPIO read interface
 */
static void gpio_init_read(nx_gpio_read_t* read) {
    read->read = gpio_read;
    read->register_exti = gpio_register_exti;
    read->get_lifecycle = gpio_read_get_lifecycle;
    read->get_power = gpio_read_get_power;
}
