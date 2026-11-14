/**
 * \file            stm32_sai_system_model.h
 * \brief           System-owned PLLSAI at an independent RCC hardware boundary
 * \author          Nexus Team
 */
#ifndef NX_SAI_SYSTEM_MODEL_H
#define NX_SAI_SYSTEM_MODEL_H
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
#define NEXUS_CLOCK_SW          0U
#if defined(STM32F479xx)
#define NEXUS_CLOCK_PLLM 10U
#else
#define NEXUS_CLOCK_PLLM 16U
#endif
#define NEXUS_CLOCK_PLLN 192U
#define NEXUS_CLOCK_PLLP 4U
#define NEXUS_CLOCK_PLLQ 7U
#define NEXUS_CLOCK_PLLCFGR                                                    \
    (NEXUS_CLOCK_PLLM | (NEXUS_CLOCK_PLLN << 6U) | (1U << 16U) | (7U << 24U))
#define NEXUS_CLOCK_CFGR           0x00001400U
#define NEXUS_CLOCK_AHB_DIV        1U
#define NEXUS_CLOCK_ADC_DIV        4U
#define NEXUS_CLOCK_CSS            0U
#define NEXUS_FLASH_WAIT_STATES    0U
#define NEXUS_APB1_HZ              4000000U
#define NEXUS_APB2_HZ              16000000U
#define NEXUS_APB1_TIMER_HZ        8000000U
#define NEXUS_APB2_TIMER_HZ        16000000U
#define NEXUS_ADC_HZ               4000000U
#define NEXUS_PLL48_HZ             0U
#define NEXUS_CLOCK_PLLSAI_ENABLED 1U
#define NEXUS_CLOCK_PLLSAI_M       10U
#define NEXUS_CLOCK_PLLSAI_N       192U
#ifndef NX_TEST_AUX_KIND
#define NX_TEST_AUX_KIND 1U
#endif
#if NX_TEST_AUX_KIND != 2U
#define NEXUS_CLOCK_PLLSAI_SAI1_ENABLED 1U
#if defined(STM32F446xx)
#define NEXUS_CLOCK_PLLSAI_SAI2_ENABLED 1U
#endif
#define NEXUS_CLOCK_PLLSAI_Q       5U
#define NEXUS_CLOCK_PLLSAI_DIVQ    5U
#define NEXUS_CLOCK_PLLSAI_SAI1_HZ 12288000U
#else
#define NEXUS_CLOCK_PLLSAI_SAI1_ENABLED 0U
#endif
#if NX_TEST_AUX_KIND != 1U
#define NEXUS_CLOCK_PLLSAI_LTDC_ENABLED 1U
#define NEXUS_CLOCK_PLLSAI_R            4U
#define NEXUS_CLOCK_PLLSAI_DIVR         16U
#define NEXUS_CLOCK_PLLSAI_LTDC_HZ      4800000U
#else
#define NEXUS_CLOCK_PLLSAI_LTDC_ENABLED 0U
#endif
#endif
