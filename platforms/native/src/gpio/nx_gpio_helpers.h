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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Port Conversion Macros                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Convert port character to port number
 * \param[in]       port_char: Port character ('A', 'B', 'C', etc.)
 * \return          Port number (0, 1, 2, etc.)
 * \note            Example: 'A' -> 0, 'B' -> 1, 'C' -> 2
 */
#define NX_GPIO_PORT_NUM(port_char) ((uint8_t)((port_char) - 'A'))

/**
 * \brief           Convert port number to port character
 * \param[in]       port_num: Port number (0, 1, 2, etc.)
 * \return          Port character ('A', 'B', 'C', etc.)
 * \note            Example: 0 -> 'A', 1 -> 'B', 2 -> 'C'
 */
#define NX_GPIO_PORT_CHAR(port_num) ((char)('A' + (port_num)))

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

/*---------------------------------------------------------------------------*/
/* Configuration Validation Functions                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Validate GPIO port number
 * \param[in]       port: Port number to validate
 * \return          true if port is valid (0-7), false otherwise
 * \note            Valid port range is 0-7 (A-H)
 */
bool nx_gpio_validate_port(uint8_t port);

/**
 * \brief           Validate GPIO pin number
 * \param[in]       pin: Pin number to validate
 * \return          true if pin is valid (0-15), false otherwise
 * \note            Valid pin range is 0-15
 */
bool nx_gpio_validate_pin(uint8_t pin);

/**
 * \brief           Validate GPIO configuration
 * \param[in]       port: Port number to validate
 * \param[in]       pin: Pin number to validate
 * \return          true if both port and pin are valid, false otherwise
 * \note            Validates both port (0-7) and pin (0-15) ranges
 */
bool nx_gpio_validate_config(uint8_t port, uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif /* NX_GPIO_HELPERS_H */
