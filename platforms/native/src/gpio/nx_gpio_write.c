/**
 * \file            nx_gpio_write.c
 * \brief           GPIO write interface implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements GPIO write operations including pin state
 *                  writing and toggling.
 */

#include "hal/nx_status.h"
#include "nx_gpio_helpers.h"
#include "nx_gpio_types.h"

/*---------------------------------------------------------------------------*/
/* GPIO Write Interface Implementation                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Write GPIO pin state
 */
static void gpio_write(nx_gpio_write_t* self, uint8_t state) {
    nx_gpio_write_impl_t* impl = gpio_write_get_impl(self);

    /* Parameter check */
    if (!impl || !impl->state) {
        return;
    }

    /* Check initialization */
    if (!impl->state->initialized) {
        return;
    }

    /* Update pin state */
    impl->state->pin_state = state ? 1 : 0;

    /* Update statistics */
    impl->state->stats.write_count++;
}

/**
 * \brief           Toggle GPIO pin state
 */
static void gpio_toggle(nx_gpio_write_t* self) {
    nx_gpio_write_impl_t* impl = gpio_write_get_impl(self);

    /* Parameter check */
    if (!impl || !impl->state) {
        return;
    }

    /* Check initialization */
    if (!impl->state->initialized) {
        return;
    }

    /* Toggle pin state */
    impl->state->pin_state = impl->state->pin_state ? 0 : 1;

    /* Update statistics */
    impl->state->stats.toggle_count++;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* gpio_write_get_lifecycle(nx_gpio_write_t* self) {
    nx_gpio_write_impl_t* impl = gpio_write_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* gpio_write_get_power(nx_gpio_write_t* self) {
    nx_gpio_write_impl_t* impl = gpio_write_get_impl(self);
    return impl ? &impl->power : NULL;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize GPIO write interface
 */
void gpio_init_write(nx_gpio_write_t* write) {
    write->write = gpio_write;
    write->toggle = gpio_toggle;
    write->get_lifecycle = gpio_write_get_lifecycle;
    write->get_power = gpio_write_get_power;
}
