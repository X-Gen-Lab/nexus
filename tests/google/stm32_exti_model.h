/**
 * \file            stm32_exti_model.h
 * \brief           External F407 EXTI W1C and NVIC acknowledgement boundaries
 * \author          Nexus Team
 */
#ifndef NX_STM32_EXTI_MODEL_H
#define NX_STM32_EXTI_MODEL_H
#include "nexus/arch/arch.h"
#include "stm32f407xx.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t ISER[3];
    uint32_t ISPR[3];
    uint32_t IABR[3];
} nx_stm32_exti_nvic_model_t;
extern nx_stm32_exti_nvic_model_t g_stm32_exti_nvic;
uint32_t nx_stm32_exti_model_read(volatile uint32_t* reg);
void nx_stm32_exti_model_write(volatile uint32_t* reg, uint32_t value);
void nx_stm32_exti_model_clear(volatile uint32_t* reg, uint32_t mask);
void nx_stm32_exti_model_disable(int irq);
void nx_stm32_exti_model_clear_irq(int irq);
#undef NVIC
#define NVIC                    (&g_stm32_exti_nvic)
#define NX_STM32_EXTI_READ(reg) nx_stm32_exti_model_read(reg)
#define NX_STM32_EXTI_WRITE(reg, value)                                        \
    nx_stm32_exti_model_write((reg), (value))
#define NX_STM32_CLEAR_FLAGS(reg, mask)                                        \
    nx_stm32_exti_model_clear(&(reg), (mask))
#define NX_STM32_NVIC_DISABLE(irq) nx_stm32_exti_model_disable((int)(irq))
#define NX_STM32_NVIC_CLEAR(irq)   nx_stm32_exti_model_clear_irq((int)(irq))
#ifdef __cplusplus
}
#endif
#endif
