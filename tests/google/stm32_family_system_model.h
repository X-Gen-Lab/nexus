/**
 * \file            stm32_family_system_model.h
 * \brief           Selected F4 SDK registers at an independent MMIO boundary
 * \author          Nexus Team
 */
#ifndef NX_SYSTEM_FAMILY_MODEL_H
#define NX_SYSTEM_FAMILY_MODEL_H
#include "nexus_config.h"
#include "stm32f4xx.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void nx_family_poll(void* system);
void nx_family_write(volatile uint32_t* reg, uint32_t value);
bool nx_family_irq_available(int irq);
int nx_family_irq_control(int irq, bool enabled);
uint32_t nx_family_idcode(void);
uint16_t nx_family_flash_kib(void);
extern uint32_t nx_family_boot_cpacr;
extern uint32_t nx_family_boot_mpu;
extern uint32_t nx_family_boot_vtor;
extern uint32_t nx_family_rng_control;
#ifdef __cplusplus
}
#endif
#define NX_STM32_POLL(system)       nx_family_poll(system)
#define NX_STM32_WRITE(reg, value)  nx_family_write((reg), (uint32_t)(value))
#define NX_STM32_IRQ_AVAILABLE(irq) nx_family_irq_available((int)(irq))
#define NX_STM32_IRQ_CONTROL(irq, enabled)                                     \
    nx_family_irq_control((int)(irq), (enabled))
#define NX_STM32_IRQ_ISOLATE(irq) nx_family_irq_control((int)(irq), false)
#define NX_STM32_TIMEBASE_ACK(system, value)                                   \
    nx_family_write(&(system)->timer->SR, (uint32_t)(value))
#define NX_STM32_BOOT_CPACR  nx_family_boot_cpacr
#define NX_STM32_BOOT_MPU    nx_family_boot_mpu
#define NX_STM32_BOOT_VTOR   nx_family_boot_vtor
#define NX_STM32_BOOT_VECTOR 0x08000000U
#define NX_STM32_RNG_CONTROL nx_family_rng_control
#define NX_STM32_DEVICE_ID() nx_family_idcode()
#define NX_STM32_FLASH_KIB() nx_family_flash_kib()
#undef NEXUS_CORE_HZ
#undef NEXUS_CLOCK_INPUT_HZ
#undef NEXUS_CLOCK_SOURCE_HSE
#define NEXUS_CORE_HZ           16000000U
#define NEXUS_CLOCK_INPUT_HZ    16000000U
#define NEXUS_CLOCK_SOURCE_HSE  0U
#define NEXUS_CLOCK_PLL_ENABLED 0U
#define NEXUS_CLOCK_SYSCLK_PLL  0U
#define NEXUS_CLOCK_PLLM        16U
#define NEXUS_CLOCK_PLLN        336U
#define NEXUS_CLOCK_PLLP        4U
#define NEXUS_CLOCK_PLLQ        7U
#define NEXUS_CLOCK_AHB_DIV     1U
#define NEXUS_CLOCK_APB1_DIV    4U
#define NEXUS_CLOCK_APB2_DIV    1U
#define NEXUS_CLOCK_ADC_DIV     4U
#define NEXUS_CLOCK_CSS         0U
#define NEXUS_FLASH_WAIT_STATES 0U
#define NEXUS_APB1_HZ           4000000U
#define NEXUS_APB2_HZ           16000000U
#define NEXUS_APB1_TIMER_HZ     8000000U
#define NEXUS_APB2_TIMER_HZ     16000000U
#define NEXUS_ADC_HZ            4000000U
#define NEXUS_PLL48_HZ          0U
#if defined(NX_TEST_SYSTEM_TIM5)
#undef NEXUS_STM32_TIMEBASE_TIMER
#undef NEXUS_STM32_TIMEBASE_IRQ
#undef NEXUS_STM32_TIMEBASE_GATE
#undef NEXUS_STM32_TIMEBASE_RESET
#undef NEXUS_STM32_TIMEBASE_HANDLER
#undef NEXUS_STM32_TIMEBASE_COUNTER_BITS
#define NEXUS_STM32_TIMEBASE_TIMER        TIM5
#define NEXUS_STM32_TIMEBASE_IRQ          TIM5_IRQn
#define NEXUS_STM32_TIMEBASE_GATE         RCC_APB1ENR_TIM5EN
#define NEXUS_STM32_TIMEBASE_RESET        RCC_APB1RSTR_TIM5RST
#define NEXUS_STM32_TIMEBASE_HANDLER      TIM5_IRQHandler
#define NEXUS_STM32_TIMEBASE_COUNTER_BITS 32U
#endif
#endif
