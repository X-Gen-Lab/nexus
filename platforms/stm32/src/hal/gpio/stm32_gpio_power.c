/**
 * \file            stm32_gpio_power.c
 * \brief           STM32 GPIO power management
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements GPIO power management (suspend/resume)
 */

#include "hal/nx_types.h"
#include "stm32_gpio.h"

/*---------------------------------------------------------------------------*/
/* Unified Power Implementation                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Unified power suspend implementation
 */
static nx_status_t stm32_gpio_power_suspend_impl(stm32_gpio_state_t* state) {
    NX_ASSERT(state);

    if (!state->initialized || state->suspended) {
        return NX_OK;
    }

    state->suspended = true;
    return NX_OK;
}

/**
 * \brief           Unified power resume implementation
 */
static nx_status_t stm32_gpio_power_resume_impl(stm32_gpio_state_t* state) {
    NX_ASSERT(state);

    if (!state->initialized || !state->suspended) {
        return NX_OK;
    }

    state->suspended = false;
    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Read Mode Power Wrappers                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Power suspend for read mode
 */
static nx_status_t stm32_gpio_power_read_suspend(nx_power_t* self) {
    stm32_gpio_read_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_read_impl_t, power);
    NX_ASSERT(impl && impl->state);
    return stm32_gpio_power_suspend_impl(impl->state);
}

/**
 * \brief           Power resume for read mode
 */
static nx_status_t stm32_gpio_power_read_resume(nx_power_t* self) {
    stm32_gpio_read_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_read_impl_t, power);
    NX_ASSERT(impl && impl->state);
    return stm32_gpio_power_resume_impl(impl->state);
}

/*---------------------------------------------------------------------------*/
/* Write Mode Power Wrappers                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Power suspend for write mode
 */
static nx_status_t stm32_gpio_power_write_suspend(nx_power_t* self) {
    stm32_gpio_write_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_write_impl_t, power);
    NX_ASSERT(impl && impl->state);
    return stm32_gpio_power_suspend_impl(impl->state);
}

/**
 * \brief           Power resume for write mode
 */
static nx_status_t stm32_gpio_power_write_resume(nx_power_t* self) {
    stm32_gpio_write_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_write_impl_t, power);
    NX_ASSERT(impl && impl->state);
    return stm32_gpio_power_resume_impl(impl->state);
}

/*---------------------------------------------------------------------------*/
/* Read-Write Mode Power Wrappers                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Power suspend for read-write mode
 */
static nx_status_t stm32_gpio_power_read_write_suspend(nx_power_t* self) {
    stm32_gpio_read_write_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_read_write_impl_t, power);
    NX_ASSERT(impl && impl->state);
    return stm32_gpio_power_suspend_impl(impl->state);
}

/**
 * \brief           Power resume for read-write mode
 */
static nx_status_t stm32_gpio_power_read_write_resume(nx_power_t* self) {
    stm32_gpio_read_write_impl_t* impl =
        NX_CONTAINER_OF(self, stm32_gpio_read_write_impl_t, power);
    NX_ASSERT(impl && impl->state);
    return stm32_gpio_power_resume_impl(impl->state);
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize power interface for read mode
 */
void stm32_gpio_init_power_read(nx_power_t* power) {
    power->enable = stm32_gpio_power_read_resume;
    power->disable = stm32_gpio_power_read_suspend;
}

/**
 * \brief           Initialize power interface for write mode
 */
void stm32_gpio_init_power_write(nx_power_t* power) {
    power->enable = stm32_gpio_power_write_resume;
    power->disable = stm32_gpio_power_write_suspend;
}

/**
 * \brief           Initialize power interface for read-write mode
 */
void stm32_gpio_init_power_read_write(nx_power_t* power) {
    power->enable = stm32_gpio_power_read_write_resume;
    power->disable = stm32_gpio_power_read_write_suspend;
}
