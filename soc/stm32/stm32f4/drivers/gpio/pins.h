/**
 * \file            pins.h
 * \brief           Shared STM32F4 cold pin capture and electrical restoration
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_GPIO_PINS_H
#define NX_STM32F4_GPIO_PINS_H

#include "nexus/core/status.h"
#include "private/model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Configure a reviewed pin in a cold control path. */
nx_result_t nx_stm32_pin_configure(GPIO_TypeDef* gpio, uint8_t pin,
                                   uint8_t mode, uint8_t af, uint8_t pull,
                                   bool open_drain);
/** \brief Save only one pin's electrical state for cold-start rollback. */
uint16_t nx_stm32_pin_capture(const GPIO_TypeDef* gpio, uint8_t pin);
/** \brief Restore a captured pin without changing neighboring pin fields. */
void nx_stm32_pin_restore(GPIO_TypeDef* gpio, uint8_t pin, uint16_t state);
/** \brief Enable one selected device GPIO bank with ordered readback. */
nx_result_t nx_stm32_gpio_clock_enable(unsigned port_index);

#ifdef __cplusplus
}
#endif
#endif /* NX_STM32F4_GPIO_PINS_H */
