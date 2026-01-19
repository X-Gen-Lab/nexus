/**
 * \file            nx_gpio_test.h
 * \brief           GPIO test support functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_GPIO_TEST_H
#define NX_GPIO_TEST_H

#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Test Support Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Trigger external interrupt (for testing)
 * \param[in]       port: GPIO port number (0=A, 1=B, ...)
 * \param[in]       pin: GPIO pin number (0-15)
 * \param[in]       pin_state: New pin state (0 or 1)
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_gpio_native_trigger_exti(uint8_t port, uint8_t pin,
                                        uint8_t pin_state);

/**
 * \brief           Get GPIO pin state (for testing)
 * \param[in]       port: GPIO port number (0=A, 1=B, ...)
 * \param[in]       pin: GPIO pin number (0-15)
 * \return          Pin state (0 or 1)
 * \note            This function is for testing purposes only
 */
uint8_t nx_gpio_native_get_pin_state(uint8_t port, uint8_t pin);

/**
 * \brief           Get GPIO state (for testing)
 * \param[in]       port: GPIO port number (0=A, 1=B, ...)
 * \param[in]       pin: GPIO pin number (0-15)
 * \param[out]      initialized: Initialization flag
 * \param[out]      suspended: Suspend flag
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_gpio_native_get_state(uint8_t port, uint8_t pin,
                                     bool* initialized, bool* suspended);

/**
 * \brief           Reset GPIO instance (for testing)
 * \param[in]       port: GPIO port number (0=A, 1=B, ...)
 * \param[in]       pin: GPIO pin number (0-15)
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_gpio_native_reset(uint8_t port, uint8_t pin);

/**
 * \brief           Reset all GPIO instances (for testing)
 * \note            This function is for testing purposes only
 */
void nx_gpio_native_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif /* NX_GPIO_TEST_H */
