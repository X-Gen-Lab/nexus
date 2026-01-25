/**
 * \file            nx_gpio_helpers.c
 * \brief           GPIO helper functions implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements GPIO helper functions for external interrupt
 *                  triggering and state management.
 */

#include "nx_gpio_helpers.h"
#include "hal/base/nx_device.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Helper Functions Implementation                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Trigger external interrupt for testing
 * \note            Frequently called functions moved to inline in header
 */
void gpio_trigger_exti(nx_gpio_state_t* state, uint8_t pin_state) {
    if (!state || !state->exti.enabled || !state->exti.callback) {
        return;
    }

    /* Check trigger condition */
    bool should_trigger = false;
    uint8_t old_state = state->pin_state;
    uint8_t new_state = pin_state ? 1 : 0;

    switch (state->exti.trigger) {
        case NX_GPIO_TRIGGER_RISING:
            should_trigger = (old_state == 0 && new_state == 1);
            break;
        case NX_GPIO_TRIGGER_FALLING:
            should_trigger = (old_state == 1 && new_state == 0);
            break;
        case NX_GPIO_TRIGGER_BOTH:
            should_trigger = (old_state != new_state);
            break;
        default:
            break;
    }

    /* Update pin state */
    state->pin_state = new_state;

    /* Trigger callback if condition met */
    if (should_trigger) {
        state->stats.exti_count++;
        state->exti.callback(state->exti.user_data);
    }
}

/**
 * \brief           Reset GPIO state for testing
 */
void gpio_reset_state(nx_gpio_state_t* state) {
    if (!state) {
        return;
    }

    /* Reset statistics */
    memset(&state->stats, 0, sizeof(nx_gpio_stats_t));

    /* Reset interrupt context */
    state->exti.callback = NULL;
    state->exti.user_data = NULL;
    state->exti.trigger = NX_GPIO_TRIGGER_RISING;
    state->exti.enabled = false;

    /* Reset pin state */
    state->pin_state = 0;
}

/*---------------------------------------------------------------------------*/
/* Configuration Validation Functions                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Validate GPIO port number
 * \details         Checks if port number is within valid range (0-7)
 *                  corresponding to GPIO ports A-H
 */
bool nx_gpio_validate_port(uint8_t port) {
    return port < 8;
}

/**
 * \brief           Validate GPIO pin number
 * \details         Checks if pin number is within valid range (0-15)
 *                  as each GPIO port has 16 pins
 */
bool nx_gpio_validate_pin(uint8_t pin) {
    return pin < 16;
}

/**
 * \brief           Validate GPIO configuration
 * \details         Validates both port and pin numbers to ensure they
 *                  are within acceptable ranges before device registration
 */
bool nx_gpio_validate_config(uint8_t port, uint8_t pin) {
    return nx_gpio_validate_port(port) && nx_gpio_validate_pin(pin);
}
