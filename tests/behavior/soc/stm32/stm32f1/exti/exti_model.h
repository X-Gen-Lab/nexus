/**
 * \file            exti_model.h
 * \brief           F103 shared vector, PR and GPIO latch model.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_F1_EXTI_MODEL_H
#define NEXUS_TEST_F1_EXTI_MODEL_H
#include "stm32f1xx.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
void nx_exti_model_clear(uint32_t mask);
void nx_exti_model_bsrr(GPIO_TypeDef* gpio, uint32_t value);
bool nx_exti_model_irq_enabled(void);
bool nx_exti_model_irq_pending(void);
bool nx_exti_model_irq_active(void);
uint32_t nx_exti_model_irq_priority(void);
void nx_exti_model_set_priority(uint32_t priority);
void nx_exti_model_irq_enable(void);
void nx_exti_model_irq_disable(void);
void nx_exti_model_irq_clear(void);
#ifdef __cplusplus
}
#endif

#define NX_STM32F1_EXTI_ACK(mask)           nx_exti_model_clear(mask)
#define NX_STM32F1_GPIO_BSRR(gpio, value)   nx_exti_model_bsrr((gpio), (value))
#define NX_STM32F1_EXTI_IRQ_ENABLED()       nx_exti_model_irq_enabled()
#define NX_STM32F1_EXTI_IRQ_PENDING()       nx_exti_model_irq_pending()
#define NX_STM32F1_EXTI_IRQ_ACTIVE()        nx_exti_model_irq_active()
#define NX_STM32F1_EXTI_IRQ_PRIORITY()      nx_exti_model_irq_priority()
#define NX_STM32F1_EXTI_SET_PRIORITY(value) nx_exti_model_set_priority(value)
#define NX_STM32F1_EXTI_IRQ_ENABLE()        nx_exti_model_irq_enable()
#define NX_STM32F1_EXTI_IRQ_DISABLE()       nx_exti_model_irq_disable()
#define NX_STM32F1_EXTI_IRQ_CLEAR()         nx_exti_model_irq_clear()
#endif
