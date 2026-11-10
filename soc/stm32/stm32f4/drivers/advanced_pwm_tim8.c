/**
 * \file            advanced_pwm_tim8.c
 * \brief           TIM8 complementary groups share the reviewed C engine
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_timer_advanced.h"
#include "stm32f407_timer_pins.h"
#define NX_ADV_CR1        (TIM8->CR1)
#define NX_ADV_SMCR       (TIM8->SMCR)
#define NX_ADV_RCR        (TIM8->RCR)
#define NX_ADV_CR2        (TIM8->CR2)
#define NX_ADV_PSC        (TIM8->PSC)
#define NX_ADV_ARR        (TIM8->ARR)
#define NX_ADV_CCR1       (TIM8->CCR1)
#define NX_ADV_CCR2       (TIM8->CCR2)
#define NX_ADV_CCR3       (TIM8->CCR3)
#define NX_ADV_CCMR1      (TIM8->CCMR1)
#define NX_ADV_CCMR2      (TIM8->CCMR2)
#define NX_ADV_CCER       (TIM8->CCER)
#define NX_ADV_DIER       (TIM8->DIER)
#define NX_ADV_EGR        (TIM8->EGR)
#define NX_ADV_SR         (TIM8->SR)
#define NX_ADV_BDTR       (TIM8->BDTR)
#define NX_ADV_INITIALIZE nx_stm32_tim8_advanced_pwm_initialize
#define NX_ADV_IRQ        nx_stm32_tim8_advanced_pwm_irq
#define NX_ADV_RELEASE    nx_stm32_tim8_advanced_pwm_release
#define NX_ADV_OPS        nx_stm32_tim8_advanced_pwm_ops
#define NX_ADV_HZ         168000000U
#define NX_ADV_BREAK_MASK 64U
#define NX_ADV_CLOCK_READY(channel)                                            \
    ((RCC->APB2ENR & RCC_APB2ENR_TIM8EN) != 0U &&                              \
     (RCC->CFGR & RCC_CFGR_PPRE2) == RCC_CFGR_PPRE2_DIV2 &&                    \
     nx_stm32_advanced_pins_ready(TIM8, (channel)))
#define NX_ADV_INPUT_VALID(p) ((p) == &GPIOA->IDR)
#include "advanced_pwm_impl.h"
