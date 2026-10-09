/**
 * \file            stm32_gpio.h
 * \brief           STM32 GPIO internal header
 * \author          Nexus Team
 */

#ifndef STM32_GPIO_H
#define STM32_GPIO_H

#include "stm32_gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Hardware Operations                                                       */
/*---------------------------------------------------------------------------*/

nx_status_t stm32_gpio_hw_init(stm32_gpio_state_t* state);
void stm32_gpio_hw_deinit(stm32_gpio_state_t* state);

#ifdef __cplusplus
}
#endif

#endif /* STM32_GPIO_H */
