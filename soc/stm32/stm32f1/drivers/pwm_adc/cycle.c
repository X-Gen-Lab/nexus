/**
 * \file            cycle.c
 * \brief           F103RB finite TIM1 CH1/CH1N and CC4-triggered injected ADC1
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "dispatch_checks.h"
#include "drivers/adc/regular_calibration.h"
#include "instance.h"
#include "system/platform.h"

#if !defined(STM32F103xB)
#error "Finite injected window requires the reviewed STM32F103xB"
#endif

#ifndef NX_F1_CYCLE_IRQ
#define NX_F1_CYCLE_IRQ(operation, ...) NVIC_##operation(__VA_ARGS__)
#endif

#ifndef NX_F1_CYCLE_RAM_BEGIN
#define NX_F1_CYCLE_RAM_BEGIN (uintptr_t) SRAM_BASE
#define NX_F1_CYCLE_RAM_END   (NX_F1_CYCLE_RAM_BEGIN + UINT32_C(20480))
#endif

#define ADEN       (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN)
#define ADC_IDLE   (ADC_CR2_JEXTSEL_0 | ADC_CR2_ADON)
#define ADC_ARMED  (ADC_IDLE | ADC_CR2_JEXTTRIG)
#define UPDATE     TIM1_UP_IRQn
#define TIMER_DONE (TIM_SR_UIF | TIM_SR_CC4IF)
#define ADC_DONE   (ADC_SR_JSTRT | ADC_SR_JEOC)

enum { WINDOW_WAITING, WINDOW_CAPTURED, WINDOW_FAULT };

enum { ADC_GATE = 1U, ADC_CONFIG = 2U, UPDATE_PRIORITY = 4U, LIVE = 8U };

static bool vector_idle(IRQn_Type irq) {
    return NX_F1_CYCLE_IRQ(GetEnableIRQ, irq) == 0U &&
           NX_F1_CYCLE_IRQ(GetPendingIRQ, irq) == 0U &&
           NX_F1_CYCLE_IRQ(GetActive, irq) == 0U;
}

static bool adc_ready(void) {
    return (RCC->APB2ENR & ADEN) == RCC_APB2ENR_ADC1EN &&
           (RCC->CFGR & RCC_CFGR_ADCPRE) == 0U;
}

static bool converter_configured(const nx_stm32f1_cycle_t* state) {
    return (ADC1->CR1 | ADC1->JOFR1) == 0U &&
           ADC1->JSQR == (1U << ADC_JSQR_JSQ4_Pos) &&
           ADC1->SMPR2 ==
               ((uint32_t)state->config->sample << ADC_SMPR2_SMP1_Pos);
}

static bool adc_reset(void) {
    const uint32_t bit = RCC_APB2RSTR_ADC1RST;
    const bool set = nx_stm32f1_write_masked(&RCC->APB2RSTR, bit, bit);
    return nx_stm32f1_write_masked(&RCC->APB2RSTR, bit, 0U) && set &&
           (ADC1->CR1 | ADC1->CR2 | ADC1->SR) == 0U;
}

nx_result_t nx_stm32f1_cycle_prepare(nx_stm32f1_cycle_t* state,
                                     const nx_stm32f1_cycle_config_t* config) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL ||
        config->sample >= sizeof(nx_stm32f1_adc_regular_ticks) ||
        config->phase < 128U ||
        config->pwm.period_ticks <=
            config->phase + 8U +
                2U * nx_stm32f1_adc_regular_ticks[config->sample] ||
        config->pwm.deadtime_ticks >= config->phase)
        return NX_ERROR_INVALID;
    if (state->config != NULL || (RCC->APB2ENR & ADEN) != 0U ||
        (RCC->APB2RSTR & RCC_APB2RSTR_ADC1RST) != 0U ||
        !nx_stm32f1_system_live() || (RCC->CFGR & RCC_CFGR_ADCPRE) != 0U ||
        !vector_idle(ADC1_2_IRQn) || !vector_idle(UPDATE) ||
        !vector_idle(TIM1_CC_IRQn))
        return NX_ERROR_BUSY;

    state->config = config;
    nx_result_t result = nx_stm32f1_pads_capture(&state->analog, GPIOA, 2U);
    if (result == NX_SUCCESS)
        result =
            nx_stm32f1_pads_apply(&state->analog, 0U, 0U, state->analog.latch);
    if (result != NX_SUCCESS)
        return result;
    state->effects |= ADC_GATE;
    if (!nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_ADC1EN,
                                 RCC_APB2ENR_ADC1EN))
        return NX_ERROR_IO;
    state->effects |= ADC_CONFIG;
    if (!adc_reset())
        return NX_ERROR_IO;

    /* RM0008 §11.3.9 and Table 68: a one-rank JSQ4 takes PA1/IN1
     * from the fixed, interior TIM1_CC4 compare. No PA0 or auto-injection. */
    WRITE_REG(ADC1->JSQR, 1U << ADC_JSQR_JSQ4_Pos);
    WRITE_REG(ADC1->SMPR2, (uint32_t)config->sample << ADC_SMPR2_SMP1_Pos);
    WRITE_REG(ADC1->CR2, ADC_IDLE & ~(uint32_t)ADC_CR2_ADON);
    nx_arch_dsb();
    if (!converter_configured(state))
        return NX_ERROR_IO;
    result = nx_stm32f1_adc_calibrate(ADC1, ADC_IDLE);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32f1_advanced_pwm_prepare(&state->pwm, &config->pwm);
    if (result != NX_SUCCESS)
        return result;

    state->update_priority = (uint8_t)NX_F1_CYCLE_IRQ(GetPriority, UPDATE);
    state->effects |= UPDATE_PRIORITY;
    NX_F1_CYCLE_IRQ(SetPriority, UPDATE, config->pwm.priority);
    if (NX_F1_CYCLE_IRQ(GetPriority, UPDATE) != config->pwm.priority)
        return NX_ERROR_IO;
    state->effects |= LIVE;
    return NX_SUCCESS;
}

