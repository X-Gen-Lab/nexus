/**
 * \file            nx_gpio_read_write.c
 * \brief           GPIO read-write interface implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements combined GPIO read-write interface for pins
 *                  that support both input and output operations.
 */

#include "hal/nx_status.h"
#include "nx_gpio_helpers.h"
#include "nx_gpio_types.h"

/*---------------------------------------------------------------------------*/
/* GPIO Read-Write Interface Implementation                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Read GPIO pin state (read-write interface)
 */
static uint8_t gpio_rw_read(nx_gpio_read_t* self) {
    nx_gpio_read_write_impl_t* impl = gpio_read_write_get_impl(
        (nx_gpio_read_write_t*)((char*)self -
                                offsetof(nx_gpio_read_write_t, read)));

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
 * \brief           Register external interrupt callback (read-write interface)
 */
static nx_status_t gpio_rw_register_exti(nx_gpio_read_t* self,
                                         nx_gpio_callback_t callback,
                                         void* user_data,
                                         nx_gpio_trigger_t trigger) {
    nx_gpio_read_write_impl_t* impl = gpio_read_write_get_impl(
        (nx_gpio_read_write_t*)((char*)self -
                                offsetof(nx_gpio_read_write_t, read)));

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
 * \brief           Get lifecycle interface (read interface)
 */
static nx_lifecycle_t* gpio_rw_read_get_lifecycle(nx_gpio_read_t* self) {
    nx_gpio_read_write_impl_t* impl = gpio_read_write_get_impl(
        (nx_gpio_read_write_t*)((char*)self -
                                offsetof(nx_gpio_read_write_t, read)));
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface (read interface)
 */
static nx_power_t* gpio_rw_read_get_power(nx_gpio_read_t* self) {
    nx_gpio_read_write_impl_t* impl = gpio_read_write_get_impl(
        (nx_gpio_read_write_t*)((char*)self -
                                offsetof(nx_gpio_read_write_t, read)));
    return impl ? &impl->power : NULL;
}

/**
 * \brief           Write GPIO pin state (read-write interface)
 */
static void gpio_rw_write(nx_gpio_write_t* self, uint8_t state) {
    nx_gpio_read_write_impl_t* impl = gpio_read_write_get_impl(
        (nx_gpio_read_write_t*)((char*)self -
                                offsetof(nx_gpio_read_write_t, write)));

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
 * \brief           Toggle GPIO pin state (read-write interface)
 */
static void gpio_rw_toggle(nx_gpio_write_t* self) {
    nx_gpio_read_write_impl_t* impl = gpio_read_write_get_impl(
        (nx_gpio_read_write_t*)((char*)self -
                                offsetof(nx_gpio_read_write_t, write)));

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
 * \brief           Get lifecycle interface (write interface)
 */
static nx_lifecycle_t* gpio_rw_write_get_lifecycle(nx_gpio_write_t* self) {
    nx_gpio_read_write_impl_t* impl = gpio_read_write_get_impl(
        (nx_gpio_read_write_t*)((char*)self -
                                offsetof(nx_gpio_read_write_t, write)));
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface (write interface)
 */
static nx_power_t* gpio_rw_write_get_power(nx_gpio_write_t* self) {
    nx_gpio_read_write_impl_t* impl = gpio_read_write_get_impl(
        (nx_gpio_read_write_t*)((char*)self -
                                offsetof(nx_gpio_read_write_t, write)));
    return impl ? &impl->power : NULL;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization (used by device registration)                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize GPIO read interface for read-write
 * \note            This is called from nx_gpio_device.c
 */
void gpio_init_read(nx_gpio_read_t* read) {
    read->read = gpio_rw_read;
    read->register_exti = gpio_rw_register_exti;
    read->get_lifecycle = gpio_rw_read_get_lifecycle;
    read->get_power = gpio_rw_read_get_power;
}

/**
 * \brief           Initialize GPIO write interface for read-write
 * \note            This is called from nx_gpio_device.c
 */
void gpio_init_write(nx_gpio_write_t* write) {
    write->write = gpio_rw_write;
    write->toggle = gpio_rw_toggle;
    write->get_lifecycle = gpio_rw_write_get_lifecycle;
    write->get_power = gpio_rw_write_get_power;
}
