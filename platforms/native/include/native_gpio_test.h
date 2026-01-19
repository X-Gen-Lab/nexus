/**
 * \file            native_gpio_test.h
 * \brief           Native GPIO Test Helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_GPIO_TEST_H
#define NATIVE_GPIO_TEST_H

#include "hal/interface/nx_gpio.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Type Definitions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO configuration structure (opaque)
 */
typedef struct nx_gpio_config_s nx_gpio_config_t;

/**
 * \brief           Simulated GPIO state
 */
typedef struct {
    bool configured; /**< Pin configured flag */
    bool is_output;  /**< Output mode flag */
    bool level;      /**< Current level */
} native_gpio_pin_t;

/*---------------------------------------------------------------------------*/
/* Factory Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get GPIO instance (factory function)
 * \param[in]       port: GPIO port number (0=A, 1=B, ...)
 * \param[in]       pin: GPIO pin number (0-15)
 * \return          GPIO interface pointer, NULL on failure
 */
nx_gpio_t* nx_gpio_native_get(uint8_t port, uint8_t pin);

/**
 * \brief           Get GPIO instance with configuration
 * \param[in]       port: GPIO port number (0=A, 1=B, ...)
 * \param[in]       pin: GPIO pin number (0-15)
 * \param[in]       cfg: GPIO configuration
 * \return          GPIO interface pointer, NULL on failure
 */
nx_gpio_t* nx_gpio_native_get_with_config(uint8_t port, uint8_t pin,
                                          const nx_gpio_config_t* cfg);

/*---------------------------------------------------------------------------*/
/* Reset Functions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset all simulated GPIO states
 */
void native_gpio_reset_all(void);

/**
 * \brief           Reset all GPIO instances (for testing)
 */
void nx_gpio_native_reset_all(void);

/*---------------------------------------------------------------------------*/
/* State Query Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get simulated GPIO state (for testing)
 * \param[in]       port: GPIO port
 * \param[in]       pin: GPIO pin
 * \return          Pointer to pin state or NULL
 */
native_gpio_pin_t* native_gpio_get_state(uint8_t port, uint8_t pin);

/*---------------------------------------------------------------------------*/
/* GPIO-Specific Test Helpers                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Simulate GPIO EXTI trigger (for testing)
 * \param[in]       port: GPIO port number (0=A, 1=B, ...)
 * \param[in]       pin: GPIO pin number (0-15)
 * \note            This function is for testing purposes only
 */
void nx_gpio_native_simulate_exti(uint8_t port, uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_GPIO_TEST_H */
