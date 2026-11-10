/**
 * \file            timer_capture_tim5.c
 * \brief           Direct TIM5 capture with immutable channel selection
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f407_timer_advanced.h"
#include "stm32f407_timer_pins.h"
#define NX_CAPTURE_CR1          (TIM5->CR1)
#define NX_CAPTURE_CR2          (TIM5->CR2)
#define NX_CAPTURE_SMCR         (TIM5->SMCR)
#define NX_CAPTURE_PSC          (TIM5->PSC)
#define NX_CAPTURE_ARR          (TIM5->ARR)
#define NX_CAPTURE_CCR1         (TIM5->CCR1)
#define NX_CAPTURE_CCR2         (TIM5->CCR2)
#define NX_CAPTURE_CCR3         (TIM5->CCR3)
#define NX_CAPTURE_CCR4         (TIM5->CCR4)
#define NX_CAPTURE_CCMR1        (TIM5->CCMR1)
#define NX_CAPTURE_CCMR2        (TIM5->CCMR2)
#define NX_CAPTURE_CCER         (TIM5->CCER)
#define NX_CAPTURE_DIER         (TIM5->DIER)
#define NX_CAPTURE_EGR          (TIM5->EGR)
#define NX_CAPTURE_SR           (TIM5->SR)
#define NX_CAPTURE_INITIALIZE   nx_stm32_tim5_timer_capture_initialize
#define NX_CAPTURE_IRQ          nx_stm32_tim5_timer_capture_irq
#define NX_CAPTURE_RELEASE      nx_stm32_tim5_timer_capture_release
#define NX_CAPTURE_OPS          nx_stm32_tim5_timer_capture_ops
#define NX_CAPTURE_HZ           NEXUS_APB1_TIMER_HZ
#define NX_CAPTURE_COUNTER_BITS 32U
#define NX_CAPTURE_CLOCK_READY(channel)                                        \
    ((RCC->APB1ENR & RCC_APB1ENR_TIM5EN) != 0U &&                              \
     (RCC->CFGR & RCC_CFGR_PPRE1) == (NEXUS_CLOCK_CFGR & RCC_CFGR_PPRE1) &&    \
     nx_stm32_capture_pins_ready(TIM5, (channel)))
#define NX_CAPTURE_ACCESS_READY() ((RCC->APB1ENR & RCC_APB1ENR_TIM5EN) != 0U)
#include "timer_capture_impl.h"
