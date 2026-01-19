/**
 * \file            nx_gpio_helpers.h
 * \brief           GPIO helper functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_GPIO_HELPERS_H
#define NX_GPIO_HELPERS_H

#include "hal/nx_status.h"
#include "nx_gpio_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get read implementation from base interface
 * \param[in]       self: GPIO read interface pointer
 * \return          Implementation pointer or NULL
 * \note            Optimized as inline for performance
 */
static inline nx_gpio_read_impl_t* gpio_read_get_impl(nx_gpio_read_t* self) {
    return self ? (nx_gpio_read_impl_t*)self : NULL;
}

/**
 * \brief           Get write implementation from base interface
 * \param[in]       self: GPIO write interface pointer
 * \return          Implementation pointer or NULL
 * \note            Optimized as inline for performance
 */
static inline nx_gpio_write_impl_t* gpio_write_get_impl(nx_gpio_write_t* self) {
    return self ? (nx_gpio_write_impl_t*)self : NULL;
}

/**
 * \brief           Get read-write implementation from base interface
 * \param[in]       self: GPIO read-write interface pointer
 * \return          Implementation pointer or NULL
 * \note            Optimized as inline for performance
 */
static inline nx_gpio_read_write_impl_t*
gpio_read_write_get_impl(nx_gpio_read_write_t* self) {
    return self ? (nx_gpio_read_write_impl_t*)self : NULL;
}

/**
 * \brief           Get pin state for testing
 * \param[in]       state: GPIO state pointer
 * \return          Current pin state (0 or 1)
 * \note            This function is for testing purposes only
 * \note            Optimized as inline for performance
 */
static inline uint8_t gpio_get_pin_state(const nx_gpio_state_t* state) {
    return state ? state->pin_state : 0;
}

/**
 * \brief           Trigger external interrupt for testing
 * \param[in]       state: GPIO state pointer
 * \param[in]       pin_state: New pin state (0 or 1)
 * \note            This function is for testing purposes only
 */
void gpio_trigger_exti(nx_gpio_state_t* state, uint8_t pin_state);

/**
 * \brief           Reset GPIO state for testing
 * \param[in]       state: GPIO state pointer
 * \note            This function is for testing purposes only
 */
void gpio_reset_state(nx_gpio_state_t* state);

#ifdef __cplusplus
}
#endif

#endif /* NX_GPIO_HELPERS_H */
