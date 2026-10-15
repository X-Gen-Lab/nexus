/**
 * \file            u575_factory_model.h
 * \brief           MMIO addresses for the real generated U575 cold assembly
 * \author          Nexus Team
 */
#ifndef NX_U575_FACTORY_MODEL_H
#define NX_U575_FACTORY_MODEL_H
#include "u575_model.h"
#ifdef __cplusplus
extern "C" {
#endif
extern USART_TypeDef g_u575_factory_uart;
#ifdef __cplusplus
}
#endif
#undef RCC
#undef FLASH
#undef TIM2
#undef GPIOA
#undef GPIOC
#undef USART1
#undef ICACHE
#undef DCACHE1
#define RCC     (&g_u575_model.rcc)
#define FLASH   (&g_u575_model.flash)
#define TIM2    (&g_u575_model.timer)
#define GPIOA   (&g_u575_model.gpioa)
#define GPIOC   (&g_u575_model.gpio)
#define USART1  (&g_u575_factory_uart)
#define ICACHE  (&g_u575_model.icache)
#define DCACHE1 (&g_u575_model.dcache)
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_u575_model_write(&(reg), (uint32_t)(value))
#endif
