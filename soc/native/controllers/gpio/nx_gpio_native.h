/**
 * \file            nx_gpio_native.h
 * \brief           GPIO native platform public interface
 * \author          Nexus Team
 */

#ifndef NX_GPIO_NATIVE_H
#define NX_GPIO_NATIVE_H

#include "hal/base/nx_device.h"
#include "hal/interface/nx_gpio.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Public API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get GPIO read interface by port and pin
 * \param[in]       port: GPIO port number (0=A, 1=B, ...)
 * \param[in]       pin: GPIO pin number (0-15)
 * \return          GPIO read interface pointer, NULL if not enabled
 */
nx_gpio_read_t* nx_gpio_native_get_read(uint8_t port, uint8_t pin);

/**
 * \brief           Get GPIO write interface by port and pin
 * \param[in]       port: GPIO port number (0=A, 1=B, ...)
 * \param[in]       pin: GPIO pin number (0-15)
 * \return          GPIO write interface pointer, NULL if not enabled
 */
nx_gpio_write_t* nx_gpio_native_get_write(uint8_t port, uint8_t pin);

/**
 * \brief           Get GPIO read-write interface by port and pin
 * \param[in]       port: GPIO port number (0=A, 1=B, ...)
 * \param[in]       pin: GPIO pin number (0-15)
 * \return          GPIO read-write interface pointer, NULL if not enabled
 */
nx_gpio_read_write_t* nx_gpio_native_get_read_write(uint8_t port, uint8_t pin);

/**
 * \brief           Get GPIO device descriptor by port and pin
 * \param[in]       port: GPIO port number (0=A, 1=B, ...)
 * \param[in]       pin: GPIO pin number (0-15)
 * \return          Device descriptor pointer, NULL if invalid
 */
nx_device_t* nx_gpio_native_get_device(uint8_t port, uint8_t pin);

/**
 * \brief           Simulate GPIO EXTI trigger (for testing)
 * \param[in]       port: GPIO port number (0=A, 1=B, ...)
 * \param[in]       pin: GPIO pin number (0-15)
 */
void nx_gpio_native_simulate_exti(uint8_t port, uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif /* NX_GPIO_NATIVE_H */
