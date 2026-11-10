/**
 * \file            stm32_pvd_model.h
 * \brief           External PWR/EXTI acknowledgements and ASan-safe NVIC
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_PVD_MODEL_H
#define NEXUS_STM32_PVD_MODEL_H
#include "stm32f4xx.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
extern NVIC_Type nx_pvd_model_nvic;
void nx_pvd_model_write(volatile uint32_t* address, uint32_t value);
void nx_pvd_model_clear(volatile uint32_t* address, uint32_t mask);
void nx_pvd_model_irq(int irq, bool enabled, bool clear);
void nx_pvd_model_priority(int irq, uint32_t value);
#ifdef __cplusplus
}
#endif
#undef NVIC
#undef NVIC_GetEnableIRQ
#undef NVIC_GetPendingIRQ
#undef NVIC_GetActive
#undef NVIC_GetPriority
#undef NVIC_SetPriority
#undef NVIC_EnableIRQ
#undef NVIC_DisableIRQ
#undef NVIC_ClearPendingIRQ
#define NVIC (&nx_pvd_model_nvic)
#define NVIC_GetEnableIRQ(irq)                                                 \
    ((NVIC->ISER[(unsigned)(irq) / 32U] >> ((unsigned)(irq) % 32U)) & 1U)
#define NVIC_GetPendingIRQ(irq)                                                \
    ((NVIC->ISPR[(unsigned)(irq) / 32U] >> ((unsigned)(irq) % 32U)) & 1U)
#define NVIC_GetActive(irq)                                                    \
    ((NVIC->IABR[(unsigned)(irq) / 32U] >> ((unsigned)(irq) % 32U)) & 1U)
#define NVIC_GetPriority(irq)           (NVIC->IPR[(unsigned)(irq)] >> 4U)
#define NVIC_SetPriority(irq, priority) nx_pvd_model_priority((irq), (priority))
#define NVIC_EnableIRQ(irq)             nx_pvd_model_irq((irq), true, false)
#define NVIC_DisableIRQ(irq)            nx_pvd_model_irq((irq), false, false)
#define NVIC_ClearPendingIRQ(irq)       nx_pvd_model_irq((irq), false, true)
#define NX_STM32_PVD_WRITE(reg, value)  nx_pvd_model_write((reg), (value))
#define NX_STM32_EXTI_WRITE(reg, value) nx_pvd_model_write((reg), (value))
#define NX_STM32_EXTI_READ(reg)         (*(reg))
#define NX_STM32_CLEAR_FLAGS(reg, mask) nx_pvd_model_clear(&(reg), (mask))
#define NX_STM32_PVD_SET_LEVEL(value)                                          \
    nx_pvd_model_write(&PWR->CR, (PWR->CR & ~(uint32_t)PWR_CR_PLS) | (value))
#define NX_STM32_PVD_ENABLE()                                                  \
    nx_pvd_model_write(&PWR->CR, PWR->CR | PWR_CR_PVDE)
#define NX_STM32_PVD_DISABLE()                                                 \
    nx_pvd_model_write(&PWR->CR, PWR->CR & ~(uint32_t)PWR_CR_PVDE)
#endif
