/**
 * \file            nx_gpio_lifecycle.c
 * \brief           GPIO lifecycle interface implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements GPIO lifecycle operations including init,
 *                  deinit, suspend, resume, and state query functions.
 */

#include "hal/nx_status.h"
#include "nx_gpio_helpers.h"
#include "nx_gpio_types.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* GPIO Lifecycle Interface Implementation                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize GPIO
 */
static nx_status_t gpio_lifecycle_init(nx_lifecycle_t* self) {
    nx_gpio_read_write_impl_t* impl =
        (nx_gpio_read_write_impl_t*)((char*)self -
                                     offsetof(nx_gpio_read_write_impl_t,
                                              lifecycle));

    /* Parameter check */
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    /* Check if already initialized */
    if (impl->state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    /* Initialize pin state based on mode */
    if (impl->state->config.mode == NX_GPIO_MODE_OUTPUT_PP ||
        impl->state->config.mode == NX_GPIO_MODE_OUTPUT_OD) {
        /* Output mode - set to low by default */
        impl->state->pin_state = 0;
    } else {
        /* Input mode - read current state (simulated as 0) */
        impl->state->pin_state = 0;
    }

    /* Mark as initialized */
    impl->state->initialized = true;
    impl->state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Deinitialize GPIO
 */
static nx_status_t gpio_lifecycle_deinit(nx_lifecycle_t* self) {
    nx_gpio_read_write_impl_t* impl =
        (nx_gpio_read_write_impl_t*)((char*)self -
                                     offsetof(nx_gpio_read_write_impl_t,
                                              lifecycle));

    /* Parameter check */
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    /* Check if initialized */
    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Clear interrupt context */
    impl->state->exti.callback = NULL;
    impl->state->exti.user_data = NULL;
    impl->state->exti.enabled = false;

    /* Mark as uninitialized */
    impl->state->initialized = false;

    return NX_OK;
}

/**
 * \brief           Suspend GPIO
 */
static nx_status_t gpio_lifecycle_suspend(nx_lifecycle_t* self) {
    nx_gpio_read_write_impl_t* impl =
        (nx_gpio_read_write_impl_t*)((char*)self -
                                     offsetof(nx_gpio_read_write_impl_t,
                                              lifecycle));

    /* Parameter check */
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    /* Check if initialized */
    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Check if already suspended */
    if (impl->state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Mark as suspended */
    impl->state->suspended = true;

    return NX_OK;
}

/**
 * \brief           Resume GPIO
 */
static nx_status_t gpio_lifecycle_resume(nx_lifecycle_t* self) {
    nx_gpio_read_write_impl_t* impl =
        (nx_gpio_read_write_impl_t*)((char*)self -
                                     offsetof(nx_gpio_read_write_impl_t,
                                              lifecycle));

    /* Parameter check */
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    /* Check if initialized */
    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Check if not suspended */
    if (!impl->state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    /* Mark as resumed */
    impl->state->suspended = false;

    return NX_OK;
}

/**
 * \brief           Get GPIO state
 */
static nx_device_state_t gpio_lifecycle_get_state(nx_lifecycle_t* self) {
    nx_gpio_read_write_impl_t* impl =
        (nx_gpio_read_write_impl_t*)((char*)self -
                                     offsetof(nx_gpio_read_write_impl_t,
                                              lifecycle));

    /* Parameter check */
    if (!impl || !impl->state) {
        return NX_DEV_STATE_ERROR;
    }

    /* Return state */
    if (!impl->state->initialized) {
        return NX_DEV_STATE_UNINITIALIZED;
    }
    if (impl->state->suspended) {
        return NX_DEV_STATE_SUSPENDED;
    }
    return NX_DEV_STATE_RUNNING;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize GPIO lifecycle interface
 */
void gpio_init_lifecycle(nx_lifecycle_t* lifecycle) {
    lifecycle->init = gpio_lifecycle_init;
    lifecycle->deinit = gpio_lifecycle_deinit;
    lifecycle->suspend = gpio_lifecycle_suspend;
    lifecycle->resume = gpio_lifecycle_resume;
    lifecycle->get_state = gpio_lifecycle_get_state;
}
