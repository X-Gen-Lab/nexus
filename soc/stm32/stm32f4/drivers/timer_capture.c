/**
 * \file            timer_capture.c
 * \brief           Direct SDK register specialization; no runtime MMIO table
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_timer_advanced.h"
#include "stm32f407xx.h"
#define NX_CAPTURE_CR1        (TIM4->CR1)
#define NX_CAPTURE_CR2        (TIM4->CR2)
#define NX_CAPTURE_SMCR       (TIM4->SMCR)
#define NX_CAPTURE_PSC        (TIM4->PSC)
#define NX_CAPTURE_ARR        (TIM4->ARR)
#define NX_CAPTURE_CCR        (TIM4->CCR1)
#define NX_CAPTURE_CCMR       (TIM4->CCMR1)
#define NX_CAPTURE_CCER       (TIM4->CCER)
#define NX_CAPTURE_DIER       (TIM4->DIER)
#define NX_CAPTURE_EGR        (TIM4->EGR)
#define NX_CAPTURE_SR         (TIM4->SR)
#define NX_CAPTURE_INITIALIZE nx_stm32_timer_capture_initialize
#define NX_CAPTURE_IRQ        nx_stm32_timer_capture_irq
#define NX_CAPTURE_OPS        nx_stm32_timer_capture_ops
#define NX_CAPTURE_HZ         84000000U
#define NX_CAPTURE_CLOCK_READY()                                               \
    ((RCC->APB1ENR & RCC_APB1ENR_TIM4EN) != 0U &&                              \
     (RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV4 &&                    \
     (RCC->AHB1ENR & RCC_AHB1ENR_GPIOBEN) != 0U &&                             \
     ((GPIOB->MODER >> 12U) & 3U) == 2U &&                                     \
     ((GPIOB->AFR[0] >> 24U) & 15U) == 2U)
#define NX_CAPTURE_RELEASE nx_stm32_timer_capture_release
#include "timer_capture_impl.h"