static bool independent(const nx_stm32f1_cycle_t* state, const void* p,
                        size_t size, size_t align) {
    return nx_io_dispatch_span(p, size, align) &&
           !nx_io_dispatch_overlap(p, size, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(p, size, state->config,
                                   sizeof(*state->config)) &&
           !nx_io_dispatch_overlap(p, size, (void*)PERIPH_BASE,
                                   UINT32_C(0x20000000)) &&
           !nx_io_dispatch_overlap(p, size, &nx_stm32f1_cycle_ops,
                                   sizeof(nx_stm32f1_cycle_ops));
}

static bool in_region(const void* p, size_t bytes, uintptr_t begin,
                      uintptr_t end) {
    const uintptr_t address = (uintptr_t)p;
    return address >= begin && address < end && bytes <= end - address;
}

static bool writable(const void* p, size_t bytes) {
    return in_region(p, bytes, NX_F1_CYCLE_RAM_BEGIN, NX_F1_CYCLE_RAM_END);
}

static bool readable_duty(const uint32_t* duty) {
    return writable(duty, sizeof(*duty)) ||
           in_region(duty, sizeof(*duty), FLASH_BASE,
                     FLASH_BASE + UINT32_C(131072));
}

/** \brief Remove PWM drive and ADC trigger before releasing any request. */
static bool drain(nx_stm32f1_cycle_t* state, bool complete) {
    const bool clock = (RCC->APB2ENR & RCC_APB2ENR_TIM1EN) != 0U;
    const bool update_off =
        clock && nx_stm32f1_write_masked(&TIM1->DIER, TIM_DIER_UIE, 0U);
    const bool parked =
        nx_stm32f1_advanced_pwm_ops.stop(&state->pwm) == NX_SUCCESS;
    NX_F1_CYCLE_IRQ(DisableIRQ, UPDATE);
    if (NX_F1_CYCLE_IRQ(GetActive, UPDATE) == 0U)
        NX_F1_CYCLE_IRQ(ClearPendingIRQ, UPDATE);
    nx_arch_dsb();
    bool cleared = false;
    if (adc_ready()) {
        if (!complete)
            cleared = nx_stm32f1_adc_power_down(ADC1);
        else {
            WRITE_REG(ADC1->CR2, ADC_IDLE);
            nx_arch_dsb();
            cleared = ADC1->CR2 == ADC_IDLE;
        }
    }
    if (!update_off || !parked || !vector_idle(UPDATE) || !cleared)
        return false;
    NX_STM32F1_WRITE(&TIM1->CCR4, 0U);
    WRITE_REG(ADC1->SR, 0U);
    nx_arch_dsb();
    return nx_stm32f1_write_masked(&TIM1->CR1, TIM_CR1_OPM, 0U) &&
           TIM1->CCR4 == 0U && ADC1->SR == 0U;
}

static nx_result_t finish(nx_stm32f1_cycle_t* state) {
    nx_pwm_adc_request_t* r = state->request;
    bool complete = state->terminal == NX_SUCCESS;
    const uint32_t raw = complete && adc_ready() ? ADC1->JDR1 : 0U;
    if (complete && raw > 0x0fffU) {
        complete = false;
        state->terminal = NX_ERROR_IO;
    }
    const bool quiet = drain(state, complete);
    if (!quiet) {
        state->terminal = NX_ERROR_IO;
        state->fault = true;
        (void)nx_request_transition(&r->base, NX_REQUEST_QUARANTINED);
        return NX_ERROR_IO;
    }
    if (complete && state->pwm.fault) {
        state->terminal = NX_ERROR_IO;
        complete = false;
    }
    state->fault = state->fault || !complete;
    if (complete)
        *r->samples = (uint16_t)raw;
    const nx_result_t result = state->terminal;
    state->request = NULL;
    nx_request_settle(&r->base, result, complete ? 1U : 0U);
    return NX_SUCCESS;
}

static nx_result_t submit(void* context, nx_pwm_adc_request_t* r) {
    nx_stm32f1_cycle_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if ((state->effects & LIVE) == 0U || state->stopped || state->fault)
        return NX_ERROR_STATE;
    if (state->request != NULL)
        return NX_ERROR_BUSY;
    if (!writable(r, sizeof(*r)) ||
        !independent(state, r, sizeof(*r), _Alignof(nx_pwm_adc_request_t)) ||
        r->sample_count != 1U || r->frame.channel_count != 1U ||
        r->frame.trigger_ticks != state->config->phase ||
        !writable(r->samples, sizeof(*r->samples)) ||
        !readable_duty(r->frame.duty_ticks) ||
        !independent(state, r->samples, sizeof(*r->samples),
                     _Alignof(uint16_t)) ||
        !independent(state, r->frame.duty_ticks, sizeof(uint32_t),
                     _Alignof(uint32_t)) ||
        nx_io_dispatch_overlap(r, sizeof(*r), r->samples,
                               sizeof(*r->samples)) ||
        nx_io_dispatch_overlap(r, sizeof(*r), r->frame.duty_ticks,
                               sizeof(uint32_t)) ||
        nx_io_dispatch_overlap(r->samples, sizeof(*r->samples),
                               r->frame.duty_ticks, sizeof(uint32_t)))
        return NX_ERROR_INVALID;
    const uint32_t max =
        state->config->phase - state->config->pwm.deadtime_ticks;
    if (*r->frame.duty_ticks >= max)
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_system_live() || !adc_ready() || ADC1->CR2 != ADC_IDLE ||
        !converter_configured(state) || !vector_idle(UPDATE) ||
        !vector_idle(ADC1_2_IRQn) || !vector_idle(TIM1_CC_IRQn))
        return NX_ERROR_IO;
    if (r->base.deadline == NX_DEADLINE_NEVER ||
        nx_deadline_expired(r->base.deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    nx_result_t result = nx_request_admit(&r->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;
    state->request = r;
    state->terminal = NX_ERROR_BUSY;
    state->completed = WINDOW_WAITING;
    WRITE_REG(ADC1->SR, 0U);
    WRITE_REG(ADC1->CR2, ADC_ARMED);
    NX_F1_CYCLE_IRQ(EnableIRQ, UPDATE);
    nx_arch_dsb();
    if (ADC1->SR != 0U || ADC1->CR2 != ADC_ARMED ||
        NX_F1_CYCLE_IRQ(GetEnableIRQ, UPDATE) == 0U ||
        nx_stm32f1_advanced_pwm_one_pulse(&state->pwm, *r->frame.duty_ticks,
                                          state->config->phase) != NX_SUCCESS ||
        (TIM1->CR1 & TIM_CR1_CEN) == 0U || TIM1->CNT >= state->config->phase) {
        state->terminal = NX_ERROR_IO;
        (void)finish(state);
    }
    return NX_SUCCESS;
}

static nx_result_t service(void* context) {
    nx_stm32f1_cycle_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state->request == NULL)
        return NX_SUCCESS;
    if (state->terminal == NX_ERROR_BUSY) {
        const uint32_t timer_flags = TIM1->SR;
        if (!nx_stm32f1_system_live() || !adc_ready() ||
            ADC1->CR2 != ADC_ARMED || state->pwm.fault ||
            (timer_flags & TIM_SR_BIF) != 0U)
            state->terminal = NX_ERROR_IO;
        else if (nx_deadline_expired(state->request->base.deadline,
                                     nx_time_now_us()))
            state->terminal = NX_ERROR_TIMEOUT;
        else if (state->completed == WINDOW_WAITING)
            return NX_ERROR_BUSY;
        else
            state->terminal =
                state->completed == WINDOW_CAPTURED ? NX_SUCCESS : NX_ERROR_IO;
    }
    return finish(state);
}

static nx_result_t cancel(void* context, nx_pwm_adc_request_t* request) {
    nx_stm32f1_cycle_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state->request != request || request == NULL)
        return NX_ERROR_STATE;
    if (state->terminal == NX_ERROR_BUSY)
        state->terminal = state->pwm.fault || (TIM1->SR & TIM_SR_BIF) != 0U
                              ? NX_ERROR_IO
                              : NX_ERROR_CANCELLED;
    return NX_SUCCESS;
}

static nx_result_t stop(void* context) {
    nx_stm32f1_cycle_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    state->stopped = true;
    if (state->request != NULL) {
        (void)cancel(state, state->request);
        return service(state);
    }
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_cycle_close(nx_stm32f1_cycle_t* state) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL)
        return NX_ERROR_INVALID;
    nx_result_t result = stop(state);
    if (result != NX_SUCCESS)
        return result;
    if ((state->effects & UPDATE_PRIORITY) != 0U) {
        if (!vector_idle(UPDATE))
            return NX_ERROR_IO;
        NX_F1_CYCLE_IRQ(SetPriority, UPDATE, state->update_priority);
        if (NX_F1_CYCLE_IRQ(GetPriority, UPDATE) != state->update_priority)
            return NX_ERROR_IO;
        state->effects &= (uint8_t)~UPDATE_PRIORITY;
    }
    result = nx_stm32f1_advanced_pwm_close(&state->pwm);
    if (result != NX_SUCCESS)
        return result;
    if ((state->effects & ADC_CONFIG) != 0U) {
        if (!adc_ready() || !nx_stm32f1_adc_power_down(ADC1) || !adc_reset())
            return NX_ERROR_IO;
        state->effects &= (uint8_t)~ADC_CONFIG;
    }
    if ((state->effects & ADC_GATE) != 0U &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_ADC1EN, 0U))
        return NX_ERROR_IO;
    state->effects &= (uint8_t)~ADC_GATE;
    result = nx_stm32f1_pads_restore(&state->analog);
    if (result != NX_SUCCESS)
        return result;
    *state = (nx_stm32f1_cycle_t){0};
    return NX_SUCCESS;
}

