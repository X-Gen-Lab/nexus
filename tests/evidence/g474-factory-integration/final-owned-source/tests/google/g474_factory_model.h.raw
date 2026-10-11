/**
 * \file            g474_factory_model.h
 * \brief           External MMIO addresses for actual generated G474 assembly
 * \author          Nexus Team
 */
#ifndef NX_G474_FACTORY_MODEL_H
#define NX_G474_FACTORY_MODEL_H
#include "g474_model.h"
#ifdef __cplusplus
extern "C" {
#endif
extern USART_TypeDef g_g474_factory_uart;
#ifdef __cplusplus
}
#endif
#undef RCC
#undef FLASH
#undef PWR
#undef TIM2
#undef GPIOA
#undef LPUART1
#define RCC     (&g_g474_model.rcc)
#define FLASH   (&g_g474_model.flash)
#define PWR     (&g_g474_model.power)
#define TIM2    (&g_g474_model.timer)
#define GPIOA   (&g_g474_model.gpio)
#define LPUART1 (&g_g474_factory_uart)
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_g474_model_write(&(reg), (uint32_t)(value))
#endif
