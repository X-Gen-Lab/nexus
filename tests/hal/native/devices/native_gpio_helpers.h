/**
 * \file            native_gpio_helpers.h
 * \brief           Native GPIO test helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_GPIO_HELPERS_H
#define NATIVE_GPIO_HELPERS_H

#include "hal/interface/nx_gpio.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* GPIO State Structure for Testing                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO state structure for testing
 *
 * Contains runtime state information that can be queried by tests.
 */
typedef struct native_gpio_state_s {
    bool initialized;          /**< Initialization flag */
    bool suspended;            /**< Suspend flag */
    uint8_t mode;              /**< GPIO mode */
    uint8_t pull;              /**< Pull-up/pull-down configuration */
    uint8_t speed;             /**< GPIO speed */
    uint8_t pin_state;         /**< Current pin state (0 or 1) */
    bool interrupt_enabled;    /**< Interrupt enabled flag */
    nx_gpio_trigger_t trigger; /**< Interrupt trigger type */
    uint32_t read_count;       /**< Number of read operations */
    uint32_t write_count;      /**< Number of write operations */
    uint32_t toggle_count;     /**< Number of toggle operations */
    uint32_t exti_count;       /**< Number of external interrupts */
} native_gpio_state_t;

/*---------------------------------------------------------------------------*/
/* GPIO Test Helpers                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get GPIO device state
 * \param[in]       port: GPIO port (0=A, 1=B, etc.)
 * \param[in]       pin: GPIO pin number (0-15)
 * \param[out]      state: State structure to fill
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_gpio_get_state(uint8_t port, uint8_t pin,
                                  native_gpio_state_t* state);

/**
 * \brief           Simulate GPIO pin change (for interrupt testing)
 * \param[in]       port: GPIO port (0=A, 1=B, etc.)
 * \param[in]       pin: GPIO pin number (0-15)
 * \param[in]       level: New pin level (0 or 1)
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_gpio_simulate_pin_change(uint8_t port, uint8_t pin,
                                            uint8_t level);

/**
 * \brief           Check if GPIO interrupt was triggered
 * \param[in]       port: GPIO port (0=A, 1=B, etc.)
 * \param[in]       pin: GPIO pin number (0-15)
 * \return          true if interrupt was triggered, false otherwise
 */
bool native_gpio_is_interrupt_triggered(uint8_t port, uint8_t pin);

/**
 * \brief           Reset specific GPIO instance to initial state
 * \param[in]       port: GPIO port (0=A, 1=B, etc.)
 * \param[in]       pin: GPIO pin number (0-15)
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_gpio_reset(uint8_t port, uint8_t pin);

/**
 * \brief           Reset all GPIO instances to initial state
 */
void native_gpio_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_GPIO_HELPERS_H */
