/**
 * \file            u575_mainstream_model.h
 * \brief           Hardware leaves for the real eight-bank U575 assembly
 * \author          Nexus Team
 */
#ifndef NX_U575_MAINSTREAM_MODEL_H
#define NX_U575_MAINSTREAM_MODEL_H
#include "u575_factory_model.h"
#ifdef __cplusplus
extern "C" {
#endif
extern USART_TypeDef g_u575_mainstream_uart2;
extern IWDG_TypeDef g_u575_mainstream_iwdg;
extern DBGMCU_TypeDef g_u575_mainstream_debug;
#ifdef __cplusplus
}
#endif
#undef GPIOB
#undef GPIOD
#undef GPIOE
#undef GPIOF
#undef GPIOG
#undef GPIOH
#undef USART2
#undef IWDG
#undef DBGMCU
#define GPIOB  (&g_u575_model.extra_gpio[0])
#define GPIOD  (&g_u575_model.extra_gpio[1])
#define GPIOE  (&g_u575_model.extra_gpio[2])
#define GPIOF  (&g_u575_model.extra_gpio[3])
#define GPIOG  (&g_u575_model.extra_gpio[4])
#define GPIOH  (&g_u575_model.extra_gpio[5])
#define USART2 (&g_u575_mainstream_uart2)
#define IWDG   (&g_u575_mainstream_iwdg)
#define DBGMCU (&g_u575_mainstream_debug)
#endif
