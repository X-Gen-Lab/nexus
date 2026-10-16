/**
 * \file            g474_peripheral_factory_model.h
 * \brief           External MMIO addresses for actual generated G474 assembly
 * \author          Nexus Team
 */
#ifndef NX_G474_PERIPHERAL_FACTORY_MODEL_H
#define NX_G474_PERIPHERAL_FACTORY_MODEL_H
#include "g474_model.h"
#ifdef __cplusplus
extern "C" {
#endif
extern SPI_TypeDef g_g474_factory_spi[2];
extern IWDG_TypeDef g_g474_factory_watchdog;
extern DBGMCU_TypeDef g_g474_factory_debug;
#ifdef __cplusplus
}
#endif
#undef RCC
#undef FLASH
#undef PWR
#undef TIM2
#undef GPIOA
#undef SPI1
#undef SPI2
#undef GPIOB
#undef IWDG
#undef DBGMCU
#define RCC    (&g_g474_model.rcc)
#define FLASH  (&g_g474_model.flash)
#define PWR    (&g_g474_model.power)
#define TIM2   (&g_g474_model.timer)
#define GPIOA  (&g_g474_model.gpio)
#define SPI1   (&g_g474_factory_spi[0])
#define SPI2   (&g_g474_factory_spi[1])
#define GPIOB  (&g_g474_model.gpio_b)
#define IWDG   (&g_g474_factory_watchdog)
#define DBGMCU (&g_g474_factory_debug)
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_g474_model_write(&(reg), (uint32_t)(value))
#endif
