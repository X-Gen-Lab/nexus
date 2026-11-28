/**
 * \file            model.h
 * \brief           F103RB advanced TIM1, GPIO, AFIO and vector model boundary.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_STM32F1_ADVANCED_PWM_MODEL_H
#define NEXUS_TEST_STM32F1_ADVANCED_PWM_MODEL_H

#include "stm32f1xx.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
extern TIM_TypeDef f1_apwm_tim;
extern GPIO_TypeDef f1_apwm_gpioa;
extern GPIO_TypeDef f1_apwm_gpiob;
extern RCC_TypeDef f1_apwm_rcc;
extern AFIO_TypeDef f1_apwm_afio;

void f1_apwm_write(volatile uint32_t* reg, uint32_t value);
bool f1_apwm_irq_enabled(void);
bool f1_apwm_irq_pending(void);
bool f1_apwm_irq_active(void);
uint32_t f1_apwm_irq_priority(void);
void f1_apwm_set_priority(uint32_t priority);
void f1_apwm_enable_irq(void);
void f1_apwm_disable_irq(void);
void f1_apwm_clear_irq(void);
#ifdef __cplusplus
}
#endif

#undef TIM1
#undef GPIOA
#undef GPIOB
#undef RCC
#undef AFIO
#define TIM1                             (&f1_apwm_tim)
#define GPIOA                            (&f1_apwm_gpioa)
#define GPIOB                            (&f1_apwm_gpiob)
#define RCC                              (&f1_apwm_rcc)
#define AFIO                             (&f1_apwm_afio)
#define NX_STM32F1_WRITE(reg, value)     f1_apwm_write((reg), (uint32_t)(value))
#define NX_STM32F1_GPIO_BSRR(reg, value) f1_apwm_write(&(reg)->BSRR, (value))
#define NX_F1_TIM1_IRQ_ENABLED()         f1_apwm_irq_enabled()
#define NX_F1_TIM1_IRQ_PENDING()         f1_apwm_irq_pending()
#define NX_F1_TIM1_IRQ_ACTIVE()          f1_apwm_irq_active()
#define NX_F1_TIM1_IRQ_PRIORITY()        f1_apwm_irq_priority()
#define NX_F1_TIM1_SET_PRIORITY(p)       f1_apwm_set_priority(p)
#define NX_F1_TIM1_IRQ_ENABLE()          f1_apwm_enable_irq()
#define NX_F1_TIM1_IRQ_DISABLE()         f1_apwm_disable_irq()
#define NX_F1_TIM1_IRQ_CLEAR()           f1_apwm_clear_irq()

#endif /* NEXUS_TEST_STM32F1_ADVANCED_PWM_MODEL_H */
