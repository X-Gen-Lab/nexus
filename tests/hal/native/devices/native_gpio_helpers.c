/**
 * \file            native_gpio_helpers.c
 * \brief           Native GPIO test helpers implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "native_gpio_helpers.h"
#include "hal/nx_factory.h"

/* Include platform-specific types */
#include "../../../../platforms/native/src/gpio/nx_gpio_types.h"

#include <string.h>

/*---------------------------------------------------------------------------*/
/* Constants                                                                 */
/*---------------------------------------------------------------------------*/

#define NX_GPIO_MAX_PORTS 8
#define NX_GPIO_MAX_PINS  16

/*---------------------------------------------------------------------------*/
/* Internal Helper                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get GPIO implementation structure
 * \details         Converts port and pin to instance index and retrieves
 *                  the implementation structure from the factory.
 */
static nx_gpio_read_write_impl_t* get_gpio_impl(uint8_t port, uint8_t pin) {
    /* Validate parameters */
    if (port >= NX_GPIO_MAX_PORTS || pin >= NX_GPIO_MAX_PINS) {
        return NULL;
    }

    /* Convert port number to character (0='A', 1='B', etc.) */
    char port_char = 'A' + port;

    /* Get GPIO instance from factory */
    nx_gpio_read_write_t* gpio = nx_factory_gpio_read_write(port_char, pin);
    if (gpio == NULL) {
        return NULL;
    }

    return (nx_gpio_read_write_impl_t*)gpio;
}

/*---------------------------------------------------------------------------*/
/* Test Helper Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get GPIO device state
 */
nx_status_t native_gpio_get_state(uint8_t port, uint8_t pin,
                                  native_gpio_state_t* state) {
    /* Validate parameters */
    if (state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Get implementation */
    nx_gpio_read_write_impl_t* impl = get_gpio_impl(port, pin);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Copy state information */
    state->initialized = impl->state->initialized;
    state->suspended = impl->state->suspended;
    state->mode = impl->state->config.mode;
    state->pull = impl->state->config.pull;
    state->speed = impl->state->config.speed;
    state->pin_state = impl->state->pin_state;
    state->interrupt_enabled = impl->state->exti.enabled;
    state->trigger = impl->state->exti.trigger;
    state->read_count = impl->state->stats.read_count;
    state->write_count = impl->state->stats.write_count;
    state->toggle_count = impl->state->stats.toggle_count;
    state->exti_count = impl->state->stats.exti_count;

    return NX_OK;
}

/**
 * \brief           Simulate GPIO pin change
 * \details         Changes the pin state and triggers interrupt if configured.
 *                  This simulates an external signal change on an input pin.
 */
nx_status_t native_gpio_simulate_pin_change(uint8_t port, uint8_t pin,
                                            uint8_t level) {
    /* Get implementation */
    nx_gpio_read_write_impl_t* impl = get_gpio_impl(port, pin);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Normalize level to 0 or 1 */
    uint8_t new_level = (level != 0) ? 1 : 0;
    uint8_t old_level = impl->state->pin_state;

    /* Update pin state */
    impl->state->pin_state = new_level;

    /* Check if interrupt should be triggered */
    if (impl->state->exti.enabled && impl->state->exti.callback != NULL) {
        bool trigger_interrupt = false;

        switch (impl->state->exti.trigger) {
            case NX_GPIO_TRIGGER_RISING:
                /* Trigger on 0 -> 1 transition */
                if (old_level == 0 && new_level == 1) {
                    trigger_interrupt = true;
                }
                break;

            case NX_GPIO_TRIGGER_FALLING:
                /* Trigger on 1 -> 0 transition */
                if (old_level == 1 && new_level == 0) {
                    trigger_interrupt = true;
                }
                break;

            case NX_GPIO_TRIGGER_BOTH:
                /* Trigger on any transition */
                if (old_level != new_level) {
                    trigger_interrupt = true;
                }
                break;

            default:
                break;
        }

        /* Trigger callback if conditions met */
        if (trigger_interrupt) {
            impl->state->stats.exti_count++;
            impl->state->exti.callback(impl->state->exti.user_data);
        }
    }

    return NX_OK;
}

/**
 * \brief           Check if GPIO interrupt was triggered
 * \details         Returns true if the interrupt counter is non-zero.
 *                  This can be used to verify that an interrupt occurred.
 */
bool native_gpio_is_interrupt_triggered(uint8_t port, uint8_t pin) {
    /* Get implementation */
    nx_gpio_read_write_impl_t* impl = get_gpio_impl(port, pin);
    if (impl == NULL || impl->state == NULL) {
        return false;
    }

    return impl->state->stats.exti_count > 0;
}

/**
 * \brief           Reset specific GPIO instance
 * \details         Resets the GPIO instance to its initial state, clearing
 *                  all configuration, state, and statistics.
 */
nx_status_t native_gpio_reset(uint8_t port, uint8_t pin) {
    /* Get implementation */
    nx_gpio_read_write_impl_t* impl = get_gpio_impl(port, pin);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Reset state */
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->pin_state = 0;

    /* Reset interrupt context */
    impl->state->exti.callback = NULL;
    impl->state->exti.user_data = NULL;
    impl->state->exti.trigger = NX_GPIO_TRIGGER_RISING;
    impl->state->exti.enabled = false;

    /* Reset statistics */
    memset(&impl->state->stats, 0, sizeof(nx_gpio_stats_t));

    return NX_OK;
}

/**
 * \brief           Reset all GPIO instances
 * \details         Iterates through all possible GPIO instances and resets
 *                  each one to its initial state. Skips instances that don't
 * exist.
 */
void native_gpio_reset_all(void) {
    for (uint8_t port = 0; port < NX_GPIO_MAX_PORTS; port++) {
        for (uint8_t pin = 0; pin < NX_GPIO_MAX_PINS; pin++) {
            /* Ignore errors - device might not exist */
            (void)native_gpio_reset(port, pin);
        }
    }
}
