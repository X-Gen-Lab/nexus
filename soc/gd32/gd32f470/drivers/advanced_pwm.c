/**
 * \file            advanced_pwm.c
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
#define NX_ADV_CR1        (TIMER_CTL0(TIMER0))
#define NX_ADV_SMCR       (TIMER_SMCFG(TIMER0))
#define NX_ADV_RCR        (TIMER_CREP(TIMER0))
#define NX_ADV_CR2        (TIMER_CTL1(TIMER0))
#define NX_ADV_PSC        (TIMER_PSC(TIMER0))
#define NX_ADV_ARR        (TIMER_CAR(TIMER0))
#define NX_ADV_CCR        (TIMER_CH0CV(TIMER0))
#define NX_ADV_CCMR       (TIMER_CHCTL0(TIMER0))
#define NX_ADV_CCER       (TIMER_CHCTL2(TIMER0))
#define NX_ADV_DIER       (TIMER_DMAINTEN(TIMER0))
#define NX_ADV_EGR        (TIMER_SWEVG(TIMER0))
#define NX_ADV_SR         (TIMER_INTF(TIMER0))
#define NX_ADV_BDTR       (TIMER_CCHP(TIMER0))
#define NX_ADV_INITIALIZE nx_gd32_advanced_pwm_initialize
#define NX_ADV_IRQ        nx_gd32_advanced_pwm_irq
#define NX_ADV_OPS        nx_gd32_advanced_pwm_ops
#define NX_ADV_HZ         200000000U
#define NX_ADV_CLOCK_READY()                                                   \
    ((RCU_APB2EN & RCU_APB2EN_TIMER0EN) != 0U &&                               \
     (RCU_CFG0 & RCU_CFG0_APB2PSC) == RCU_APB2_CKAHB_DIV2 &&                   \
     (RCU_CFG1 & RCU_CFG1_TIMERSEL) == 0U &&                                   \
     (RCU_AHB1EN & (RCU_AHB1EN_PAEN | RCU_AHB1EN_PBEN)) ==                     \
         (RCU_AHB1EN_PAEN | RCU_AHB1EN_PBEN) &&                                \
     ((GPIO_CTL(GPIOA) >> 16U) & 3U) == 2U &&                                  \
     (GPIO_AFSEL1(GPIOA) & 15U) == 1U &&                                       \
     ((GPIO_CTL(GPIOB) >> 24U) & 15U) == 10U &&                                \
     ((GPIO_AFSEL1(GPIOB) >> 16U) & 255U) == 17U)
#define NX_ADV_INPUT_VALID(p) ((p) == &GPIO_ISTAT(GPIOB))
#define NX_ADV_RELEASE        nx_gd32_advanced_pwm_release
#include "advanced_pwm_impl.h"
