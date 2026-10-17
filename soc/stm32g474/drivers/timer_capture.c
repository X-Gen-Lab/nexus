/**
 * \file            timer_capture.c
 * \brief           Fixed TIM5 CH1 full-width input capture specialization
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32g474_timer.h"
#define NX_CAPTURE_CR1            (TIM5->CR1)
#define NX_CAPTURE_CR2            (TIM5->CR2)
#define NX_CAPTURE_SMCR           (TIM5->SMCR)
#define NX_CAPTURE_PSC            (TIM5->PSC)
#define NX_CAPTURE_ARR            (TIM5->ARR)
#define NX_CAPTURE_CCR            (TIM5->CCR1)
#define NX_CAPTURE_CCMR           (TIM5->CCMR1)
#define NX_CAPTURE_CCER           (TIM5->CCER)
#define NX_CAPTURE_DIER           (TIM5->DIER)
#define NX_CAPTURE_EGR            (TIM5->EGR)
#define NX_CAPTURE_SR             (TIM5->SR)
#define NX_CAPTURE_INITIALIZE     nx_stm32g474_timer_capture_initialize
#define NX_CAPTURE_IRQ            nx_stm32g474_timer_capture_irq
#define NX_CAPTURE_OPS            nx_stm32g474_timer_capture_ops
#define NX_CAPTURE_RELEASE        nx_stm32g474_timer_capture_release
#define NX_CAPTURE_HZ             170000000U
#define NX_CAPTURE_COUNTER_BITS   32U
#define NX_CAPTURE_VECTOR_QUIET() NX_G474_IRQ_AVAILABLE(TIM5_IRQn)
/* Recovery needs readable registers, even if pin or rate facts drifted. */
#define NX_CAPTURE_ACCESS_READY()                                              \
    ((NX_G474_READ(&RCC->APB1ENR1) & RCC_APB1ENR1_TIM5EN) != 0U &&             \
     (NX_G474_READ(&RCC->APB1RSTR1) & RCC_APB1RSTR1_TIM5RST) == 0U)
#define NX_CAPTURE_CLOCK_READY()                                               \
    (g_nx_stm32g474_system.started &&                                          \
     g_nx_stm32g474_system.core_hz == NX_CAPTURE_HZ &&                         \
     g_nx_stm32g474_system.rcc == RCC &&                                       \
     (NX_G474_READ(&RCC->APB1ENR1) & RCC_APB1ENR1_TIM5EN) != 0U &&             \
     (NX_G474_READ(&RCC->APB1RSTR1) & RCC_APB1RSTR1_TIM5RST) == 0U &&          \
     (NX_G474_READ(&RCC->CFGR) & RCC_CFGR_PPRE1) == 0U &&                      \
     (NX_G474_READ(&RCC->AHB2ENR) & RCC_AHB2ENR_GPIOAEN) != 0U &&              \
     (NX_G474_READ(&GPIOA->MODER) & 3U) == 2U &&                               \
     (NX_G474_READ(&GPIOA->AFR[0]) & 15U) == 2U)
#define NX_CAPTURE_CONTROL_OVERLAP(pointer, bytes)                             \
    (capture_overlap((pointer), (bytes), TIM5, sizeof(*TIM5)) ||               \
     capture_overlap((pointer), (bytes), TIM3, sizeof(*TIM3)) ||               \
     capture_overlap((pointer), (bytes), RCC, sizeof(*RCC)) ||                 \
     capture_overlap((pointer), (bytes), GPIOA, sizeof(*GPIOA)) ||             \
     capture_overlap((pointer), (bytes), (const void*)(uintptr_t)NVIC_BASE,    \
                     sizeof(NVIC_Type)))
#ifndef NX_TIMER_READ
#define NX_TIMER_READ(reg) NX_G474_READ(&(reg))
#endif
#ifndef NX_TIMER_WRITE
#define NX_TIMER_WRITE(reg, value) NX_G474_WRITE(&(reg), (uint32_t)(value))
#endif
#include "timer_capture_impl.h"
