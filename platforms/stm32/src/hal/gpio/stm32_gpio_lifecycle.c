/**
 * \file            stm32_gpio_lifecycle.c
 * \brief           STM32 GPIO lifecycle management
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements GPIO lifecycle management (init/deinit)
 */

#include "hal/nx_types.h"
#include "stm32_gpio.h"

/*---------------------------------------------------------------------------*/
/* Hardware Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize GPIO hardware
 */
nx_status_t stm32_gpio_hw_init(stm32_gpio_state_t* state) {
    NX_ASSERT(state && state->config);

    if (state->initialized) {
        return NX_OK;
    }

    /* Configure GPIO */
    GPIO_InitTypeDef gpio_init = {0};
    gpio_init.Pin = state->config->pin;
    gpio_init.Mode = state->config->mode;
    gpio_init.Pull = state->config->pull;
    gpio_init.Speed = state->config->speed;
    gpio_init.Alternate = state->config->alternate;

    HAL_GPIO_Init(state->config->port, &gpio_init);

    /* Set initial value for output pins */
    if (state->config->mode == GPIO_MODE_OUTPUT_PP ||
        state->config->mode == GPIO_MODE_OUTPUT_OD) {
        HAL_GPIO_WritePin(state->config->port, state->config->pin,
                          state->config->init_value ? GPIO_PIN_SET
                                                    : GPIO_PIN_RESET);
    }

    state->initialized = true;
    return NX_OK;
}

/**
 * \brief           Deinitialize GPIO hardware
 */
void stm32_gpio_hw_deinit(stm32_gpio_state_t* state) {
    NX_ASSERT(state && state->config);

    if (!state->initialized) {
        return;
    }

    HAL_GPIO_DeInit(state->config->port, state->config->pin);
    state->initialized = false;
}

/*---------------------------------------------------------------------------*/
/* Unified Lifecycle Implementation                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Unified lifecycle init implementation
 */
static nx_status_t stm32_gpio_lifecycle_init_impl(stm32_gpio_state_t* state) {
    return stm32_gpio_hw_init(state);
}

/**
 * \brief           Unified lifecycle deinit implementation
 */
static nx_status_t stm32_gpio_lifecycle_deinit_impl(stm32_gpio_state_t* state) {
    stm32_gpio_hw_deinit(state);
    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Read Mode Lifecycle Wrappers                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Lifecycle init for read mode
 */
static nx_status_t stm32_gpio_lifecycle_read_init(nx_lifecycle_t* self) {
    stm32_gpio_read_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_read_impl_t, lifecycle);
    NX_ASSERT(impl && impl->state);
    return stm32_gpio_lifecycle_init_impl(impl->state);
}

/**
 * \brief           Lifecycle deinit for read mode
 */
static nx_status_t stm32_gpio_lifecycle_read_deinit(nx_lifecycle_t* self) {
    stm32_gpio_read_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_read_impl_t, lifecycle);
    NX_ASSERT(impl && impl->state);
    return stm32_gpio_lifecycle_deinit_impl(impl->state);
}

/*---------------------------------------------------------------------------*/
/* Write Mode Lifecycle Wrappers                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Lifecycle init for write mode
 */
static nx_status_t stm32_gpio_lifecycle_write_init(nx_lifecycle_t* self) {
    stm32_gpio_write_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_write_impl_t, lifecycle);
    NX_ASSERT(impl && impl->state);
    return stm32_gpio_lifecycle_init_impl(impl->state);
}

/**
 * \brief           Lifecycle deinit for write mode
 */
static nx_status_t stm32_gpio_lifecycle_write_deinit(nx_lifecycle_t* self) {
    stm32_gpio_write_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_write_impl_t, lifecycle);
    NX_ASSERT(impl && impl->state);
    return stm32_gpio_lifecycle_deinit_impl(impl->state);
}

/*---------------------------------------------------------------------------*/
/* Read-Write Mode Lifecycle Wrappers                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Lifecycle init for read-write mode
 */
static nx_status_t stm32_gpio_lifecycle_read_write_init(nx_lifecycle_t* self) {
    stm32_gpio_read_write_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_read_write_impl_t, lifecycle);
    NX_ASSERT(impl && impl->state);
    return stm32_gpio_lifecycle_init_impl(impl->state);
}

/**
 * \brief           Lifecycle deinit for read-write mode
 */
static nx_status_t
stm32_gpio_lifecycle_read_write_deinit(nx_lifecycle_t* self) {
    stm32_gpio_read_write_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_read_write_impl_t, lifecycle);
    NX_ASSERT(impl && impl->state);
    return stm32_gpio_lifecycle_deinit_impl(impl->state);
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize lifecycle interface for read mode
 */
void stm32_gpio_init_lifecycle_read(nx_lifecycle_t* lifecycle) {
    lifecycle->init = stm32_gpio_lifecycle_read_init;
    lifecycle->deinit = stm32_gpio_lifecycle_read_deinit;
}

/**
 * \brief           Initialize lifecycle interface for write mode
 */
void stm32_gpio_init_lifecycle_write(nx_lifecycle_t* lifecycle) {
    lifecycle->init = stm32_gpio_lifecycle_write_init;
    lifecycle->deinit = stm32_gpio_lifecycle_write_deinit;
}

/**
 * \brief           Initialize lifecycle interface for read-write mode
 */
void stm32_gpio_init_lifecycle_read_write(nx_lifecycle_t* lifecycle) {
    lifecycle->init = stm32_gpio_lifecycle_read_write_init;
    lifecycle->deinit = stm32_gpio_lifecycle_read_write_deinit;
}
