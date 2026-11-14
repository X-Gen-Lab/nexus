/**
 * \file            stm32_high_clock_model.h
 * \brief           Reviewed HSI PLL profiles at the real System MMIO boundary
 * \author          Nexus Team
 */
#ifndef NX_STM32_HIGH_CLOCK_MODEL_H
#define NX_STM32_HIGH_CLOCK_MODEL_H

#include "stm32_family_system_model.h"

#include "stm32_high_clock_profile.h"

#ifdef __cplusplus
extern "C" {
#endif
extern uint32_t nx_family_power_scr;
extern uint32_t nx_family_power_debug;
extern uint32_t nx_family_power_icsr;
extern uint32_t nx_family_power_tick;
extern uint32_t nx_family_power_emr;
extern uint32_t nx_family_power_rtsr;
extern uint32_t nx_family_power_imr;
extern uint32_t nx_family_power_ftsr;
extern uint32_t nx_family_power_dma1[64];
extern uint32_t nx_family_power_dma2[64];
extern uint32_t nx_family_power_enabled[4];
extern uint32_t nx_family_power_active[4];
void nx_family_power_wait(bool event);
#ifdef __cplusplus
}
#endif
#define NX_POWER_SCR                nx_family_power_scr
#define NX_POWER_DEBUG              nx_family_power_debug
#define NX_POWER_ICSR               nx_family_power_icsr
#define NX_POWER_TICK               nx_family_power_tick
#define NX_POWER_DMA1               ((uintptr_t)nx_family_power_dma1)
#define NX_POWER_DMA2               ((uintptr_t)nx_family_power_dma2)
#define NX_POWER_IRQ_ENABLED(index) nx_family_power_enabled[index]
#define NX_POWER_IRQ_ACTIVE(index)  nx_family_power_active[index]
#define NX_POWER_WAIT()             nx_family_power_wait(false)
#define NX_POWER_WAIT_EVENT()       nx_family_power_wait(true)
#define NX_POWER_EMR                nx_family_power_emr
#define NX_POWER_RTSR               nx_family_power_rtsr
#define NX_POWER_IMR                nx_family_power_imr
#define NX_POWER_FTSR               nx_family_power_ftsr
#endif /* NX_STM32_HIGH_CLOCK_MODEL_H */
