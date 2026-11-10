/**
 * \file            timer_capture_tim3.c
 * \brief           Direct TIM3 capture with immutable channel selection
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_timer_advanced.h"
#include "stm32f407_timer_pins.h"
#define NX_CAPTURE_CR1          (TIM3->CR1)
#define NX_CAPTURE_CR2          (TIM3->CR2)
#define NX_CAPTURE_SMCR         (TIM3->SMCR)
#define NX_CAPTURE_PSC          (TIM3->PSC)
#define NX_CAPTURE_ARR          (TIM3->ARR)
#define NX_CAPTURE_CCR1         (TIM3->CCR1)
#define NX_CAPTURE_CCR2         (TIM3->CCR2)
#define NX_CAPTURE_CCR3         (TIM3->CCR3)
#define NX_CAPTURE_CCR4         (TIM3->CCR4)
#define NX_CAPTURE_CCMR1        (TIM3->CCMR1)
#define NX_CAPTURE_CCMR2        (TIM3->CCMR2)
#define NX_CAPTURE_CCER         (TIM3->CCER)
#define NX_CAPTURE_DIER         (TIM3->DIER)
#define NX_CAPTURE_EGR          (TIM3->EGR)
#define NX_CAPTURE_SR           (TIM3->SR)
#define NX_CAPTURE_INITIALIZE   nx_stm32_tim3_timer_capture_initialize
#define NX_CAPTURE_IRQ          nx_stm32_tim3_timer_capture_irq
#define NX_CAPTURE_RELEASE      nx_stm32_tim3_timer_capture_release
#define NX_CAPTURE_OPS          nx_stm32_tim3_timer_capture_ops
#define NX_CAPTURE_HZ           84000000U
#define NX_CAPTURE_COUNTER_BITS 16U
#define NX_CAPTURE_CLOCK_READY(channel)                                        \
    ((RCC->APB1ENR & RCC_APB1ENR_TIM3EN) != 0U &&                              \
     (RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV4 &&                    \
     nx_stm32_capture_pins_ready(TIM3, (channel)))
#include "timer_capture_impl.h"