static nx_result_t status(const void* context, nx_pwm_adc_state_t* out) {
    const nx_stm32f1_cycle_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!writable(out, sizeof(*out)) ||
        !independent(state, out, sizeof(*out), _Alignof(nx_pwm_adc_state_t)))
        return NX_ERROR_INVALID;
    if ((state->effects & LIVE) == 0U)
        return NX_ERROR_STATE;
    const nx_pwm_adc_request_t* r = state->request;
    if (r != NULL &&
        (nx_io_dispatch_overlap(out, sizeof(*out), r, sizeof(*r)) ||
         nx_io_dispatch_overlap(out, sizeof(*out), r->samples,
                                sizeof(*r->samples)) ||
         nx_io_dispatch_overlap(out, sizeof(*out), r->frame.duty_ticks,
                                sizeof(uint32_t))))
        return NX_ERROR_INVALID;
    *out = (nx_pwm_adc_state_t){
        .format = {.sample_stride = 2U,
                   .scan_stride = 2U,
                   .trigger_identity = 1U,
                   .channel_count = 1U,
                   .storage_bytes = 2U,
                   .significant_bits = 12U,
                   .encoding = NX_ADC_UNSIGNED,
                   .byte_order = NX_ADC_LITTLE_ENDIAN,
                   .group = NX_ADC_INJECTED,
                   .trigger = NX_ADC_TRIGGER_TIMER},
        .tick_hz = state->config->pwm.tick_hz,
        .period_ticks = state->config->pwm.period_ticks,
        .trigger_ticks = state->config->phase,
        .deadtime_ticks = state->config->pwm.deadtime_ticks,
        .deadtime_hz = state->config->pwm.tick_hz,
        .pwm_channels = 1U,
        .max_samples = 1U,
        .running = (TIM1->CR1 & TIM_CR1_CEN) != 0U,
        .fault = state->fault || state->pwm.fault,
        .quarantined =
            state->request != NULL &&
            nx_request_state(&state->request->base) == NX_REQUEST_QUARANTINED,
    };
    return NX_SUCCESS;
}

