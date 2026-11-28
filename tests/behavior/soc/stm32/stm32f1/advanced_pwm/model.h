/**
 * \file            model.h
 * \brief           F103 advanced PWM GPIO and vector model boundary.
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
extern TIM_TypeDef f1_apwm_peer_tim;
extern GPIO_TypeDef f1_apwm_gpioa;
extern GPIO_TypeDef f1_apwm_gpiob;
extern GPIO_TypeDef f1_apwm_gpioc;
extern RCC_TypeDef f1_apwm_rcc;
extern AFIO_TypeDef f1_apwm_afio;

void f1_apwm_write(volatile uint32_t* reg, uint32_t value);
void f1_apwm_check_vector(IRQn_Type irq);
bool f1_apwm_irq_enabled(void);
bool f1_apwm_irq_pending(void);
bool f1_apwm_irq_active(void);
uint32_t f1_apwm_irq_priority(void);
void f1_apwm_set_priority(uint32_t priority);
void f1_apwm_enable_irq(void);
void f1_apwm_disable_irq(void);
void f1_apwm_clear_irq(void);
#if defined(TEST_F1_CYCLE)
extern ADC_TypeDef f1_apwm_adc;
extern uintptr_t f1_cycle_ram_begin;
extern uintptr_t f1_cycle_ram_end;
uint32_t f1_cycle_GetEnableIRQ(IRQn_Type irq);
uint32_t f1_cycle_GetPendingIRQ(IRQn_Type irq);
uint32_t f1_cycle_GetActive(IRQn_Type irq);
uint32_t f1_cycle_GetPriority(IRQn_Type irq);
void f1_cycle_SetPriority(IRQn_Type irq, uint32_t priority);
void f1_cycle_EnableIRQ(IRQn_Type irq);
void f1_cycle_DisableIRQ(IRQn_Type irq);
void f1_cycle_ClearPendingIRQ(IRQn_Type irq);
#endif
#ifdef __cplusplus
}
#endif

#undef TIM1
#undef GPIOA
#undef GPIOB
#undef GPIOC
#undef RCC
#undef AFIO
#if defined(STM32F103xE)
#undef TIM8
#define TIM8 (&f1_apwm_tim)
#define TIM1 (&f1_apwm_peer_tim)
#else
#define TIM1 (&f1_apwm_tim)
#endif
#define GPIOA                            (&f1_apwm_gpioa)
#define GPIOB                            (&f1_apwm_gpiob)
#define GPIOC                            (&f1_apwm_gpioc)
#define RCC                              (&f1_apwm_rcc)
#define AFIO                             (&f1_apwm_afio)
#define NX_STM32F1_WRITE(reg, value)     f1_apwm_write((reg), (uint32_t)(value))
#define NX_STM32F1_GPIO_BSRR(reg, value) f1_apwm_write(&(reg)->BSRR, (value))
#define NX_F1_APWM_IRQ_ENABLED(irq)                                            \
    (f1_apwm_check_vector(irq), f1_apwm_irq_enabled())
#define NX_F1_APWM_IRQ_PENDING(irq)                                            \
    (f1_apwm_check_vector(irq), f1_apwm_irq_pending())
#define NX_F1_APWM_IRQ_ACTIVE(irq)                                             \
    (f1_apwm_check_vector(irq), f1_apwm_irq_active())
#define NX_F1_APWM_IRQ_PRIORITY(irq)                                           \
    (f1_apwm_check_vector(irq), f1_apwm_irq_priority())
#define NX_F1_APWM_SET_PRIORITY(irq, p)                                        \
    (f1_apwm_check_vector(irq), f1_apwm_set_priority(p))
#define NX_F1_APWM_IRQ_ENABLE(irq)                                             \
    (f1_apwm_check_vector(irq), f1_apwm_enable_irq())
#define NX_F1_APWM_IRQ_DISABLE(irq)                                            \
    (f1_apwm_check_vector(irq), f1_apwm_disable_irq())
#define NX_F1_APWM_IRQ_CLEAR(irq)                                              \
    (f1_apwm_check_vector(irq), f1_apwm_clear_irq())

#if defined(TEST_F1_CYCLE)
#undef ADC1
#undef WRITE_REG
#undef SET_BIT
#define ADC1                            (&f1_apwm_adc)
#define WRITE_REG(reg, value)           f1_apwm_write(&(reg), (uint32_t)(value))
#define SET_BIT(reg, bits)              WRITE_REG(reg, (reg) | (bits))
#define NX_F1_CYCLE_IRQ(operation, ...) f1_cycle_##operation(__VA_ARGS__)
#define NX_F1_CYCLE_RAM_BEGIN           f1_cycle_ram_begin
#define NX_F1_CYCLE_RAM_END             f1_cycle_ram_end
#endif

#endif /* NEXUS_TEST_STM32F1_ADVANCED_PWM_MODEL_H */
