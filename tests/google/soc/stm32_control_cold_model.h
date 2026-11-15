/**
 * \file            stm32_control_cold_model.h
 * \brief           External CPU primitives for actual control factory models
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_CONTROL_COLD_MODEL_H
#define NEXUS_STM32_CONTROL_COLD_MODEL_H
#include "stm32_official_model.h"
#include "stm32_platform_cold_model.h"

#ifdef __cplusplus
extern "C" {
#endif
void nx_control_model_clear(volatile uint32_t* reg, uint32_t mask);
void nx_control_model_priority(IRQn_Type irq, uint32_t priority);
void nx_control_model_enable(IRQn_Type irq);
#ifdef __cplusplus
}
#endif
#define NX_STM32_EXTI_WRITE(reg, value)                                        \
    nx_platform_cold_model_write((reg), (uint32_t)(value))
#define NX_STM32_EXTI_READ(reg) (*(reg))
#define NX_STM32_CLEAR_FLAGS(reg, mask)                                        \
    nx_control_model_clear(&(reg), (uint32_t)(mask))
#undef NVIC_SetPriority
#define NVIC_SetPriority(irq, priority)                                        \
    nx_control_model_priority((irq), (priority))
#undef NVIC_EnableIRQ
#define NVIC_EnableIRQ(irq) nx_control_model_enable(irq)

/** \brief ARM CLZ semantics, including the defined zero result. */
static inline uint32_t nx_control_model_clz(uint32_t value) {
    return value == 0U ? 32U : (uint32_t)__builtin_clz(value);
}
/** \brief Reverse exactly the CPU register's thirty-two bits. */
static inline uint32_t nx_control_model_rbit(uint32_t value) {
    uint32_t result = 0U;
    for (unsigned bit = 0U; bit < 32U; ++bit) {
        result = (result << 1U) | (value & 1U);
        value >>= 1U;
    }
    return result;
}
#define __CLZ(value)  nx_control_model_clz((uint32_t)(value))
#define __RBIT(value) nx_control_model_rbit((uint32_t)(value))
#endif
