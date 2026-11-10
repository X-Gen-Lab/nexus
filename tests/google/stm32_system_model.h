/**
 * \file            stm32_system_model.h
 * \brief           External clock register and NVIC boundaries for boot tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32_SYSTEM_MODEL_H
#define NX_STM32_SYSTEM_MODEL_H
#include "stm32f407xx.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void nx_stm32_system_model_poll(void* system);
void nx_stm32_system_model_write(volatile uint32_t* reg, uint32_t value);
bool nx_stm32_system_model_irq_available(int irq);
int nx_stm32_system_model_irq_control(int irq, bool enabled);
extern uint32_t nx_power_scr;
extern uint32_t nx_power_icsr;
extern uint32_t nx_power_tick;
extern uint32_t nx_power_emr;
extern uint32_t nx_power_rtsr;
extern uint32_t nx_power_imr;
extern uint32_t nx_power_ftsr;
extern uint32_t nx_power_dma1[64];
extern uint32_t nx_power_dma2[64];
extern uint32_t nx_power_enabled[3];
extern uint32_t nx_power_active[3];
void nx_stm32_power_model_wait(void);
void nx_stm32_power_model_event_wait(void);
extern uint32_t nx_boot_cpacr;
extern uint32_t nx_boot_mpu;
extern uint32_t nx_boot_vtor;
extern uint32_t nx_system_rng_control;
extern SCB_Type nx_supervision_scb;
extern DWT_Type nx_supervision_dwt;
extern DCB_Type nx_supervision_debug;
#ifdef __cplusplus
}
#endif
#define NX_STM32_POLL(system) nx_stm32_system_model_poll(system)
#define NX_STM32_WRITE(reg, value)                                             \
    nx_stm32_system_model_write((reg), (uint32_t)(value))
#define NX_STM32_IRQ_AVAILABLE(irq)                                            \
    nx_stm32_system_model_irq_available((int)(irq))
#define NX_STM32_IRQ_CONTROL(irq, enabled)                                     \
    nx_stm32_system_model_irq_control((int)(irq), (enabled))
#define NX_STM32_IRQ_ISOLATE(irq)                                              \
    nx_stm32_system_model_irq_control((int)(irq), false)
#define NX_STM32_TIMEBASE_ACK(system, value)                                   \
    nx_stm32_system_model_write(&(system)->timer->SR, (uint32_t)(value))
#define NX_POWER_SCR                nx_power_scr
#define NX_POWER_ICSR               nx_power_icsr
#define NX_POWER_TICK               nx_power_tick
#define NX_POWER_DMA1               ((uintptr_t)nx_power_dma1)
#define NX_POWER_DMA2               ((uintptr_t)nx_power_dma2)
#define NX_POWER_IRQ_ENABLED(index) nx_power_enabled[index]
#define NX_POWER_IRQ_ACTIVE(index)  nx_power_active[index]
#define NX_POWER_WAIT()             nx_stm32_power_model_wait()
#define NX_POWER_WAIT_EVENT()       nx_stm32_power_model_event_wait()
#define NX_POWER_EMR                nx_power_emr
#define NX_POWER_RTSR               nx_power_rtsr
#define NX_POWER_IMR                nx_power_imr
#define NX_POWER_FTSR               nx_power_ftsr
#define NX_STM32_BOOT_CPACR         nx_boot_cpacr
#define NX_STM32_BOOT_MPU           nx_boot_mpu
#define NX_STM32_BOOT_VTOR          nx_boot_vtor
#define NX_STM32_RNG_CONTROL        nx_system_rng_control
#define NX_STM32_BOOT_VECTOR        0x08000000U
#define NX_SUPERVISION_SCB          (&nx_supervision_scb)
#define NX_SUPERVISION_DWT          (&nx_supervision_dwt)
#define NX_SUPERVISION_DEBUG        (&nx_supervision_debug)
#define NX_SUPERVISION_RCC          (g_nx_stm32_system.rcc)
#if defined(NEXUS_SYSTEM_CLOCK_TEST_84)
#include "nexus_config.h"
#undef NEXUS_CORE_HZ
#define NEXUS_CORE_HZ       84000000U
#define NEXUS_APB1_HZ       21000000U
#define NEXUS_APB2_HZ       42000000U
#define NEXUS_APB1_TIMER_HZ 42000000U
#define NEXUS_APB2_TIMER_HZ 84000000U
#define NEXUS_ADC_HZ        10500000U
#define NEXUS_PLL48_HZ      48000000U
#define NEXUS_CLOCK_PLLM    8U
#define NEXUS_CLOCK_PLLN    336U
#define NEXUS_CLOCK_PLLP    4U
#define NEXUS_CLOCK_PLLQ    7U
#undef NEXUS_CLOCK_INPUT_HZ
#define NEXUS_CLOCK_INPUT_HZ    8000000U
#define NEXUS_CLOCK_PLLCFGR     0x07415408U
#define NEXUS_CLOCK_CFGR        0x00009400U
#define NEXUS_FLASH_WAIT_STATES 2U
#define NEXUS_CLOCK_CSS         0U
#define NEXUS_CLOCK_AHB_DIV     1U
#define NEXUS_CLOCK_ADC_DIV     4U
#if defined(NEXUS_SYSTEM_CLOCK_TEST_HSI84)
#undef NEXUS_CLOCK_INPUT_HZ
#define NEXUS_CLOCK_INPUT_HZ 16000000U
#undef NEXUS_CLOCK_SOURCE_HSE
#define NEXUS_CLOCK_SOURCE_HSE 0U
#undef NEXUS_CLOCK_PLLM
#define NEXUS_CLOCK_PLLM 16U
#undef NEXUS_CLOCK_PLLCFGR
#define NEXUS_CLOCK_PLLCFGR 0x07015410U
#endif
#endif
#endif /* NX_STM32_SYSTEM_MODEL_H */
