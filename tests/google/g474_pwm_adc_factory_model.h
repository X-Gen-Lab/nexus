/**
 * \file            g474_pwm_adc_factory_model.h
 * \brief           Shared external clock and coupled acquisition MMIO ports
 * \author          Nexus Team
 */
#ifndef NX_G474_PWM_ADC_FACTORY_MODEL_H
#define NX_G474_PWM_ADC_FACTORY_MODEL_H
#include "g474_pwm_adc_model.h"
#undef NX_G474_READ
#undef NX_G474_WRITE
#undef NX_G474_SYSTEM_POLL
#undef NX_G474_BOOST_DELAY
#undef NX_G474_IRQ_AVAILABLE
#undef NX_G474_IRQ_CONTROL
#undef NX_G474_IRQ_PRIORITY
#include "g474_model.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_pa_factory_read(volatile uint32_t* address);
void nx_pa_factory_write(volatile uint32_t* address, uint32_t value);
void nx_pa_factory_poll(void* context);
void nx_pa_factory_adc_poll(void* context);
void nx_pa_factory_advance(uint64_t microseconds);
void nx_pa_factory_priority_write(int irq, uint32_t priority);
extern bool g_pa_factory_refuse_priority;
extern bool g_pa_factory_refuse_comparator;
#ifdef __cplusplus
}
#endif
#undef RCC
#undef FLASH
#undef PWR
#undef TIM2
#undef GPIOA
#define RCC   (&g_g474_model.rcc)
#define FLASH (&g_g474_model.flash)
#define PWR   (&g_g474_model.power)
#define TIM2  (&g_g474_model.timer)
#define GPIOA (&g_g474_model.gpio)
#undef NX_G474_READ
#undef NX_G474_WRITE
#undef NX_G474_SYSTEM_POLL
#undef NX_G474_ADC_POLL
#undef NX_G474_IRQ_PRIORITY_WRITE
#undef NX_PLATFORM_REGISTER_WRITE
#define NX_G474_READ(address) nx_pa_factory_read(address)
#define NX_G474_WRITE(address, value)                                          \
    nx_pa_factory_write((address), (uint32_t)(value))
#define NX_G474_SYSTEM_POLL(context) nx_pa_factory_poll(context)
#define NX_G474_ADC_POLL(context)    nx_pa_factory_adc_poll(context)
#define NX_G474_IRQ_PRIORITY_WRITE(irq, priority)                              \
    nx_pa_factory_priority_write((irq), (priority))
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_pa_factory_write(&(reg), (uint32_t)(value))
#endif