void nx_stm32f1_cycle_break_irq(nx_stm32f1_cycle_t* state) {
    nx_stm32f1_advanced_pwm_irq(&state->pwm);
}

void nx_stm32f1_cycle_update_irq(nx_stm32f1_cycle_t* state) {
    if ((state->effects & LIVE) != 0U && (TIM1->DIER & TIM_DIER_UIE) != 0U &&
        (TIM1->SR & TIM_SR_UIF) != 0U) {
        NX_STM32F1_WRITE(&TIM1->BDTR, TIM1->BDTR & ~(uint32_t)TIM_BDTR_MOE);
        NX_STM32F1_WRITE(&TIM1->DIER, TIM1->DIER & ~(uint32_t)TIM_DIER_UIE);
        nx_arch_dsb();
        /* Snapshot only once; a later JEOC must not repair a short window. */
        const bool complete = (TIM1->CR1 & TIM_CR1_CEN) == 0U &&
                              (TIM1->SR & TIMER_DONE) == TIMER_DONE &&
                              adc_ready() &&
                              (ADC1->SR & ADC_DONE) == ADC_DONE &&
                              (TIM1->BDTR & TIM_BDTR_MOE) == 0U &&
                              (TIM1->DIER & TIM_DIER_UIE) == 0U;
        if (state->completed == WINDOW_WAITING)
            state->completed = complete ? WINDOW_CAPTURED : WINDOW_FAULT;
    }
}

const nx_pwm_adc_ops_t nx_stm32f1_cycle_ops = {submit, cancel, service, stop,
                                               status};
