/**
 * \file            stm32_sai_model.h
 * \brief           External F446/F479 SAI/DMA/IRQ and monotonic-time leaves
 * \author          Nexus Team
 */
#ifndef NX_STM32_SAI_MODEL_H
#define NX_STM32_SAI_MODEL_H
#include "nexus_config.h"
#undef NEXUS_CORE_HZ
#define NEXUS_CORE_HZ 16000000U
#undef NEXUS_CLOCK_INPUT_HZ
#define NEXUS_CLOCK_INPUT_HZ 16000000U
#undef NEXUS_CLOCK_SOURCE_HSE
#define NEXUS_CLOCK_SOURCE_HSE  0U
#define NEXUS_CLOCK_PLL_ENABLED 0U
#define NEXUS_CLOCK_SW          0U
#define NEXUS_CLOCK_PLLM        16U
#define NEXUS_CLOCK_PLLN        336U
#define NEXUS_CLOCK_PLLP        4U
#define NEXUS_CLOCK_PLLQ        7U
#define NEXUS_CLOCK_PLLCFGR     0x07015410U
#define NEXUS_CLOCK_CFGR        0x00001400U
#define NEXUS_CLOCK_AHB_DIV     1U
#define NEXUS_CLOCK_ADC_DIV     4U
#define NEXUS_CLOCK_CSS         0U
#define NEXUS_FLASH_WAIT_STATES 0U
#define NEXUS_APB1_HZ           4000000U
#define NEXUS_APB2_HZ           16000000U
#define NEXUS_APB1_TIMER_HZ     8000000U
#define NEXUS_APB2_TIMER_HZ     16000000U
#define NEXUS_ADC_HZ            4000000U
#define NEXUS_PLL48_HZ          0U
#ifdef NX_SAI_MASTER_TEST
#undef NEXUS_CLOCK_PLLM
#undef NEXUS_CLOCK_PLLN
#undef NEXUS_CLOCK_PLLCFGR
#if defined(STM32F479xx)
#define NEXUS_CLOCK_PLLM 10U
#else
#define NEXUS_CLOCK_PLLM 16U
#endif
#define NEXUS_CLOCK_PLLN 192U
#define NEXUS_CLOCK_PLLCFGR                                                    \
    (NEXUS_CLOCK_PLLM | (192U << 6U) | (1U << 16U) | (7U << 24U))
#undef NEXUS_CLOCK_PLLSAI_SAI2_ENABLED
#define NEXUS_CLOCK_PLLSAI_SAI2_ENABLED 1U
#define NEXUS_CLOCK_PLLSAI_ENABLED      1U
#define NEXUS_CLOCK_PLLSAI_SAI1_ENABLED 1U
#define NEXUS_CLOCK_PLLSAI_LTDC_ENABLED 0U
#define NEXUS_CLOCK_PLLSAI_M            10U
#define NEXUS_CLOCK_PLLSAI_N            192U
#define NEXUS_CLOCK_PLLSAI_Q            5U
#define NEXUS_CLOCK_PLLSAI_DIVQ         5U
#define NEXUS_CLOCK_PLLSAI_SAI1_HZ      12288000U
#ifdef NX_SAI_ALT_CLOCK
#undef NEXUS_CLOCK_PLLSAI_N
#undef NEXUS_CLOCK_PLLSAI_Q
#undef NEXUS_CLOCK_PLLSAI_DIVQ
#undef NEXUS_CLOCK_PLLSAI_SAI1_HZ
#define NEXUS_CLOCK_PLLSAI_N       140U
#define NEXUS_CLOCK_PLLSAI_Q       4U
#define NEXUS_CLOCK_PLLSAI_DIVQ    8U
#define NEXUS_CLOCK_PLLSAI_SAI1_HZ 7000000U
#endif
#endif
#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "stm32f407_system.h"
#include "stm32f4xx_hal.h"
uint32_t nx_sai_read(volatile uint32_t* reg);
void nx_sai_write(volatile uint32_t* reg, uint32_t value);
nx_time_us_t nx_sai_now(void);
bool nx_sai_task_privileged(void);
bool nx_sai_isr(void);
bool nx_sai_masked(void);
uint32_t nx_sai_irq_enabled(IRQn_Type irq);
uint32_t nx_sai_irq_active(IRQn_Type irq);
uint32_t nx_sai_irq_pending(IRQn_Type irq);
uint32_t nx_sai_irq_priority_read(IRQn_Type irq);
void nx_sai_irq_priority(IRQn_Type irq, uint32_t priority);
void nx_sai_irq_enable(IRQn_Type irq);
void nx_sai_irq_disable(IRQn_Type irq);
void nx_sai_irq_clear(IRQn_Type irq);
#ifdef __cplusplus
}
#endif
#define NX_AV_READ(reg)                   nx_sai_read(reg)
#define NX_AV_WRITE(reg, value)           nx_sai_write((reg), (value))
#define NX_AV_POLL()                      ((void)0)
#define NX_AV_NOW()                       nx_sai_now()
#define NX_AV_PRIVILEGED()                nx_sai_task_privileged()
#define NX_AV_IN_ISR()                    nx_sai_isr()
#define NX_AV_MASKED()                    nx_sai_masked()
#define NX_AV_IRQ_ENABLED(irq)            nx_sai_irq_enabled(irq)
#define NX_AV_IRQ_ACTIVE(irq)             nx_sai_irq_active(irq)
#define NX_AV_IRQ_PENDING(irq)            nx_sai_irq_pending(irq)
#define NX_AV_IRQ_PRIORITY_READ(irq)      nx_sai_irq_priority_read(irq)
#define NX_AV_IRQ_PRIORITY(irq, priority) nx_sai_irq_priority((irq), (priority))
#define NX_AV_IRQ_ENABLE(irq)             nx_sai_irq_enable(irq)
#define NX_AV_IRQ_DISABLE(irq)            nx_sai_irq_disable(irq)
#define NX_AV_IRQ_CLEAR(irq)              nx_sai_irq_clear(irq)
#endif
