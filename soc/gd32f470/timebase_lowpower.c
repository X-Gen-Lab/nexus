/**
 * \file            timebase_lowpower.c
 * \brief           Selected private continuous timer and SysTick sleep binding
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "timebase_lowpower.h"
#include "FreeRTOS.h"
#include "gd32f470_lowpower.h"
#include "gd32f4xx.h"
#include "nexus/core/time.h"

#define NX_TIMER_STATUS      (&TIMER_INTF(TIMER1))
#define NX_TIMER_INTERRUPTS  (&TIMER_DMAINTEN(TIMER1))
#define NX_TIMER_COMPARE     (&TIMER_CH0CV(TIMER1))
#define NX_COMPARE_FLAG      TIMER_INTF_CH0IF
#define NX_COMPARE_INTERRUPT TIMER_DMAINTEN_CH0IE

/** \brief Reuse the boot time domain, never create a second clock epoch. */
static uint64_t clock_now(void* context) {
    (void)context;
    return nx_time_now_us();
}

static const nx_timebase_registers_t s_registers = {
    .tick_control = &SysTick->CTRL,
    .tick_reload = &SysTick->LOAD,
    .tick_value = &SysTick->VAL,
    .pending = &SCB->ICSR,
    .timer_status = NX_TIMER_STATUS,
    .timer_interrupts = NX_TIMER_INTERRUPTS,
    .timer_compare = NX_TIMER_COMPARE,
    .compare_flag = NX_COMPARE_FLAG,
    .compare_interrupt = NX_COMPARE_INTERRUPT,
    .cycles_per_us = configCPU_CLOCK_HZ / 1000000u,
    .full_reload = configCPU_CLOCK_HZ / configTICK_RATE_HZ - 1u,
    .now_us = clock_now,
    .clock_context = NULL};
static nx_timebase_lowpower_t s_context = {.registers = &s_registers};
static const nx_freertos_lowpower_port_t s_port = {
    .context = &s_context,
    .now_us = clock_now,
    .pause_tick = nx_timebase_pause_tick,
    .arm_deadline = nx_timebase_arm_deadline,
    .disarm = nx_timebase_disarm,
    .resume_tick = nx_timebase_resume_tick};

/** \brief Actual source, PLL, bus mux/divisors and established Flash latency.
 */
static bool fixed_run(void) {
    const uint32_t pll_mask =
        (uint32_t)(RCU_PLL_PLLPSC | RCU_PLL_PLLN | RCU_PLL_PLLP |
                   RCU_PLL_PLLSEL | RCU_PLL_PLLQ);
    const uint32_t pll = 25U | (400U << 6U) | RCU_PLLSRC_HXTAL | (9U << 24U);
    const uint32_t ready =
        RCU_CTL_HXTALEN | RCU_CTL_HXTALSTB | RCU_CTL_PLLEN | RCU_CTL_PLLSTB;
    const uint32_t bus_mask =
        (uint32_t)(RCU_CFG0_SCS | RCU_CFG0_SCSS | RCU_CFG0_AHBPSC |
                   RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC);
    const uint32_t bus = RCU_CKSYSSRC_PLLP | RCU_SCSS_PLLP |
                         RCU_APB1_CKAHB_DIV4 | RCU_APB2_CKAHB_DIV2;
    return (RCU_CTL & ready) == ready && (RCU_PLL & pll_mask) == pll &&
           (RCU_CFG0 & bus_mask) == bus &&
           (FMC_WS & FMC_WC_WSCNT) == WS_WSCNT_7 &&
           (RCU_PLLSSCTL & RCU_PLLSSCTL_SSCGON) == 0U &&
           (RCU_CFG1 & RCU_CFG1_TIMERSEL) == 0U;
}

/** \brief Inspect the established exclusive profile without repairing it. */
const nx_freertos_lowpower_port_t* nx_freertos_lowpower_port(void) {
    uint32_t irq = (uint32_t)TIMER1_IRQn;
    if (!fixed_run() || configCPU_CLOCK_HZ != 200000000u ||
        SystemCoreClock != 200000000u ||
        (RCU_APB1EN & RCU_APB1EN_TIMER1EN) == 0u ||
        (RCU_APB1SPEN & RCU_APB1SPEN_TIMER1SPEN) == 0u ||
        TIMER_CTL0(TIMER1) != TIMER_CTL0_CEN || TIMER_SMCFG(TIMER1) != 0u ||
        TIMER_PSC(TIMER1) != 99u || TIMER_CAR(TIMER1) != UINT32_MAX ||
        (TIMER_DMAINTEN(TIMER1) & TIMER_DMAINTEN_UPIE) == 0u ||
        TIMER_CHCTL0(TIMER1) != 0u || TIMER_CHCTL2(TIMER1) != 0u ||
        (NVIC->ISER[irq / 32u] & (UINT32_C(1) << (irq % 32u))) == 0u) {
        return NULL;
    }
    return &s_port;
}

/** \brief Retain the continuous clock for every outstanding tick/alarm loan. */
bool nx_gd32_lowpower_busy(void) {
    return s_context.paused || s_context.armed;
}

/** \brief Own only compare acknowledgement; the overflow owner runs first. */
void nx_gd32_lowpower_alarm_irq(void) {
    nx_timebase_alarm_irq(&s_context);
}

/** \brief The selected wrapper calls the renamed real kernel Tick function. */
void SysTick_Handler(void) {
    nx_timebase_tick_irq(&s_context);
}
