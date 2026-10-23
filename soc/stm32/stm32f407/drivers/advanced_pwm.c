/**
 * \file            advanced_pwm.c
 * \brief           Direct SDK register specialization; no runtime MMIO table
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_timer_advanced.h"
#include "stm32f407xx.h"
#define NX_ADV_CR1        (TIM1->CR1)
#define NX_ADV_SMCR       (TIM1->SMCR)
#define NX_ADV_RCR        (TIM1->RCR)
#define NX_ADV_CR2        (TIM1->CR2)
#define NX_ADV_PSC        (TIM1->PSC)
#define NX_ADV_ARR        (TIM1->ARR)
#define NX_ADV_CCR        (TIM1->CCR1)
#define NX_ADV_CCMR       (TIM1->CCMR1)
#define NX_ADV_CCER       (TIM1->CCER)
#define NX_ADV_DIER       (TIM1->DIER)
#define NX_ADV_EGR        (TIM1->EGR)
#define NX_ADV_SR         (TIM1->SR)
#define NX_ADV_BDTR       (TIM1->BDTR)
#define NX_ADV_INITIALIZE nx_stm32_advanced_pwm_initialize
#define NX_ADV_IRQ        nx_stm32_advanced_pwm_irq
#define NX_ADV_OPS        nx_stm32_advanced_pwm_ops
#define NX_ADV_HZ         168000000U
#define NX_ADV_CLOCK_READY()                                                   \
    ((RCC->APB2ENR & RCC_APB2ENR_TIM1EN) != 0U &&                              \
     (RCC->CFGR & RCC_CFGR_PPRE2) == RCC_CFGR_PPRE2_DIV2 &&                    \
     (RCC->AHB1ENR & (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN)) ==           \
         (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN) &&                        \
     ((GPIOA->MODER >> 16U) & 3U) == 2U && (GPIOA->AFR[1] & 15U) == 1U &&      \
     ((GPIOB->MODER >> 24U) & 15U) == 10U &&                                   \
     ((GPIOB->AFR[1] >> 16U) & 255U) == 17U)
#define NX_ADV_INPUT_VALID(p) ((p) == &GPIOB->IDR)
#define NX_ADV_RELEASE        nx_stm32_advanced_pwm_release
#include "advanced_pwm_impl.h"
