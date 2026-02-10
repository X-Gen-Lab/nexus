/**
 * \file            stm32_gpio_ops.c
 * \brief           STM32 GPIO operations implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements GPIO read, write, and read-write operations
 */

#include "hal/base/nx_device.h"
#include "hal/nx_types.h"
#include "stm32_gpio.h"

/*---------------------------------------------------------------------------*/
/* Read Operations                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Read GPIO pin state
 */
static uint8_t stm32_gpio_read_impl(nx_gpio_read_t* self) {
    stm32_gpio_read_impl_t* impl = stm32_gpio_read_get_impl(self);
    NX_ASSERT(impl && impl->state);

    stm32_gpio_state_t* state = impl->state;
    if (!state->initialized) {
        return 0;
    }

    state->stats.read_count++;
    return (uint8_t)HAL_GPIO_ReadPin(state->port, state->pin);
}

/**
 * \brief           Register external interrupt callback
 */
static nx_status_t stm32_gpio_register_exti_impl(nx_gpio_read_t* self,
                                                 nx_gpio_callback_t callback,
                                                 void* user_data,
                                                 nx_gpio_trigger_t trigger) {
    stm32_gpio_read_impl_t* impl = stm32_gpio_read_get_impl(self);
    NX_ASSERT(impl && impl->state);

    stm32_gpio_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    state->exti.callback = callback;
    state->exti.user_data = user_data;
    state->exti.trigger = trigger;
    state->exti.enabled = (callback != NULL);

    return NX_OK;
}

/**
 * \brief           Get lifecycle interface for read mode
 */
