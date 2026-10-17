/**
 * \file            g474_adc_group_factory_model.h
 * \brief           Actual generated assembly external clock and ADC ports
 * \author          Nexus Team
 */
#ifndef NX_G474_ADC_GROUP_FACTORY_MODEL_H
#define NX_G474_ADC_GROUP_FACTORY_MODEL_H
#include "g474_adc_group_model.h"
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
uint32_t nx_ag_factory_read(volatile uint32_t* reg);
void nx_ag_factory_write(volatile uint32_t* reg, uint32_t value);
void nx_ag_factory_poll(void* context);
void nx_ag_factory_adc_poll(void* context);
void nx_ag_factory_advance(uint64_t microseconds);
void nx_ag_factory_priority_write(int irq, uint32_t priority);
extern bool g_ag_factory_refuse_priority;
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
#undef FLASHSIZE_BASE
#undef NX_G474_FLASH_KIB
#undef NX_G474_DEVICE_ID
/* Legacy density/group and strict observed cut share these exact registers. */
#define FLASHSIZE_BASE      ((uintptr_t)&g_ag.flash_kib)
#define NX_G474_FLASH_KIB() (g_ag.flash_kib)
#define NX_G474_DEVICE_ID() (g_ag.debug.IDCODE & UINT32_C(0xfff))
/* CPU context is external to the real vector/provider implementation. */
#define nx_arch_in_isr()           (g_ag.in_isr)
#define nx_arch_exception_number() (g_ag.in_isr ? 16U : 0U)
#undef NX_G474_READ
#undef NX_G474_WRITE
#undef NX_G474_SYSTEM_POLL
#undef NX_G474_ADC_POLL
#undef NX_G474_IRQ_PRIORITY_WRITE
#undef NX_PLATFORM_REGISTER_WRITE
#define NX_G474_READ(reg)            nx_ag_factory_read(reg)
#define NX_G474_WRITE(reg, value)    nx_ag_factory_write((reg), (uint32_t)(value))
#define NX_G474_SYSTEM_POLL(context) nx_ag_factory_poll(context)
#define NX_G474_ADC_POLL(context)    nx_ag_factory_adc_poll(context)
#define NX_G474_IRQ_PRIORITY_WRITE(irq, priority)                              \
    nx_ag_factory_priority_write((irq), (priority))
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_ag_factory_write(&(reg), (uint32_t)(value))
#endif
