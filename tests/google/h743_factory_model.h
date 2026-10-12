/**
 * \file            h743_factory_model.h
 * \brief           External MMIO addresses for actual H743 generated assembly
 * \author          Nexus Team
 */
#ifndef NX_H743_FACTORY_MODEL_H
#define NX_H743_FACTORY_MODEL_H
#include "h743_model.h"
#ifdef __cplusplus
extern "C" {
#endif
extern USART_TypeDef g_h743_factory_uart;
#ifdef __cplusplus
}
#endif
#undef RCC
#undef FLASH
#undef PWR
#undef TIM2
#undef GPIOB
#undef GPIOD
#undef USART3
#define RCC    (&g_h743_model.rcc)
#define FLASH  (&g_h743_model.flash)
#define PWR    (&g_h743_model.power)
#define TIM2   (&g_h743_model.timer)
#define GPIOB  (&g_h743_model.gpio_b)
#define GPIOD  (&g_h743_model.gpio_d)
#define USART3 (&g_h743_factory_uart)
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_h743_model_write(&(reg), (uint32_t)(value))
#endif
