/**
 * \file            timer_capture_tim14.c
 * \brief           Direct TIM14 capture with immutable channel selection
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f407_timer_advanced.h"
#include "stm32f407_timer_pins.h"
#define NX_CAPTURE_CR1          (TIM14->CR1)
#define NX_CAPTURE_CR2          (TIM14->CR2)
#define NX_CAPTURE_SMCR         (TIM14->SMCR)
#define NX_CAPTURE_PSC          (TIM14->PSC)
#define NX_CAPTURE_ARR          (TIM14->ARR)
#define NX_CAPTURE_CCR1         (TIM14->CCR1)
#define NX_CAPTURE_CCR2         (TIM14->CCR2)
#define NX_CAPTURE_CCR3         (TIM14->CCR3)
#define NX_CAPTURE_CCR4         (TIM14->CCR4)
#define NX_CAPTURE_CCMR1        (TIM14->CCMR1)
#define NX_CAPTURE_CCMR2        (TIM14->CCMR2)
#define NX_CAPTURE_CCER         (TIM14->CCER)
#define NX_CAPTURE_DIER         (TIM14->DIER)
#define NX_CAPTURE_EGR          (TIM14->EGR)
#define NX_CAPTURE_SR           (TIM14->SR)
#define NX_CAPTURE_INITIALIZE   nx_stm32_tim14_timer_capture_initialize
#define NX_CAPTURE_IRQ          nx_stm32_tim14_timer_capture_irq
#define NX_CAPTURE_RELEASE      nx_stm32_tim14_timer_capture_release
#define NX_CAPTURE_OPS          nx_stm32_tim14_timer_capture_ops
#define NX_CAPTURE_HZ           NEXUS_APB1_TIMER_HZ
#define NX_CAPTURE_COUNTER_BITS 16U
#define NX_CAPTURE_CLOCK_READY(channel)                                        \
    ((RCC->APB1ENR & RCC_APB1ENR_TIM14EN) != 0U &&                             \
     (RCC->CFGR & RCC_CFGR_PPRE1) == (NEXUS_CLOCK_CFGR & RCC_CFGR_PPRE1) &&    \
     nx_stm32_capture_pins_ready(TIM14, (channel)))
#define NX_CAPTURE_ACCESS_READY() ((RCC->APB1ENR & RCC_APB1ENR_TIM14EN) != 0U)
#define NX_CAPTURE_CHANNELS       1U
#define NX_CAPTURE_HAS_CR2        0
#define NX_CAPTURE_HAS_SMCR       0
#include "timer_capture_impl.h"
