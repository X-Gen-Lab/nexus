/**
 * \file            g474_control_factory_model.h
 * \brief           External TIM1, COMP1 and clock ports for generated control
 * \author          Nexus Team
 */
#ifndef NX_G474_CONTROL_FACTORY_MODEL_H
#define NX_G474_CONTROL_FACTORY_MODEL_H
#include "g474_model.h"
#ifdef __cplusplus
extern "C" {
#endif
extern TIM_TypeDef g_g474_control_tim1;
extern TIM_TypeDef g_g474_control_tim15;
extern COMP_TypeDef g_g474_control_comp1;
uint32_t nx_g474_control_read(volatile uint32_t* address);
void nx_g474_control_write(volatile uint32_t* address, uint32_t value);
void nx_g474_control_poll(void* system);
void nx_g474_control_priority_write(int irq, uint32_t priority);
#ifdef __cplusplus
}
#endif
#undef RCC
#undef FLASH
#undef PWR
#undef TIM2
#undef TIM1
#undef TIM15
#undef COMP1
#undef GPIOA
#define RCC   (&g_g474_model.rcc)
#define FLASH (&g_g474_model.flash)
#define PWR   (&g_g474_model.power)
#define TIM2  (&g_g474_model.timer)
#define TIM1  (&g_g474_control_tim1)
#define TIM15 (&g_g474_control_tim15)
#define COMP1 (&g_g474_control_comp1)
#define GPIOA (&g_g474_model.gpio)
#undef NX_G474_READ
#undef NX_G474_WRITE
#undef NX_G474_SYSTEM_POLL
#define NX_G474_READ(address) nx_g474_control_read(address)
#define NX_G474_WRITE(address, value)                                          \
    nx_g474_control_write((address), (uint32_t)(value))
#define NX_G474_SYSTEM_POLL(system) nx_g474_control_poll(system)
#define NX_G474_IRQ_PRIORITY_WRITE(irq, priority)                              \
    nx_g474_control_priority_write((irq), (priority))
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_g474_control_write(&(reg), (uint32_t)(value))
#endif
