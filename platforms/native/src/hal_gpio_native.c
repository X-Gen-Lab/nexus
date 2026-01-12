/**
 * \file            hal_gpio_native.c
 * \brief           Native Platform GPIO HAL Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "hal/hal_gpio.h"
#include "native_platform.h"
#include <string.h>

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

#define MAX_PORTS   8
#define MAX_PINS    16

/**
 * \brief           Simulated GPIO state array
 */
static native_gpio_pin_t gpio_state[MAX_PORTS][MAX_PINS];

/*===========================================================================*/
/* Public functions                                                           */
/*===========================================================================*/

native_gpio_pin_t* native_gpio_get_state(uint8_t port, uint8_t pin)
{
    if (port >= MAX_PORTS || pin >= MAX_PINS) {
        return NULL;
    }
    return &gpio_state[port][pin];
}

void native_gpio_reset_all(void)
{
    memset(gpio_state, 0, sizeof(gpio_state));
}

hal_status_t hal_gpio_init(hal_gpio_port_t port,
                           hal_gpio_pin_t pin,
                           const hal_gpio_config_t* config)
{
    native_gpio_pin_t* state;

    if (port >= MAX_PORTS || pin >= MAX_PINS || config == NULL) {
        return HAL_ERR_PARAM;
    }

    state = &gpio_state[port][pin];
    state->configured = true;
    state->is_output = (config->direction == HAL_GPIO_DIR_OUTPUT);
    state->level = (config->init_level == HAL_GPIO_LEVEL_HIGH);

    return HAL_OK;
}

hal_status_t hal_gpio_deinit(hal_gpio_port_t port, hal_gpio_pin_t pin)
{
    native_gpio_pin_t* state;

    if (port >= MAX_PORTS || pin >= MAX_PINS) {
        return HAL_ERR_PARAM;
    }

    state = &gpio_state[port][pin];
    state->configured = false;
    state->is_output = false;
    state->level = false;

    return HAL_OK;
}

hal_status_t hal_gpio_write(hal_gpio_port_t port,
                            hal_gpio_pin_t pin,
                            hal_gpio_level_t level)
{
    native_gpio_pin_t* state;

    if (port >= MAX_PORTS || pin >= MAX_PINS) {
        return HAL_ERR_PARAM;
    }

    state = &gpio_state[port][pin];
    if (!state->configured || !state->is_output) {
        return HAL_ERR_STATE;
    }

    state->level = (level == HAL_GPIO_LEVEL_HIGH);
    return HAL_OK;
}

hal_status_t hal_gpio_read(hal_gpio_port_t port,
                           hal_gpio_pin_t pin,
                           hal_gpio_level_t* level)
{
    native_gpio_pin_t* state;

    if (port >= MAX_PORTS || pin >= MAX_PINS || level == NULL) {
        return HAL_ERR_PARAM;
    }

    state = &gpio_state[port][pin];
    if (!state->configured) {
        return HAL_ERR_STATE;
    }

    *level = state->level ? HAL_GPIO_LEVEL_HIGH : HAL_GPIO_LEVEL_LOW;
    return HAL_OK;
}

hal_status_t hal_gpio_toggle(hal_gpio_port_t port, hal_gpio_pin_t pin)
{
    native_gpio_pin_t* state;

    if (port >= MAX_PORTS || pin >= MAX_PINS) {
        return HAL_ERR_PARAM;
    }

    state = &gpio_state[port][pin];
    if (!state->configured || !state->is_output) {
        return HAL_ERR_STATE;
    }

    state->level = !state->level;
    return HAL_OK;
}

hal_status_t hal_gpio_irq_config(hal_gpio_port_t port,
                                 hal_gpio_pin_t pin,
                                 hal_gpio_irq_mode_t mode,
                                 hal_gpio_irq_callback_t callback,
                                 void* context)
{
    (void)port;
    (void)pin;
    (void)mode;
    (void)callback;
    (void)context;
    return HAL_ERR_NOT_SUPPORTED;
}

hal_status_t hal_gpio_irq_enable(hal_gpio_port_t port, hal_gpio_pin_t pin)
{
    (void)port;
    (void)pin;
    return HAL_ERR_NOT_SUPPORTED;
}

hal_status_t hal_gpio_irq_disable(hal_gpio_port_t port, hal_gpio_pin_t pin)
{
    (void)port;
    (void)pin;
    return HAL_ERR_NOT_SUPPORTED;
}
