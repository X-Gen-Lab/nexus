/**
 * \file            stm32_basic_cold_flash_model.h
 * \brief           External GPIO stores and Flash W1C, lock and NVIC facts
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_BASIC_COLD_FLASH_MODEL_H
#define NEXUS_STM32_BASIC_COLD_FLASH_MODEL_H

#include "stm32f4xx.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
void nx_basic_cold_flash_write(volatile uint32_t* reg, uint32_t value);
void nx_basic_cold_gpio_write(volatile uint32_t* reg, uint32_t value);
bool nx_basic_cold_flash_irq_idle(void);
bool nx_basic_cold_flash_irq_enabled(void);
bool nx_basic_cold_flash_irq_active(void);
bool nx_basic_cold_flash_irq_clear(void);
#ifdef __cplusplus
}
#endif

/* Real LL register operations keep their bodies. Only the external write
 * leaf supplies BSRR side effects and selected field write refusal. */
#undef WRITE_REG
#define WRITE_REG(reg, value)                                                  \
    nx_basic_cold_gpio_write(&(reg), (uint32_t)(value))
#define NX_PLATFORM_REGISTER_WRITE(reg, value) WRITE_REG(reg, value)

#define NX_STM32_FLASH_WRITE(reg, value)                                       \
    nx_basic_cold_flash_write((reg), (value))
#define NX_STM32_CLEAR_FLAGS(reg, mask)                                        \
    nx_basic_cold_flash_write(&(reg), (mask))
#define NX_STM32_FLASH_IRQ_IDLE()    nx_basic_cold_flash_irq_idle()
#define NX_STM32_FLASH_IRQ_ENABLED() nx_basic_cold_flash_irq_enabled()
#define NX_STM32_FLASH_IRQ_ACTIVE()  nx_basic_cold_flash_irq_active()
#define NX_STM32_FLASH_IRQ_CLEAR()   nx_basic_cold_flash_irq_clear()

#endif