static nx_lifecycle_t* stm32_gpio_read_get_lifecycle(nx_gpio_read_t* self) {
    stm32_gpio_read_impl_t* impl = stm32_gpio_read_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface for read mode
 */
static nx_power_t* stm32_gpio_read_get_power(nx_gpio_read_t* self) {
    stm32_gpio_read_impl_t* impl = stm32_gpio_read_get_impl(self);
    return impl ? &impl->power : NULL;
}

/*---------------------------------------------------------------------------*/
/* Write Operations                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Write GPIO pin state
 */
static void stm32_gpio_write_impl(nx_gpio_write_t* self, uint8_t state) {
    stm32_gpio_write_impl_t* impl = stm32_gpio_write_get_impl(self);
    NX_ASSERT(impl && impl->state);

    stm32_gpio_state_t* gpio_state = impl->state;
    if (!gpio_state->initialized) {
        return;
    }

    gpio_state->stats.write_count++;
    HAL_GPIO_WritePin(gpio_state->port, gpio_state->pin,
                      state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * \brief           Toggle GPIO pin state
 */
static void stm32_gpio_toggle_impl(nx_gpio_write_t* self) {
    stm32_gpio_write_impl_t* impl = stm32_gpio_write_get_impl(self);
    NX_ASSERT(impl && impl->state);

    stm32_gpio_state_t* state = impl->state;
    if (!state->initialized) {
        return;
    }

    state->stats.toggle_count++;
    HAL_GPIO_TogglePin(state->port, state->pin);
}

/**
 * \brief           Get lifecycle interface for write mode
 */
static nx_lifecycle_t* stm32_gpio_write_get_lifecycle(nx_gpio_write_t* self) {
    stm32_gpio_write_impl_t* impl = stm32_gpio_write_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface for write mode
 */
static nx_power_t* stm32_gpio_write_get_power(nx_gpio_write_t* self) {
    stm32_gpio_write_impl_t* impl = stm32_gpio_write_get_impl(self);
    return impl ? &impl->power : NULL;
}

/*---------------------------------------------------------------------------*/
/* Read-Write Operations                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Read GPIO pin state (read-write mode)
 */
static uint8_t stm32_gpio_rw_read_impl(nx_gpio_read_t* self) {
    stm32_gpio_read_write_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_read_write_impl_t, base.read);
    NX_ASSERT(impl && impl->state);

    stm32_gpio_state_t* state = impl->state;
    if (!state->initialized) {
        return 0;
    }

    state->stats.read_count++;
    return (uint8_t)HAL_GPIO_ReadPin(state->port, state->pin);
}

/**
 * \brief           Register external interrupt callback (read-write mode)
 */
static nx_status_t stm32_gpio_rw_register_exti_impl(nx_gpio_read_t* self,
                                                    nx_gpio_callback_t callback,
                                                    void* user_data,
                                                    nx_gpio_trigger_t trigger) {
    stm32_gpio_read_write_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_read_write_impl_t, base.read);
    NX_ASSERT(impl && impl->state);

    stm32_gpio_state_t* state = impl->state;
    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    state->exti.callback = callback;
    state->exti.user_data = user_data;
    state->exti.trigger = trigger;
    state->exti.enabled = (callback != NULL);

    return NX_OK;
}

/**
 * \brief           Get lifecycle interface for read-write mode (read side)
 */
static nx_lifecycle_t* stm32_gpio_rw_read_get_lifecycle(nx_gpio_read_t* self) {
    stm32_gpio_read_write_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_read_write_impl_t, base.read);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface for read-write mode (read side)
 */
static nx_power_t* stm32_gpio_rw_read_get_power(nx_gpio_read_t* self) {
    stm32_gpio_read_write_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_read_write_impl_t, base.read);
    return impl ? &impl->power : NULL;
}

/**
 * \brief           Write GPIO pin state (read-write mode)
 */
static void stm32_gpio_rw_write_impl(nx_gpio_write_t* self, uint8_t state) {
    stm32_gpio_read_write_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_read_write_impl_t, base.write);
    NX_ASSERT(impl && impl->state);

    stm32_gpio_state_t* gpio_state = impl->state;
    if (!gpio_state->initialized) {
        return;
    }

    gpio_state->stats.write_count++;
    HAL_GPIO_WritePin(gpio_state->port, gpio_state->pin,
                      state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * \brief           Toggle GPIO pin state (read-write mode)
 */
static void stm32_gpio_rw_toggle_impl(nx_gpio_write_t* self) {
    stm32_gpio_read_write_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_read_write_impl_t, base.write);
    NX_ASSERT(impl && impl->state);

    stm32_gpio_state_t* state = impl->state;
    if (!state->initialized) {
        return;
    }

    state->stats.toggle_count++;
    HAL_GPIO_TogglePin(state->port, state->pin);
}

/**
 * \brief           Get lifecycle interface for read-write mode (write side)
 */
static nx_lifecycle_t*
stm32_gpio_rw_write_get_lifecycle(nx_gpio_write_t* self) {
    stm32_gpio_read_write_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_read_write_impl_t, base.write);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface for read-write mode (write side)
 */
static nx_power_t* stm32_gpio_rw_write_get_power(nx_gpio_write_t* self) {
    stm32_gpio_read_write_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_read_write_impl_t, base.write);
    return impl ? &impl->power : NULL;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize read interface
 */
void stm32_gpio_init_read(nx_gpio_read_t* read) {
    NX_INIT_GPIO_READ(read, stm32_gpio_read_impl, stm32_gpio_register_exti_impl,
                      stm32_gpio_read_get_lifecycle, stm32_gpio_read_get_power);
}

/**
 * \brief           Initialize write interface
 */
void stm32_gpio_init_write(nx_gpio_write_t* write) {
    NX_INIT_GPIO_WRITE(write, stm32_gpio_write_impl, stm32_gpio_toggle_impl,
                       stm32_gpio_write_get_lifecycle,
                       stm32_gpio_write_get_power);
}

/**
 * \brief           Initialize read-write interface
 */
void stm32_gpio_init_read_write(nx_gpio_read_write_t* read_write) {
    NX_INIT_GPIO_READ_WRITE(
        read_write, stm32_gpio_rw_read_impl, stm32_gpio_rw_register_exti_impl,
        stm32_gpio_rw_write_impl, stm32_gpio_rw_toggle_impl,
        stm32_gpio_rw_read_get_lifecycle, stm32_gpio_rw_read_get_power,
        stm32_gpio_rw_write_get_lifecycle, stm32_gpio_rw_write_get_power);
}
