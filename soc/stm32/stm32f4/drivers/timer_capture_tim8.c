/**
 * \file            timer_capture_tim8.c
 * \brief           Direct TIM8 capture with immutable channel selection
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f407_timer_advanced.h"
#include "stm32f407_timer_pins.h"
#define NX_CAPTURE_CR1          (TIM8->CR1)
#define NX_CAPTURE_CR2          (TIM8->CR2)
#define NX_CAPTURE_SMCR         (TIM8->SMCR)
#define NX_CAPTURE_PSC          (TIM8->PSC)
#define NX_CAPTURE_ARR          (TIM8->ARR)
#define NX_CAPTURE_CCR1         (TIM8->CCR1)
#define NX_CAPTURE_CCR2         (TIM8->CCR2)
#define NX_CAPTURE_CCR3         (TIM8->CCR3)
#define NX_CAPTURE_CCR4         (TIM8->CCR4)
#define NX_CAPTURE_CCMR1        (TIM8->CCMR1)
#define NX_CAPTURE_CCMR2        (TIM8->CCMR2)
#define NX_CAPTURE_CCER         (TIM8->CCER)
#define NX_CAPTURE_DIER         (TIM8->DIER)
#define NX_CAPTURE_EGR          (TIM8->EGR)
#define NX_CAPTURE_SR           (TIM8->SR)
#define NX_CAPTURE_INITIALIZE   nx_stm32_tim8_timer_capture_initialize
#define NX_CAPTURE_IRQ          nx_stm32_tim8_timer_capture_irq
#define NX_CAPTURE_RELEASE      nx_stm32_tim8_timer_capture_release
#define NX_CAPTURE_OPS          nx_stm32_tim8_timer_capture_ops
#define NX_CAPTURE_HZ           NEXUS_APB2_TIMER_HZ
#define NX_CAPTURE_COUNTER_BITS 16U
#define NX_CAPTURE_CLOCK_READY(channel)                                        \
    ((RCC->APB2ENR & RCC_APB2ENR_TIM8EN) != 0U &&                              \
     (RCC->CFGR & RCC_CFGR_PPRE2) == (NEXUS_CLOCK_CFGR & RCC_CFGR_PPRE2) &&    \
     nx_stm32_capture_pins_ready(TIM8, (channel)))
#define NX_CAPTURE_ACCESS_READY() ((RCC->APB2ENR & RCC_APB2ENR_TIM8EN) != 0U)
#include "timer_capture_impl.h"
