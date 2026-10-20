/**
 * \file            h743_uart_rtscts_factory_model.h
 * \brief           Generated FIFO assembly with external exact hardware leaves
 * \author          Nexus Team
 */
#ifndef NX_H743_UART_RTSCTS_FACTORY_MODEL_H
#define NX_H743_UART_RTSCTS_FACTORY_MODEL_H
#include "h743_uart_rtscts_model.h"
#undef RCC
#undef FLASH
#undef PWR
#undef TIM2
#undef GPIOD
#define RCC   (&g_h743_model.rcc)
#define FLASH (&g_h743_model.flash)
#define PWR   (&g_h743_model.power)
#define TIM2  (&g_h743_model.timer)
#define GPIOD (&g_h743_model.gpio_d)
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_h743_uart_rtscts_model_write(&(reg), (uint32_t)(value))
#endif
