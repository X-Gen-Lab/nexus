/**
 * \file            g474_mainstream_model.h
 * \brief           External MMIO addresses for actual generated G474 assembly
 * \author          Nexus Team
 */
#ifndef NX_G474_MAINSTREAM_MODEL_H
#define NX_G474_MAINSTREAM_MODEL_H
#include "g474_model.h"
#ifdef __cplusplus
extern "C" {
#endif
extern USART_TypeDef g_g474_mainstream_uarts[5];
#ifdef __cplusplus
}
#endif
#undef RCC
#undef FLASH
#undef PWR
#undef TIM2
#undef GPIOA
#undef USART1
#undef USART2
#undef USART3
#undef UART4
#undef UART5
#undef GPIOB
#undef GPIOC
#undef GPIOD
#define RCC    (&g_g474_model.rcc)
#define FLASH  (&g_g474_model.flash)
#define PWR    (&g_g474_model.power)
#define TIM2   (&g_g474_model.timer)
#define GPIOA  (&g_g474_model.gpio)
#define USART1 (&g_g474_mainstream_uarts[0])
#define USART2 (&g_g474_mainstream_uarts[1])
#define USART3 (&g_g474_mainstream_uarts[2])
#define UART4  (&g_g474_mainstream_uarts[3])
#define UART5  (&g_g474_mainstream_uarts[4])
#define GPIOB  (&g_g474_model.gpio_b)
#define GPIOC  (&g_g474_model.gpio_c)
#define GPIOD  (&g_g474_model.gpio_d)
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_g474_model_write(&(reg), (uint32_t)(value))
#endif
