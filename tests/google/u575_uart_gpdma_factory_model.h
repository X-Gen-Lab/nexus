/**
 * \file            u575_uart_gpdma_factory_model.h
 * \brief           Exact generated U575 addresses and external hardware leaves
 * \author          Nexus Team
 */
#ifndef NX_U575_UART_GPDMA_FACTORY_MODEL_H
#define NX_U575_UART_GPDMA_FACTORY_MODEL_H
#include "u575_uart_gpdma_model.h"
#undef RCC
#undef FLASH
#undef TIM2
#undef GPIOA
#undef ICACHE
#undef DCACHE1
#define RCC     (&g_u575_model.rcc)
#define FLASH   (&g_u575_model.flash)
#define TIM2    (&g_u575_model.timer)
#define GPIOA   (&g_u575_model.gpioa)
#define ICACHE  (&g_u575_model.icache)
#define DCACHE1 (&g_u575_model.dcache)
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_u575_uart_gpdma_model_write(&(reg), (uint32_t)(value))
#endif
