/**
 * \file            native_platform.h
 * \brief           Native Platform Header (for host testing)
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef NATIVE_PLATFORM_H
#define NATIVE_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Simulated GPIO state
 */
typedef struct {
    bool     configured;    /**< Pin configured flag */
    bool     is_output;     /**< Output mode flag */
    bool     level;         /**< Current level */
} native_gpio_pin_t;

/**
 * \brief           Get simulated GPIO state (for testing)
 * \param[in]       port: GPIO port
 * \param[in]       pin: GPIO pin
 * \return          Pointer to pin state or NULL
 */
native_gpio_pin_t* native_gpio_get_state(uint8_t port, uint8_t pin);

/**
 * \brief           Reset all simulated GPIO states
 */
void native_gpio_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_PLATFORM_H */
