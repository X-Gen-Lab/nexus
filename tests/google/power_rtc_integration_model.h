/**
 * \file            power_rtc_integration_model.h
 * \brief           Shared external registers for real System and RTC power C
 * \author          Nexus Team
 */
#ifndef NX_POWER_RTC_INTEGRATION_MODEL_H
#define NX_POWER_RTC_INTEGRATION_MODEL_H
#include "f407_rtc_model.h"
#include "stm32f407_clock.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_power_rtc_write(volatile uint32_t* address, uint32_t value);
void nx_power_rtc_poll(void* system);
void nx_power_rtc_wait(bool event);
int nx_power_rtc_timer_irq(bool enabled);
extern uint32_t nx_power_rtc_scr;
extern uint32_t nx_power_rtc_icsr;
extern uint32_t nx_power_rtc_tick;
extern uint32_t nx_power_rtc_enabled[3];
extern uint32_t nx_power_rtc_active[3];
#ifdef __cplusplus
}
#endif
#define NX_STM32_WRITE(reg, value)  nx_power_rtc_write((reg), (uint32_t)(value))
#define NX_STM32_POLL(system)       nx_power_rtc_poll(system)
#define NX_STM32_IRQ_AVAILABLE(irq) ((void)(irq), true)
#define NX_STM32_IRQ_CONTROL(irq, enabled)                                     \
    ((void)(irq), nx_power_rtc_timer_irq(enabled))
#define NX_STM32_IRQ_ISOLATE(irq) ((void)(irq), nx_power_rtc_timer_irq(false))
#define NX_POWER_SCR              nx_power_rtc_scr
#define NX_POWER_ICSR             nx_power_rtc_icsr
#define NX_POWER_TICK             nx_power_rtc_tick
#define NX_POWER_IRQ_ENABLED(n)   nx_power_rtc_enabled[n]
#define NX_POWER_IRQ_ACTIVE(n)    nx_power_rtc_active[n]
#define NX_POWER_WAIT()           nx_power_rtc_wait(false)
#define NX_POWER_WAIT_EVENT()     nx_power_rtc_wait(true)
#define NX_POWER_EMR              g_f407_rtc_model.exti.EMR
#define NX_POWER_RTSR             g_f407_rtc_model.exti.RTSR
#define NX_POWER_IMR              g_f407_rtc_model.exti.IMR
#define NX_POWER_FTSR             g_f407_rtc_model.exti.FTSR
#define NX_POWER_DMA1             DMA1_BASE
#define NX_POWER_DMA2             DMA2_BASE
#endif /* NX_POWER_RTC_INTEGRATION_MODEL_H */
