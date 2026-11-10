/**
 * \file            advanced_pwm.c
 * \brief           TIM1 complementary groups share the reviewed C engine
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f407_timer_advanced.h"
#include "stm32f407_timer_pins.h"
#define NX_ADV_CR1        (TIM1->CR1)
#define NX_ADV_REGISTERS  TIM1
#define NX_ADV_SMCR       (TIM1->SMCR)
#define NX_ADV_RCR        (TIM1->RCR)
#define NX_ADV_CR2        (TIM1->CR2)
#define NX_ADV_PSC        (TIM1->PSC)
#define NX_ADV_ARR        (TIM1->ARR)
#define NX_ADV_CCR1       (TIM1->CCR1)
#define NX_ADV_CCR2       (TIM1->CCR2)
#define NX_ADV_CCR3       (TIM1->CCR3)
#define NX_ADV_CCMR1      (TIM1->CCMR1)
#define NX_ADV_CCMR2      (TIM1->CCMR2)
#define NX_ADV_CCER       (TIM1->CCER)
#define NX_ADV_DIER       (TIM1->DIER)
#define NX_ADV_EGR        (TIM1->EGR)
#define NX_ADV_SR         (TIM1->SR)
#define NX_ADV_BDTR       (TIM1->BDTR)
#define NX_ADV_INITIALIZE nx_stm32_advanced_pwm_initialize
#define NX_ADV_IRQ        nx_stm32_advanced_pwm_irq
#define NX_ADV_RELEASE    nx_stm32_advanced_pwm_release
#define NX_ADV_OPS        nx_stm32_advanced_pwm_ops
#define NX_ADV_HZ         NEXUS_APB2_TIMER_HZ
#define NX_ADV_BREAK_MASK 4096U
#define NX_ADV_CLOCK_READY(channel)                                            \
    ((RCC->APB2ENR & RCC_APB2ENR_TIM1EN) != 0U &&                              \
     (RCC->CFGR & RCC_CFGR_PPRE2) == (NEXUS_CLOCK_CFGR & RCC_CFGR_PPRE2) &&    \
     nx_stm32_advanced_pins_ready(TIM1, (channel)))
#define NX_ADV_INPUT_VALID(p) ((p) == &GPIOB->IDR)
#define NX_ADV_ACCESS_READY() ((RCC->APB2ENR & RCC_APB2ENR_TIM1EN) != 0U)
#include "advanced_pwm_impl.h"
