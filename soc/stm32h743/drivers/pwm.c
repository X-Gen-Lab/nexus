/**
 * \file            pwm.c
 * \brief           H743 exclusive fixed-base CH1 PWM with safe GPIO stop
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_provider.h"
#if NEXUS_CORE_HZ == 384000000U
#include "stm32h743_clock.h"
#endif
#define PWM_MODE                                                               \
    ((uint32_t)(TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1PE))
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static uint32_t timer_hz(void) {
#if NEXUS_CORE_HZ == 384000000U
    return 192000000U;
#else
    return g_nx_stm32h743_system.core_hz;
#endif
}
static bool valid(const nx_stm32h743_pwm_state_t* p) {
    return p != NULL && p->registers != NULL && p->idle != NULL &&
           p->idle->registers != NULL && p->reset != NULL && p->pin < 16U &&
           p->idle->mask == (1U << p->pin) && p->idle->output &&
           (p->inactive & ~p->idle->mask) == 0U && p->period_ticks != 0U &&
           p->period_ticks <= 65535U && p->duty_ticks <= p->period_ticks &&
           p->tick_hz != 0U && timer_hz() % p->tick_hz == 0U &&
           timer_hz() / p->tick_hz <= 65536U && p->poll_limit != 0U;
}
static bool clock_ready(const nx_stm32h743_pwm_state_t* p) {
    RCC_TypeDef* r = g_nx_stm32h743_system.rcc;
    if (r == NULL || p->reset != &r->APB1LRSTR ||
        !g_nx_stm32h743_system.started ||
#if NEXUS_CORE_HZ == 384000000U
        !nx_stm32h743_pll384_clocks_ready())
#else
        (g_nx_stm32h743_system.core_hz != 32000000U &&
         g_nx_stm32h743_system.core_hz != 64000000U))
#endif
        return false;
    if (p->reset_mask == RCC_APB1LRSTR_TIM3RST)
        return (NX_H743_READ(&r->APB1LENR) & RCC_APB1LENR_TIM3EN) != 0U;
    return false;
}
static bool write_checked(volatile uint32_t* reg, uint32_t value) {
    NX_H743_WRITE(reg, value);
    nx_arch_dsb();
    return NX_H743_READ(reg) == value;
}
static bool cold(const nx_stm32h743_pwm_state_t* p) {
    TIM_TypeDef* r = p->registers;
    return NX_H743_READ(&r->CR1) == 0U && NX_H743_READ(&r->CR2) == 0U &&
           NX_H743_READ(&r->SMCR) == 0U && NX_H743_READ(&r->DIER) == 0U &&
           NX_H743_READ(&r->CCER) == 0U && NX_H743_READ(&r->CCMR1) == 0U &&
           NX_H743_READ(&r->CCMR2) == 0U &&
           (NX_H743_READ(p->reset) & p->reset_mask) == 0U;
}
/** \brief Hold TRGO on disabled CEN while UG loads the preload registers. */
static nx_result_t epoch(nx_stm32h743_pwm_state_t* p) {
    TIM_TypeDef* r = p->registers;
    if (!write_checked(&r->CR2, TIM_CR2_MMS_0) || !write_checked(&r->SR, 0U))
        return NX_ERROR_IO;
    NX_H743_WRITE(&r->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    uint32_t remaining = p->poll_limit;
    do {
        NX_H743_PWM_POLL(p);
        if ((NX_H743_READ(&r->SR) & TIM_SR_UIF) != 0U)
            break;
    } while (--remaining != 0U);
    if ((NX_H743_READ(&r->SR) & TIM_SR_UIF) == 0U)
        return NX_ERROR_TIMEOUT;
    if (!write_checked(&r->SR, 0U) || !write_checked(&r->CR2, 0U) ||
        !write_checked(&r->CNT, 0U))
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
/** \brief Latch the Board idle before exposing the GPIO output mode. */
static nx_result_t idle_level(nx_stm32h743_pwm_state_t* p) {
    GPIO_TypeDef* gpio = p->idle->registers;
    uint32_t mask = p->idle->mask;
    NX_H743_WRITE(&gpio->BSRR, p->inactive | ((mask & ~p->inactive) << 16U));
    nx_arch_dsb();
    if ((NX_H743_READ(&gpio->ODR) & mask) != p->inactive ||
        (NX_H743_READ(&gpio->OTYPER) & mask) != 0U)
        return NX_ERROR_IO;
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    uint32_t shift = (uint32_t)p->pin * 2U;
    uint32_t mode =
        (NX_H743_READ(&gpio->MODER) & ~(3U << shift)) | (1U << shift);
    nx_result_t result =
        write_checked(&gpio->MODER, mode) ? NX_SUCCESS : NX_ERROR_IO;
    nx_arch_irq_restore(previous);
    return result;
}
nx_result_t nx_stm32h743_pwm_initialize(nx_stm32h743_pwm_state_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->retained || p->initialized || p->idle->retained ||
        p->idle->initialized)
        return NX_ERROR_BUSY;
    if (!clock_ready(p))
        return NX_ERROR_STATE;
    if (!cold(p))
        return NX_ERROR_BUSY;
    p->retained = true;
    p->fault = false;
    p->running = false;
    nx_result_t result = nx_stm32h743_gpio_initialize(p->idle, p->inactive);
    if (result != NX_SUCCESS)
        return result;
    TIM_TypeDef* r = p->registers;
    if (!write_checked(&r->CR1, TIM_CR1_ARPE) ||
        !write_checked(&r->PSC, timer_hz() / p->tick_hz - 1U) ||
        !write_checked(&r->ARR, p->period_ticks - 1U) ||
        !write_checked(&r->CCMR1, PWM_MODE) ||
        !write_checked(&r->CCR1, p->duty_ticks))
        return NX_ERROR_IO;
    result = epoch(p);
    if (result == NX_SUCCESS)
        p->initialized = true;
    return result;
}
static bool base_matches(const nx_stm32h743_pwm_state_t* p) {
    TIM_TypeDef* r = p->registers;
    return NX_H743_READ(&r->PSC) == timer_hz() / p->tick_hz - 1U &&
           NX_H743_READ(&r->ARR) == p->period_ticks - 1U &&
           NX_H743_READ(&r->CR2) == 0U && NX_H743_READ(&r->SMCR) == 0U &&
           NX_H743_READ(&r->DIER) == 0U &&
           NX_H743_READ(&r->CCMR1) == PWM_MODE && NX_H743_READ(&r->CCMR2) == 0U;
}
static nx_result_t set(void* context, uint32_t period, uint32_t duty) {
    nx_stm32h743_pwm_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || period == 0U || period > 65535U || duty > period)
        return NX_ERROR_INVALID;
    if (!p->initialized || p->fault || !clock_ready(p) ||
        period != p->period_ticks || !base_matches(p))
        return NX_ERROR_STATE;
    /* PWM1 with CCR=0 is constant low. CCR=period exceeds ARR and is high.
     * Both fit16bits, so endpoint transitions preserve normal CCR preload. */
    if (!write_checked(&p->registers->CCR1, duty))
        return NX_ERROR_IO;
    p->duty_ticks = duty;
    return NX_SUCCESS;
}
static nx_result_t stop(void* context) {
    nx_stm32h743_pwm_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !p->initialized)
        return NX_ERROR_INVALID;
    if (!clock_ready(p))
        return NX_ERROR_STATE;
    TIM_TypeDef* r = p->registers;
    if (!write_checked(&r->CCER, 0U) || !write_checked(&r->CR1, TIM_CR1_ARPE) ||
        !write_checked(&r->CR2, 0U)) {
        p->fault = true;
        return NX_ERROR_IO;
    }
    nx_result_t result = idle_level(p);
    if (result != NX_SUCCESS) {
        p->fault = true;
        return result;
    }
    p->running = false;
    return NX_SUCCESS;
}
static nx_result_t start(void* context) {
    nx_stm32h743_pwm_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !p->initialized || !p->idle->initialized || p->fault)
        return NX_ERROR_STATE;
    if (!clock_ready(p) || !base_matches(p))
        return NX_ERROR_STATE;
    if (p->running)
        return NX_SUCCESS;
    if (NX_H743_READ(&p->registers->CCER) != 0U ||
        NX_H743_READ(&p->registers->CR1) != TIM_CR1_ARPE)
        return NX_ERROR_STATE;
    nx_result_t result = epoch(p);
    if (result == NX_SUCCESS)
        result = nx_stm32h743_pin_alternate(p->idle->registers, p->pin, 2U);
    if (result == NX_SUCCESS &&
        (!write_checked(&p->registers->CCER, TIM_CCER_CC1E) ||
         !write_checked(&p->registers->CR1, TIM_CR1_ARPE | TIM_CR1_CEN)))
        result = NX_ERROR_IO;
    if (result != NX_SUCCESS) {
        p->fault = true;
        (void)stop(p);
        return result;
    }
    p->running = true;
    return NX_SUCCESS;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static nx_result_t state(const void* context, nx_pwm_state_t* out) {
    const nx_stm32h743_pwm_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !p->initialized || out == NULL ||
        (uintptr_t)out > UINTPTR_MAX - sizeof(*out) ||
        overlap(out, sizeof(*out), p, sizeof(*p)) ||
        overlap(out, sizeof(*out), p->registers, sizeof(*p->registers)) ||
        overlap(out, sizeof(*out), p->idle, sizeof(*p->idle)) ||
        overlap(out, sizeof(*out), p->idle->registers,
                sizeof(*p->idle->registers)) ||
        overlap(out, sizeof(*out), (const void*)p->reset, sizeof(*p->reset)))
        return NX_ERROR_INVALID;
    *out = (nx_pwm_state_t){p->period_ticks, p->duty_ticks, p->tick_hz,
                            p->running};
    return NX_SUCCESS;
}
nx_result_t nx_stm32h743_pwm_close(nx_stm32h743_pwm_state_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (!p->retained)
        return NX_SUCCESS;
    if (!clock_ready(p))
        return NX_ERROR_STATE;
    NX_H743_WRITE(p->reset, NX_H743_READ(p->reset) | p->reset_mask);
    nx_arch_dsb();
    if ((NX_H743_READ(p->reset) & p->reset_mask) == 0U)
        return NX_ERROR_IO;
    NX_H743_WRITE(p->reset, NX_H743_READ(p->reset) & ~p->reset_mask);
    nx_arch_dsb();
    if ((NX_H743_READ(p->reset) & p->reset_mask) != 0U || !cold(p))
        return NX_ERROR_IO;
    if (p->idle->initialized && idle_level(p) != NX_SUCCESS)
        return NX_ERROR_IO;
    nx_result_t result = nx_stm32h743_gpio_stop(p->idle, p->inactive);
    if (result != NX_SUCCESS)
        return result;
    p->initialized = false;
    p->running = false;
    p->retained = false;
    p->fault = false;
    return NX_SUCCESS;
}
const nx_pwm_ops_t nx_stm32h743_pwm_ops = {set, start, stop, state};
