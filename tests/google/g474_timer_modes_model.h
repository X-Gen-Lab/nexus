/**
 * \file            g474_timer_modes_model.h
 * \brief           External G474 capture read-clear and quadrature MMIO ports
 * \author          Nexus Team
 */
#ifndef NX_G474_TIMER_MODES_MODEL_H
#define NX_G474_TIMER_MODES_MODEL_H
#include "stm32g474xx.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
extern TIM_TypeDef g_g474_mode_timers[2];
extern TIM_TypeDef* g_g474_mode_tim3;
extern TIM_TypeDef* g_g474_mode_tim5;
extern GPIO_TypeDef g_g474_mode_gpio;
extern RCC_TypeDef g_g474_mode_rcc;
extern bool g_g474_mode_irq_quiet;
uint32_t nx_g474_mode_read(volatile uint32_t* address);
void nx_g474_mode_write(volatile uint32_t* address, uint32_t value);
#ifdef __cplusplus
}
#endif
#undef TIM3
#undef TIM5
#undef GPIOA
#undef RCC
#define TIM3                     (g_g474_mode_tim3)
#define TIM5                     (g_g474_mode_tim5)
#define GPIOA                    (&g_g474_mode_gpio)
#define RCC                      (&g_g474_mode_rcc)
#define NX_TIMER_READ(reg)       nx_g474_mode_read(&(reg))
#define NX_TIMER_WRITE(reg, val) nx_g474_mode_write(&(reg), (uint32_t)(val))
#define NX_G474_READ(address)    nx_g474_mode_read(address)
#define NX_G474_WRITE(address, value)                                          \
    nx_g474_mode_write((address), (uint32_t)(value))
#define NX_G474_IRQ_AVAILABLE(irq) (g_g474_mode_irq_quiet)
#endif
