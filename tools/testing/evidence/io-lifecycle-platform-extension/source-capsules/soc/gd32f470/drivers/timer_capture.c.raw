/**
 * \file            timer_capture.c
 * \brief           Direct SDK register specialization; no runtime MMIO table
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_timer_advanced.h"
#include "gd32f4xx.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_timer.h"
#define NX_CAPTURE_CR1        (TIMER_CTL0(TIMER3))
#define NX_CAPTURE_CR2        (TIMER_CTL1(TIMER3))
#define NX_CAPTURE_SMCR       (TIMER_SMCFG(TIMER3))
#define NX_CAPTURE_PSC        (TIMER_PSC(TIMER3))
#define NX_CAPTURE_ARR        (TIMER_CAR(TIMER3))
#define NX_CAPTURE_CCR        (TIMER_CH0CV(TIMER3))
#define NX_CAPTURE_CCMR       (TIMER_CHCTL0(TIMER3))
#define NX_CAPTURE_CCER       (TIMER_CHCTL2(TIMER3))
#define NX_CAPTURE_DIER       (TIMER_DMAINTEN(TIMER3))
#define NX_CAPTURE_EGR        (TIMER_SWEVG(TIMER3))
#define NX_CAPTURE_SR         (TIMER_INTF(TIMER3))
#define NX_CAPTURE_INITIALIZE nx_gd32_timer_capture_initialize
#define NX_CAPTURE_IRQ        nx_gd32_timer_capture_irq
#define NX_CAPTURE_OPS        nx_gd32_timer_capture_ops
#define NX_CAPTURE_HZ         100000000U
#define NX_CAPTURE_CLOCK_READY()                                               \
    ((RCU_APB1EN & RCU_APB1EN_TIMER3EN) != 0U &&                               \
     (RCU_CFG0 & RCU_CFG0_APB1PSC) == RCU_APB1_CKAHB_DIV4 &&                   \
     (RCU_CFG1 & RCU_CFG1_TIMERSEL) == 0U &&                                   \
     (RCU_AHB1EN & RCU_AHB1EN_PBEN) != 0U &&                                   \
     ((GPIO_CTL(GPIOB) >> 12U) & 3U) == 2U &&                                  \
     ((GPIO_AFSEL0(GPIOB) >> 24U) & 15U) == 2U)
#define NX_CAPTURE_RELEASE nx_gd32_timer_capture_release
#include "timer_capture_impl.h"
