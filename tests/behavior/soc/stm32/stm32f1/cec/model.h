/**
 * \file            model.h
 * \brief           Test register hooks for the production F100 CEC driver.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_F100_CEC_MODEL_H
#define NEXUS_TEST_F100_CEC_MODEL_H

#include "stm32f1xx.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
extern CEC_TypeDef nx_cec_registers;
extern AFIO_TypeDef nx_cec_afio;
extern GPIO_TypeDef nx_cec_gpio;
extern RCC_TypeDef nx_cec_rcc;
extern bool nx_cec_irq_enable;
extern bool nx_cec_irq_pending;
extern uint32_t nx_cec_irq_priority;
void nx_cec_model_write(volatile uint32_t* reg, uint32_t value);
void nx_cec_model_bsrr(GPIO_TypeDef* gpio, uint32_t value);
#ifdef __cplusplus
}
#endif

#undef CEC
#undef AFIO
#undef GPIOB
#undef RCC
#define CEC                                     (&nx_cec_registers)
#define AFIO                                    (&nx_cec_afio)
#define GPIOB                                   (&nx_cec_gpio)
#define RCC                                     (&nx_cec_rcc)
#define NX_STM32F1_WRITE(reg, value)            nx_cec_model_write((reg), (value))
#define NX_STM32F1_GPIO_BSRR(gpio, value)       nx_cec_model_bsrr((gpio), (value))
#define NX_STM32F1_IRQ_ENABLED(irq)             (nx_cec_irq_enable)
#define NX_STM32F1_IRQ_PENDING(irq)             (nx_cec_irq_pending)
#define NX_STM32F1_IRQ_ACTIVE(irq)              false
#define NX_STM32F1_IRQ_PRIORITY(irq)            (nx_cec_irq_priority)
#define NX_STM32F1_IRQ_SET_PRIORITY(irq, value) (nx_cec_irq_priority = (value))
#define NX_STM32F1_IRQ_ENABLE(irq)              (nx_cec_irq_enable = true)
#define NX_STM32F1_IRQ_DISABLE(irq)             (nx_cec_irq_enable = false)
#define NX_STM32F1_IRQ_CLEAR(irq)               (nx_cec_irq_pending = false)
#endif
