/**
 * \file            advanced_pwm_tim8.c
 * \brief           TIM8 specialization of the shared complementary PWM engine
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_timer_advanced.h"
#include "stm32f407xx.h"
#define NX_ADV_CR1        (TIM8->CR1)
#define NX_ADV_SMCR       (TIM8->SMCR)
#define NX_ADV_RCR        (TIM8->RCR)
#define NX_ADV_CR2        (TIM8->CR2)
#define NX_ADV_PSC        (TIM8->PSC)
#define NX_ADV_ARR        (TIM8->ARR)
#define NX_ADV_CCR        (TIM8->CCR1)
#define NX_ADV_CCMR       (TIM8->CCMR1)
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
#define NX_ADV_CLOCK_READY()                                                   \
    ((RCC->APB2ENR & RCC_APB2ENR_TIM8EN) != 0U &&                              \
     (RCC->CFGR & RCC_CFGR_PPRE2) == RCC_CFGR_PPRE2_DIV2 &&                    \
     (RCC->AHB1ENR & (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN)) ==           \
         (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN) &&                        \
     ((GPIOC->MODER >> 12U) & 3U) == 2U &&                                     \
     ((GPIOC->AFR[0] >> 24U) & 15U) == 3U &&                                   \
     ((GPIOA->MODER >> 12U) & 15U) == 10U &&                                   \
     ((GPIOA->AFR[0] >> 24U) & 255U) == 51U)
#define NX_ADV_INPUT_VALID(p) ((p) == &GPIOA->IDR)
#include "advanced_pwm_impl.h"
