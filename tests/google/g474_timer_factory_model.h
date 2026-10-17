/**
 * \file            g474_timer_factory_model.h
 * \brief           External counter and clock ports for actual timer factories
 * \author          Nexus Team
 */
#ifndef NX_G474_TIMER_FACTORY_MODEL_H
#define NX_G474_TIMER_FACTORY_MODEL_H
#include "g474_model.h"
#ifdef __cplusplus
extern "C" {
#endif
extern TIM_TypeDef g_g474_factory_timers[2];
extern volatile uint32_t* g_g474_factory_refused;
extern uint32_t g_g474_factory_held;
uint32_t nx_g474_timer_factory_read(volatile uint32_t* address);
void nx_g474_timer_factory_write(volatile uint32_t* address, uint32_t value);
void nx_g474_timer_factory_priority_write(int irq, uint32_t priority);
#ifdef __cplusplus
}
#endif
#undef RCC
#undef FLASH
#undef PWR
#undef TIM2
#undef TIM3
#undef TIM5
#undef GPIOA
#define RCC   (&g_g474_model.rcc)
#define FLASH (&g_g474_model.flash)
#define PWR   (&g_g474_model.power)
#define TIM2  (&g_g474_model.timer)
#define TIM3  (&g_g474_factory_timers[0])
#define TIM5  (&g_g474_factory_timers[1])
#define GPIOA (&g_g474_model.gpio)
#undef NX_G474_READ
#undef NX_G474_WRITE
#define NX_G474_READ(address) nx_g474_timer_factory_read(address)
#define NX_G474_WRITE(address, value)                                          \
    nx_g474_timer_factory_write((address), (uint32_t)(value))
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_g474_timer_factory_write(&(reg), (uint32_t)(value))
#define NX_G474_IRQ_PRIORITY_WRITE(irq, priority)                              \
    nx_g474_timer_factory_priority_write((irq), (priority))
#endif
