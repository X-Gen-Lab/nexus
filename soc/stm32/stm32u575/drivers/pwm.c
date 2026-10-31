/**
 * \file            pwm.c
 * \brief           U575 exclusive fixed-base CH1 PWM with safe GPIO stop
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_cold.h"
#include "stm32u575_provider.h"
#include "stm32u5xx_ll_tim.h"
#ifndef NX_U575_PWM_SET_PRESCALER
#define NX_U575_PWM_SET_PRESCALER(timer, value)                                \
    LL_TIM_SetPrescaler((timer), (value))
#endif
#ifndef NX_U575_PWM_SET_AUTORELOAD
#define NX_U575_PWM_SET_AUTORELOAD(timer, value)                               \
    LL_TIM_SetAutoReload((timer), (value))
#endif
#ifndef NX_U575_PWM_SET_COMPARE_CH1
#define NX_U575_PWM_SET_COMPARE_CH1(timer, value)                              \
    LL_TIM_OC_SetCompareCH1((timer), (value))
#endif
#define PWM_MODE                                                               \
    ((uint32_t)(TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1PE))
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_stm32u575_pwm_state_t* p) {
    return p != NULL && p->registers != NULL && p->idle != NULL &&
           p->idle->registers != NULL && p->reset != NULL && p->pin < 16U &&
           p->idle->mask == (1U << p->pin) && p->idle->output &&
           (p->inactive & ~p->idle->mask) == 0U && p->period_ticks != 0U &&
           p->period_ticks <= 65535U && p->duty_ticks <= p->period_ticks &&
           p->tick_hz != 0U && 4000000U % p->tick_hz == 0U &&
           4000000U / p->tick_hz <= 65536U && p->poll_limit != 0U;
}
static bool clock_ready(const nx_stm32u575_pwm_state_t* p) {
    RCC_TypeDef* r = g_nx_stm32u575_system.rcc;
    if (r == NULL || p->reset != &r->APB1RSTR1 ||
        !g_nx_stm32u575_system.started ||
        g_nx_stm32u575_system.core_hz != 4000000U)
        return false;
    if (p->reset_mask == RCC_APB1RSTR1_TIM3RST)
        return (NX_U575_READ(&r->APB1ENR1) & RCC_APB1ENR1_TIM3EN) != 0U;
    return false;
}
static bool write_checked(volatile uint32_t* reg, uint32_t value) {
    NX_U575_WRITE(reg, value);
    nx_arch_dsb();
    return NX_U575_READ(reg) == value;
}
/** \brief Keep the official timer store inside the exact readback protocol. */
static bool prescaler_checked(TIM_TypeDef* timer, uint32_t value) {
    NX_U575_PWM_SET_PRESCALER(timer, value);
    nx_arch_dsb();
    return NX_U575_READ(&timer->PSC) == value;
}
static bool autoreload_checked(TIM_TypeDef* timer, uint32_t value) {
    NX_U575_PWM_SET_AUTORELOAD(timer, value);
    nx_arch_dsb();
    return NX_U575_READ(&timer->ARR) == value;
}
static bool compare_checked(TIM_TypeDef* timer, uint32_t value) {
    NX_U575_PWM_SET_COMPARE_CH1(timer, value);
    nx_arch_dsb();
    return NX_U575_READ(&timer->CCR1) == value;
}
static bool cold(const nx_stm32u575_pwm_state_t* p) {
    TIM_TypeDef* r = p->registers;
    return NX_U575_READ(&r->CR1) == 0U && NX_U575_READ(&r->CR2) == 0U &&
           NX_U575_READ(&r->SMCR) == 0U && NX_U575_READ(&r->DIER) == 0U &&
           NX_U575_READ(&r->CCER) == 0U && NX_U575_READ(&r->CCMR1) == 0U &&
           NX_U575_READ(&r->CCMR2) == 0U &&
           (NX_U575_READ(p->reset) & p->reset_mask) == 0U;
}
/** \brief Hold TRGO on disabled CEN while UG loads the preload registers. */
static nx_result_t epoch(nx_stm32u575_pwm_state_t* p) {
    TIM_TypeDef* r = p->registers;
    if (!write_checked(&r->CR2, TIM_CR2_MMS_0) || !write_checked(&r->SR, 0U))
        return NX_ERROR_IO;
    NX_U575_WRITE(&r->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    uint32_t remaining = p->poll_limit;
    do {
        NX_U575_PWM_POLL(p);
        if ((NX_U575_READ(&r->SR) & TIM_SR_UIF) != 0U)
            break;
    } while (--remaining != 0U);
    if ((NX_U575_READ(&r->SR) & TIM_SR_UIF) == 0U)
        return NX_ERROR_TIMEOUT;
    if (!write_checked(&r->SR, 0U) || !write_checked(&r->CR2, 0U) ||
        !write_checked(&r->CNT, 0U))
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
/** \brief Latch the Board idle before exposing the GPIO output mode. */
static nx_result_t idle_level(nx_stm32u575_pwm_state_t* p) {
    GPIO_TypeDef* gpio = p->idle->registers;
    uint32_t mask = p->idle->mask;
    NX_U575_WRITE(&gpio->BSRR, p->inactive | ((mask & ~p->inactive) << 16U));
    nx_arch_dsb();
    if ((NX_U575_READ(&gpio->ODR) & mask) != p->inactive ||
        (NX_U575_READ(&gpio->OTYPER) & mask) != 0U)
        return NX_ERROR_IO;
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    uint32_t shift = (uint32_t)p->pin * 2U;
    uint32_t mode =
        (NX_U575_READ(&gpio->MODER) & ~(3U << shift)) | (1U << shift);
    nx_result_t result =
        write_checked(&gpio->MODER, mode) ? NX_SUCCESS : NX_ERROR_IO;
    nx_arch_irq_restore(previous);
    return result;
}
nx_result_t nx_stm32u575_pwm_initialize(nx_stm32u575_pwm_state_t* p) {
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
    nx_result_t result = nx_stm32u575_gpio_initialize(p->idle, p->inactive);
    if (result != NX_SUCCESS)
        return result;
    TIM_TypeDef* r = p->registers;
    if (!write_checked(&r->CR1, TIM_CR1_ARPE) ||
        !prescaler_checked(r, 4000000U / p->tick_hz - 1U) ||
        !autoreload_checked(r, p->period_ticks - 1U) ||
        !write_checked(&r->CCMR1, PWM_MODE) ||
        !compare_checked(r, p->duty_ticks))
        return NX_ERROR_IO;
    result = epoch(p);
    if (result == NX_SUCCESS)
        p->initialized = true;
    return result;
}
static bool base_matches(const nx_stm32u575_pwm_state_t* p) {
    TIM_TypeDef* r = p->registers;
    return LL_TIM_GetPrescaler(r) == 4000000U / p->tick_hz - 1U &&
           NX_U575_READ(&r->ARR) == p->period_ticks - 1U &&
           NX_U575_READ(&r->CR2) == 0U && NX_U575_READ(&r->SMCR) == 0U &&
           NX_U575_READ(&r->DIER) == 0U &&
           NX_U575_READ(&r->CCMR1) == PWM_MODE && NX_U575_READ(&r->CCMR2) == 0U;
}
static nx_result_t set(void* context, uint32_t period, uint32_t duty) {
    nx_stm32u575_pwm_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || period == 0U || period > 65535U || duty > period)
        return NX_ERROR_INVALID;
    if (!p->initialized || p->fault || !clock_ready(p) ||
        period != p->period_ticks || !base_matches(p))
        return NX_ERROR_STATE;
    /* PWM1 with CCR=0 is constant low. CCR=period exceeds ARR and is high.
     * Both fit16bits, so endpoint transitions preserve normal CCR preload. */
    if (!compare_checked(p->registers, duty))
        return NX_ERROR_IO;
    p->duty_ticks = duty;
    return NX_SUCCESS;
}
static nx_result_t stop(void* context) {
    nx_stm32u575_pwm_state_t* p = context;
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
    nx_stm32u575_pwm_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !p->initialized || !p->idle->initialized || p->fault)
        return NX_ERROR_STATE;
    if (!clock_ready(p) || !base_matches(p))
        return NX_ERROR_STATE;
    if (p->running)
        return NX_SUCCESS;
    if (NX_U575_READ(&p->registers->CCER) != 0U ||
        NX_U575_READ(&p->registers->CR1) != TIM_CR1_ARPE)
        return NX_ERROR_STATE;
    nx_result_t result = epoch(p);
    if (result == NX_SUCCESS)
        result = nx_stm32u575_pin_alternate(p->idle->registers, p->pin, 2U);
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
    const nx_stm32u575_pwm_state_t* p = context;
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
nx_result_t nx_stm32u575_pwm_close(nx_stm32u575_pwm_state_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (!p->retained)
        return NX_SUCCESS;
    if (!clock_ready(p))
        return NX_ERROR_STATE;
    NX_U575_WRITE(p->reset, NX_U575_READ(p->reset) | p->reset_mask);
    nx_arch_dsb();
    if ((NX_U575_READ(p->reset) & p->reset_mask) == 0U)
        return NX_ERROR_IO;
    NX_U575_WRITE(p->reset, NX_U575_READ(p->reset) & ~p->reset_mask);
    nx_arch_dsb();
    if ((NX_U575_READ(p->reset) & p->reset_mask) != 0U || !cold(p))
        return NX_ERROR_IO;
    if (p->idle->initialized && idle_level(p) != NX_SUCCESS)
        return NX_ERROR_IO;
    nx_result_t result = nx_stm32u575_gpio_stop(p->idle, p->inactive);
    if (result != NX_SUCCESS)
        return result;
    p->initialized = false;
    p->running = false;
    p->retained = false;
    p->fault = false;
    return NX_SUCCESS;
}
const nx_pwm_ops_t nx_stm32u575_pwm_ops = {set, start, stop, state};

nx_result_t nx_stm32u575_pwm_prepare(nx_stm32u575_pwm_state_t* port,
                                     const nx_stm32u575_pwm_config_t* config) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || config->idle == NULL ||
        config->gpio == NULL || config->pin > 15U)
        return NX_ERROR_INVALID;
    if (port->retained || port->initialized || config->idle->retained ||
        config->idle->initialized)
        return NX_ERROR_BUSY;
    config->idle->registers = config->gpio;
    config->idle->mask = 1U << config->pin;
    config->idle->output = true;
    port->registers = TIM3;
    port->idle = config->idle;
    port->reset = &RCC->APB1RSTR1;
    port->reset_mask = RCC_APB1RSTR1_TIM3RST;
    port->pin = config->pin;
    port->inactive = config->inactive;
    port->period_ticks = config->period_ticks;
    port->duty_ticks = config->duty_ticks;
    port->tick_hz = config->tick_hz;
    port->poll_limit = 1000000U;
    return nx_stm32u575_pwm_initialize(port);
}
