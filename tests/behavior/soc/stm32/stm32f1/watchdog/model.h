/**
 * \file            model.h
 * \brief           F103 IWDG register bus and LSI model
 * \author          Nexus Team
 */
#ifndef NEXUS_F103_WATCHDOG_MODEL_H
#define NEXUS_F103_WATCHDOG_MODEL_H

#include "stm32f1xx.h"

#ifdef __cplusplus
extern "C" {
#endif
extern RCC_TypeDef f103_watchdog_rcc;
extern FLASH_TypeDef f103_watchdog_flash;
extern IWDG_TypeDef f103_watchdog_iwdg;
void f103_watchdog_write(volatile uint32_t* reg, uint32_t value);
#ifdef __cplusplus
}
#endif

#undef RCC
#undef FLASH
#undef IWDG
#undef WRITE_REG
#define RCC                   (&f103_watchdog_rcc)
#define FLASH                 (&f103_watchdog_flash)
#define IWDG                  (&f103_watchdog_iwdg)
#define WRITE_REG(reg, value) f103_watchdog_write(&(reg), (uint32_t)(value))
#define NX_STM32F1_WRITE(reg, value)                                           \
    f103_watchdog_write((reg), (uint32_t)(value))
#endif
