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
#include "nexus/core/time.h"
#include "stm32f407_clock.h"
#include "stm32f407_lowpower.h"
#include "stm32f407_system.h"

extern nx_stm32_system_t g_nx_stm32_system;

#define NX_TIMER_STATUS      (&TIM2->SR)
#define NX_TIMER_INTERRUPTS  (&TIM2->DIER)
#define NX_TIMER_COMPARE     (&TIM2->CCR1)
#define NX_COMPARE_FLAG      TIM_SR_CC1IF
#define NX_COMPARE_INTERRUPT TIM_DIER_CC1IE

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
    return nx_stm32_clock_profile_matches(RCC, FLASH);
}

/** \brief Inspect the established exclusive profile without repairing it. */
const nx_freertos_lowpower_port_t* nx_freertos_lowpower_port(void) {
    uint32_t irq = (uint32_t)TIM2_IRQn;
    if (!fixed_run() || configCPU_CLOCK_HZ != NEXUS_CORE_HZ ||
        !g_nx_stm32_system.started ||
        g_nx_stm32_system.core_hz != NEXUS_CORE_HZ ||
        g_nx_stm32_system.timer != TIM2 ||
        (RCC->APB1ENR & RCC_APB1ENR_TIM2EN) == 0u ||
        (RCC->APB1LPENR & RCC_APB1LPENR_TIM2LPEN) == 0u ||
        TIM2->CR1 != TIM_CR1_CEN || TIM2->SMCR != 0u ||
        TIM2->PSC != NEXUS_APB1_TIMER_HZ / 1000000U - 1U ||
        TIM2->ARR != UINT32_MAX || (TIM2->DIER & TIM_DIER_UIE) == 0u ||
        TIM2->CCMR1 != 0u || TIM2->CCER != 0u ||
        (NVIC->ISER[irq / 32u] & (UINT32_C(1) << (irq % 32u))) == 0u) {
        return NULL;
    }
    return &s_port;
}

/** \brief Retain the continuous clock for every outstanding tick/alarm loan. */
bool nx_stm32_lowpower_busy(void) {
    return s_context.paused || s_context.armed || s_context.first_short;
}

/** \brief Own only compare acknowledgement; the overflow owner runs first. */
void nx_stm32_lowpower_alarm_irq(void) {
    nx_timebase_alarm_irq(&s_context);
}

/** \brief The selected wrapper calls the renamed real kernel Tick function. */
void SysTick_Handler(void) {
    nx_timebase_tick_irq(&s_context);
}
