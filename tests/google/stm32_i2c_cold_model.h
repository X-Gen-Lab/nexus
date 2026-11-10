/**
 * \file            stm32_i2c_cold_model.h
 * \brief           External register write refusal for cold lifecycle tests
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_I2C_COLD_MODEL_H
#define NEXUS_STM32_I2C_COLD_MODEL_H
#include "stm32f407xx.h"
#include <stdint.h>
#ifdef NX_I2C_CLOCK_21
#include "nexus_config.h"
#undef NEXUS_CORE_HZ
#define NEXUS_CORE_HZ 84000000U
#undef NEXUS_CLOCK_SOURCE_HSE
#define NEXUS_CLOCK_SOURCE_HSE 1U
#undef NEXUS_CLOCK_INPUT_HZ
#define NEXUS_CLOCK_INPUT_HZ    8000000U
#define NEXUS_APB1_HZ           21000000U
#define NEXUS_APB2_HZ           42000000U
#define NEXUS_APB1_TIMER_HZ     42000000U
#define NEXUS_APB2_TIMER_HZ     84000000U
#define NEXUS_ADC_HZ            10500000U
#define NEXUS_PLL48_HZ          48000000U
#define NEXUS_CLOCK_PLLM        8U
#define NEXUS_CLOCK_PLLN        336U
#define NEXUS_CLOCK_PLLP        4U
#define NEXUS_CLOCK_PLLQ        7U
#define NEXUS_CLOCK_PLLCFGR     0x07415408U
#define NEXUS_CLOCK_CFGR        0x00009400U
#define NEXUS_FLASH_WAIT_STATES 2U
#define NEXUS_CLOCK_CSS         0U
#define NEXUS_CLOCK_AHB_DIV     1U
#define NEXUS_CLOCK_ADC_DIV     4U
#endif

#ifdef __cplusplus
extern "C" {
#endif
/** \brief Ordinary host storage keeps NVIC outside sanitizer shadow memory. */
extern NVIC_Type g_nx_i2c_cold_nvic;
uint32_t nx_i2c_cold_irq_enabled(IRQn_Type irq);
uint32_t nx_i2c_cold_irq_pending(IRQn_Type irq);
uint32_t nx_i2c_cold_irq_active(IRQn_Type irq);
void nx_i2c_cold_write(volatile uint32_t* reg, uint32_t value);
void nx_i2c_cold_priority_write(volatile uint8_t* reg, uint8_t value);
void nx_i2c_cold_irq_enable(IRQn_Type irq);
void nx_i2c_cold_irq_disable(IRQn_Type irq);
void nx_i2c_cold_irq_clear(IRQn_Type irq);
void nx_i2c_cold_irq_priority(IRQn_Type irq, uint32_t priority);
#ifdef __cplusplus
}
#endif
#undef NVIC
#define NVIC (&g_nx_i2c_cold_nvic)
#undef NVIC_GetEnableIRQ
#undef NVIC_GetPendingIRQ
#undef NVIC_GetActive
#define NVIC_GetEnableIRQ(irq)  nx_i2c_cold_irq_enabled(irq)
#define NVIC_GetPendingIRQ(irq) nx_i2c_cold_irq_pending(irq)
#define NVIC_GetActive(irq)     nx_i2c_cold_irq_active(irq)
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_i2c_cold_write(&(reg), (uint32_t)(value))
#define NX_PLATFORM_PRIORITY_WRITE(reg, value)                                 \
    nx_i2c_cold_priority_write(&(reg), (uint8_t)(value))
#undef NVIC_EnableIRQ
#undef NVIC_DisableIRQ
#undef NVIC_ClearPendingIRQ
#undef NVIC_SetPriority
#define NVIC_EnableIRQ(irq)       nx_i2c_cold_irq_enable(irq)
#define NVIC_DisableIRQ(irq)      nx_i2c_cold_irq_disable(irq)
#define NVIC_ClearPendingIRQ(irq) nx_i2c_cold_irq_clear(irq)
#define NVIC_SetPriority(irq, priority)                                        \
    nx_i2c_cold_irq_priority((irq), (priority))
#endif
