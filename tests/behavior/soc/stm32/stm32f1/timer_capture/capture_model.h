/**
 * \file            capture_model.h
 * \brief           Fault-injected TIM4, EXTI and NVIC register behavior.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_F1_TIM4_CAPTURE_MODEL_H
#define NEXUS_TEST_F1_TIM4_CAPTURE_MODEL_H

#include "stm32f1xx.h"

#ifdef __cplusplus
extern "C" {
#endif
void nx_f1_capture_write(volatile uint32_t* reg, uint32_t value);
void nx_f1_capture_bsrr(GPIO_TypeDef* gpio, uint32_t value);
uint32_t nx_f1_capture_ccr(void);
uint32_t nx_f1_capture_get_enable(IRQn_Type irq);
uint32_t nx_f1_capture_get_active(IRQn_Type irq);
uint32_t nx_f1_capture_get_pending(IRQn_Type irq);
uint32_t nx_f1_capture_get_priority(IRQn_Type irq);
void nx_f1_capture_enable(IRQn_Type irq);
void nx_f1_capture_disable(IRQn_Type irq);
void nx_f1_capture_clear(IRQn_Type irq);
void nx_f1_capture_set_priority(IRQn_Type irq, uint32_t priority);
#ifdef __cplusplus
}
#endif

#define NX_STM32F1_WRITE(reg, value)      nx_f1_capture_write((reg), (value))
#define NX_STM32F1_GPIO_BSRR(gpio, value) nx_f1_capture_bsrr((gpio), (value))
#define NX_STM32F1_CAPTURE_CCR()          nx_f1_capture_ccr()
#undef NVIC_GetEnableIRQ
#undef NVIC_GetActive
#undef NVIC_GetPendingIRQ
#undef NVIC_GetPriority
#undef NVIC_EnableIRQ
#undef NVIC_DisableIRQ
#undef NVIC_ClearPendingIRQ
#undef NVIC_SetPriority
#define NVIC_GetEnableIRQ    nx_f1_capture_get_enable
#define NVIC_GetActive       nx_f1_capture_get_active
#define NVIC_GetPendingIRQ   nx_f1_capture_get_pending
#define NVIC_GetPriority     nx_f1_capture_get_priority
#define NVIC_EnableIRQ       nx_f1_capture_enable
#define NVIC_DisableIRQ      nx_f1_capture_disable
#define NVIC_ClearPendingIRQ nx_f1_capture_clear
#define NVIC_SetPriority     nx_f1_capture_set_priority

#endif
