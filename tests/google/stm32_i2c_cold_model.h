/**
 * \file            stm32_i2c_cold_model.h
 * \brief           External register write refusal for cold lifecycle tests
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_I2C_COLD_MODEL_H
#define NEXUS_STM32_I2C_COLD_MODEL_H
#include "stm32f407xx.h"
#include <stdint.h>
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
