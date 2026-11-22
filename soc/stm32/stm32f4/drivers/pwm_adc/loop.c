/**
 * \file            loop.c
 * \brief           Anchored injected-ADC IRQ control on one TIM8 owner
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/pwm_adc/loop.h"
#include "drivers/timer/stm32f407_timer_output.h"
#include "stm32f4xx_ll_adc.h"

static uint32_t fault(const nx_stm32_pwm_adc_loop_state_t* s) {
    return __atomic_load_n(&s->fault, __ATOMIC_ACQUIRE);
}

static uint32_t conversion_us(const nx_stm32_pwm_adc_loop_config_t* c) {
    const uint16_t cycles[8] = {3U, 15U, 28U, 56U, 84U, 112U, 144U, 480U};
    uint64_t total = 0U;
    for (size_t i = 0U; i < c->analog.channel_count; ++i)
        total += (uint64_t)cycles[c->analog.sample_times[i]] + 12U;
    return (uint32_t)((total * 1000000U + c->analog.clock_hz - 1U) /
                      c->analog.clock_hz);
}

static bool config_valid(const nx_stm32_pwm_adc_loop_config_t* c) {
    if (c == NULL ||
        (uintptr_t)c % _Alignof(nx_stm32_pwm_adc_loop_config_t) != 0U ||
        !nx_stm32_control_adc_valid(&c->analog) ||
        c->analog.channel_count > 4U || c->pwm.controller != 8U ||
        c->pwm.kind != NX_STM32_WAVE_GROUP || c->pwm.channels != 7U ||
        !c->pwm.internal_trigger || c->pwm.master_mode != 7U ||
        c->pwm.sync_mode != 0U || c->pwm.tick_hz != 1000000U ||
        c->pwm.period_ticks < 3U || c->pwm.period_ticks > UINT16_MAX ||
        c->trigger_ticks == 0U || c->max_irq_delay_us == 0U ||
        c->compute_budget_ticks == 0U ||
        c->commit_before_ticks > c->pwm.period_ticks - 2U)
        return false;
    return (uint64_t)c->trigger_ticks + conversion_us(c) + c->max_irq_delay_us +
               c->compute_budget_ticks + 4U <
           c->commit_before_ticks;
}

static bool irq_idle(IRQn_Type irq) {
    return NX_AV_IRQ_ACTIVE(irq) == 0U && NX_AV_IRQ_ENABLED(irq) == 0U &&
           NX_AV_IRQ_PENDING(irq) == 0U;
}

nx_result_t
nx_stm32_pwm_adc_loop_admit(const nx_stm32_pwm_adc_loop_state_t* s,
                            const nx_stm32_pwm_adc_loop_config_t* c) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL ||
        (uintptr_t)s % _Alignof(nx_stm32_pwm_adc_loop_state_t) != 0U ||
        !config_valid(c) || nx_stm32_av_overlap(s, sizeof(*s), c, sizeof(*c)))
        return NX_ERROR_INVALID;
    if (s->owned || s->active != NULL)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32_waveform_admit(&s->pwm, &c->pwm);
    if (result == NX_SUCCESS)
        result = nx_stm32_control_adc_admit(&s->analog, &c->analog);
    return result != NX_SUCCESS ? result
           : irq_idle(ADC_IRQn) ? NX_SUCCESS
                                : NX_ERROR_BUSY;
}

/** \brief Keep channel/sample programming shared by all selected injected
 * ranks.
 */
static bool sequence(const nx_stm32_control_adc_config_t* c) {
    uint32_t smpr1 = 0U, smpr2 = 0U;
    uint32_t jsqr = (uint32_t)(c->channel_count - 1U) << ADC_JSQR_JL_Pos;
    for (size_t i = 0U; i < c->channel_count; ++i) {
        uint8_t channel = c->channels[i];
        if (channel < 10U)
            smpr2 |= (uint32_t)c->sample_times[i] << (channel * 3U);
        else
            smpr1 |= (uint32_t)c->sample_times[i] << ((channel - 10U) * 3U);
        jsqr |= (uint32_t)channel << ((4U - c->channel_count + i) * 5U);
    }
    bool ok = nx_stm32_av_write(&c->adc->SMPR1, smpr1, UINT32_MAX);
    ok = nx_stm32_av_write(&c->adc->SMPR2, smpr2, UINT32_MAX) && ok;
    return nx_stm32_av_write(&c->adc->JSQR, jsqr, UINT32_MAX) && ok;
}

nx_result_t
nx_stm32_pwm_adc_loop_prepare(nx_stm32_pwm_adc_loop_state_t* s,
                              const nx_stm32_pwm_adc_loop_config_t* c) {
    nx_result_t result = nx_stm32_pwm_adc_loop_admit(s, c);
    if (result != NX_SUCCESS)
        return result;
    if (!g_nx_stm32_system.started ||
        !nx_stm32_clock_profile_matches(g_nx_stm32_system.rcc, NULL))
        return NX_ERROR_STATE;
    s->config = c;
    s->owned = true;
    s->irq_priority = NX_AV_IRQ_PRIORITY_READ(ADC_IRQn);
    s->sample_delay_us = conversion_us(c);
    result = nx_stm32_control_adc_prepare(&s->analog, &c->analog);
    if (result != NX_SUCCESS)
        return result;
    if (!sequence(&c->analog))
        return NX_ERROR_IO;
    result = nx_stm32_waveform_prepare(&s->pwm, &c->pwm);
    if (result == NX_SUCCESS)
        s->initialized = true;
    return result;
}

static bool live(nx_stm32_pwm_adc_loop_state_t* s) {
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    bool ok = !s->history_lost && g_nx_stm32_system.started &&
              nx_stm32_clock_profile_matches(rcc, NULL) &&
              (NX_AV_READ(&rcc->APB2ENR) &
               (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_TIM8EN)) ==
                  (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_TIM8EN) &&
              (NX_AV_READ(&rcc->APB2RSTR) &
               (RCC_APB2RSTR_ADCRST | RCC_APB2RSTR_TIM8RST)) == 0U;
    if (s->active != NULL)
        ok = ok && nx_stm32_av_timebase() &&
             g_nx_stm32_system.timer_error == NX_SUCCESS &&
             s->time_epoch == nx_stm32_av_epoch();
    if (!ok && s->active != NULL)
        s->history_lost = true;
    return ok;
}

/** \brief Hardware suppression is bounded and never releases callback storage.
 */
static bool suppress(nx_stm32_pwm_adc_loop_state_t* s) {
    const nx_stm32_pwm_adc_loop_config_t* c = s->config;
    bool quiet = nx_stm32_timer_output_stop(c->pwm.registers, true, UINT32_MAX,
                                            UINT32_MAX);
    quiet = nx_stm32_av_bits(&c->analog.adc->CR1,
                             ADC_CR1_JEOCIE | ADC_CR1_OVRIE, 0U) &&
            quiet;
    return nx_stm32_av_bits(&c->analog.adc->CR2, ADC_CR2_JEXTEN | ADC_CR2_ADON,
                            0U) &&
           quiet;
}

static void trip(nx_stm32_pwm_adc_loop_state_t* s, nx_result_t result) {
    uint32_t empty = NX_SUCCESS;
    (void)__atomic_compare_exchange_n(&s->fault, &empty, (uint32_t)result,
                                      false, __ATOMIC_RELEASE,
                                      __ATOMIC_RELAXED);
    (void)suppress(s);
}

static bool disjoint(const nx_stm32_pwm_adc_loop_state_t* s, const void* data,
                     size_t bytes) {
    const nx_stm32_pwm_adc_loop_config_t* c = s->config;
    if (!nx_stm32_control_adc_disjoint(&c->analog, data, bytes) ||
        nx_stm32_av_overlap(data, bytes, s, sizeof(*s)) ||
        nx_stm32_av_overlap(data, bytes, c, sizeof(*c)) ||
        nx_stm32_av_overlap(data, bytes, c->pwm.registers,
                            sizeof(*c->pwm.registers)) ||
        nx_stm32_av_overlap(data, bytes, c->pwm.common.pins,
                            c->pwm.common.pin_count *
                                sizeof(*c->pwm.common.pins)))
        return false;
    for (size_t i = 0U; i < c->pwm.common.pin_count; ++i) {
        const nx_stm32_av_pin_t* pin = &c->pwm.common.pins[i];
        if (nx_stm32_av_overlap(data, bytes, pin->registers,
                                sizeof(*pin->registers)) ||
            nx_stm32_av_overlap(data, bytes, pin->snapshot,
                                sizeof(*pin->snapshot)))
            return false;
    }
    return true;
}

static nx_result_t session_valid(nx_stm32_pwm_adc_loop_state_t* s,
                                 nx_pwm_adc_loop_session_t* session) {
    if (session == NULL ||
        (uintptr_t)session % _Alignof(nx_pwm_adc_loop_session_t) != 0U ||
        !disjoint(s, session, sizeof(*session)) || session->samples == NULL ||
        (uintptr_t)session->samples % _Alignof(uint16_t) != 0U ||
        session->sample_count != s->config->analog.channel_count ||
        session->duty_ticks == NULL ||
        (uintptr_t)session->duty_ticks % _Alignof(uint32_t) != 0U ||
        session->duty_count != 3U || session->compute == NULL ||
        session->context == NULL)
        return NX_ERROR_INVALID;
    size_t sample_bytes = session->sample_count * sizeof(uint16_t);
    size_t duty_bytes = session->duty_count * sizeof(uint32_t);
    if (!disjoint(s, session->samples, sample_bytes) ||
        !disjoint(s, session->duty_ticks, duty_bytes) ||
        !disjoint(s, session->context, 1U) ||
        nx_stm32_av_overlap(session, sizeof(*session), session->samples,
                            sample_bytes) ||
        nx_stm32_av_overlap(session, sizeof(*session), session->duty_ticks,
                            duty_bytes) ||
        nx_stm32_av_overlap(session->samples, sample_bytes, session->duty_ticks,
                            duty_bytes) ||
        nx_stm32_av_overlap(session->context, 1U, session, sizeof(*session)) ||
        nx_stm32_av_overlap(session->context, 1U, session->samples,
                            sample_bytes) ||
        nx_stm32_av_overlap(session->context, 1U, session->duty_ticks,
                            duty_bytes))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < 3U; ++i)
        if (session->duty_ticks[i] > s->config->pwm.period_ticks)
            return NX_ERROR_INVALID;
    return NX_SUCCESS;
}

static nx_result_t service(void* context);

static nx_result_t start(void* context, nx_pwm_adc_loop_session_t* session) {
    nx_stm32_pwm_adc_loop_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized || s->stopping || fault(s) != NX_SUCCESS ||
        s->quarantined)
        return NX_ERROR_STATE;
    if (s->active != NULL)
        return NX_ERROR_BUSY;
    nx_result_t result = session_valid(s, session);
    if (result != NX_SUCCESS)
        return result;
    if (nx_request_state(&session->base) != NX_REQUEST_READY || !live(s))
        return NX_ERROR_STATE;
    nx_time_us_t now = NX_AV_NOW();
    if (session->base.deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (now >= session->base.deadline)
        return NX_ERROR_TIMEOUT;
    nx_time_us_t ready;
    result = nx_deadline_finite_after(now, 4U, &ready);
    if (result != NX_SUCCESS)
        return result;
    result = nx_request_admit(&session->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;
    s->active = session;
    s->time_epoch = nx_stm32_av_epoch();
    s->completed_cycles = 0U;
    s->first_cycle = true;
    s->ready_at = ready;
    s->warming = true;
    nx_pwm_group_frame_t frame = {session->duty_ticks, 3U,
                                  s->config->trigger_ticks};
    result = nx_stm32_pwm_group_ops.commit(&s->pwm, &frame);
    ADC_TypeDef* adc = s->config->analog.adc;
    NX_AV_WRITE(&adc->SR, 0U);
    bool ok = nx_stm32_av_write(
        &adc->CR2,
        ADC_CR2_ADON | (LL_ADC_INJ_TRIG_EXT_TIM8_CH4 & ADC_CR2_JEXTSEL),
        UINT32_MAX);
    if (result != NX_SUCCESS || !ok) {
        trip(s, result != NX_SUCCESS ? result : NX_ERROR_IO);
        (void)service(s);
    }
    return NX_SUCCESS;
}

static nx_result_t drain(nx_stm32_pwm_adc_loop_state_t* s) {
    bool quiet = suppress(s);
    quiet = nx_stm32_pwm_group_ops.stop(&s->pwm) == NX_SUCCESS && quiet;
    if (s->irq_owned)
        quiet = nx_stm32_av_irq_detach(ADC_IRQn) && quiet;
    if (!quiet) {
        s->quarantined = true;
        if (s->active != NULL) {
            (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
            (void)nx_request_transition(&s->active->base,
                                        NX_REQUEST_QUARANTINED);
        }
        return NX_ERROR_BUSY;
    }
    s->irq_owned = false;
    s->warming = false;
    s->running = false;
    s->quarantined = false;
    nx_result_t result = (nx_result_t)fault(s);
    nx_pwm_adc_loop_session_t* session = s->active;
    size_t completed =
        s->completed_cycles > SIZE_MAX ? SIZE_MAX : (size_t)s->completed_cycles;
    s->active = NULL;
    if (session != NULL)
        nx_request_settle(&session->base, result, completed);
    return result;
}

/** \brief Relate modulo CNT to one persistent Core-time start interval. */
static bool anchor(nx_stm32_pwm_adc_loop_state_t* s) {
    TIM_TypeDef* timer = s->config->pwm.registers;
    nx_time_us_t before = NX_AV_NOW();
    uint32_t counter = NX_AV_READ(&timer->CNT);
    nx_time_us_t after = NX_AV_NOW();
    if (after < before || after - before > 1U || after < counter ||
        counter >= s->config->trigger_ticks ||
        (NX_AV_READ(&timer->SR) & TIM_SR_UIF) != 0U)
        return false;
    s->cycle_anchor = after - counter;
    s->last_observed = after;
    return true;
}

static nx_result_t service(void* context) {
    nx_stm32_pwm_adc_loop_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized)
        return NX_ERROR_STATE;
    if (!live(s)) {
        if (s->active != NULL) {
            trip(s, NX_ERROR_STATE);
            s->quarantined = true;
            (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
            (void)nx_request_transition(&s->active->base,
                                        NX_REQUEST_QUARANTINED);
        }
        return NX_ERROR_STATE;
    }
    if (s->active == NULL)
        return NX_SUCCESS;
    if (NX_AV_NOW() >= s->active->base.deadline)
        trip(s, NX_ERROR_TIMEOUT);
    if (fault(s) != NX_SUCCESS)
        return drain(s);
    const nx_stm32_pwm_adc_loop_config_t* c = s->config;
    if (s->running) {
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        nx_time_us_t now = NX_AV_NOW();
        uint64_t limit = (uint64_t)c->trigger_ticks + s->sample_delay_us +
                         c->max_irq_delay_us + 2U;
        bool late = now < s->last_observed ||
                    (now >= s->cycle_anchor && now - s->cycle_anchor > limit);
        nx_arch_irq_restore(saved);
        if (late) {
            trip(s, NX_ERROR_TIMEOUT);
            return drain(s);
        }
    }
    if (s->warming && NX_AV_NOW() >= s->ready_at) {
        s->irq_owned = true;
        NX_AV_IRQ_PRIORITY(ADC_IRQn, c->pwm.common.priority);
        NX_AV_IRQ_CLEAR(ADC_IRQn);
        bool ok = nx_stm32_av_bits(&c->analog.adc->CR1, ADC_CR1_JEOCIE,
                                   ADC_CR1_JEOCIE);
        ok = nx_stm32_av_bits(&c->analog.adc->CR2, ADC_CR2_JEXTEN,
                              ADC_CR2_JEXTEN_0) &&
             ok;
        nx_result_t result =
            ok ? nx_stm32_pwm_group_ops.start(&s->pwm) : NX_ERROR_IO;
        if (result != NX_SUCCESS || !anchor(s) || fault(s) != NX_SUCCESS) {
            trip(s, result != NX_SUCCESS ? result : NX_ERROR_TIMEOUT);
            return drain(s);
        }
        s->warming = false;
        s->running = true;
        nx_arch_dsb();
        NX_AV_IRQ_ENABLE(ADC_IRQn);
    }
    return NX_SUCCESS;
}

static nx_result_t stop(void* context) {
    nx_stm32_pwm_adc_loop_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized || !live(s))
        return NX_ERROR_STATE;
    s->stopping = true;
    if (s->active != NULL)
        trip(s, NX_ERROR_CANCELLED);
    nx_result_t result = drain(s);
    return s->active == NULL && !s->quarantined ? NX_SUCCESS : result;
}

static nx_result_t state(const void* context, nx_pwm_adc_loop_state_t* out) {
    const nx_stm32_pwm_adc_loop_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized || out == NULL ||
        (uintptr_t)out % _Alignof(nx_pwm_adc_loop_state_t) != 0U ||
        !disjoint(s, out, sizeof(*out)))
        return NX_ERROR_INVALID;
    const nx_stm32_pwm_adc_loop_config_t* c = s->config;
    if (s->active != NULL) {
        const nx_pwm_adc_loop_session_t* session = s->active;
        if (nx_stm32_av_overlap(out, sizeof(*out), session, sizeof(*session)) ||
            nx_stm32_av_overlap(out, sizeof(*out), session->samples,
                                session->sample_count * sizeof(uint16_t)) ||
            nx_stm32_av_overlap(out, sizeof(*out), session->duty_ticks,
                                session->duty_count * sizeof(uint32_t)) ||
            nx_stm32_av_overlap(out, sizeof(*out), session->context, 1U))
            return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    *out = (nx_pwm_adc_loop_state_t){
        .format = {.sample_stride = sizeof(uint16_t),
                   .scan_stride = sizeof(uint16_t) * c->analog.channel_count,
                   .trigger_identity = 8U,
                   .channel_count = (uint16_t)c->analog.channel_count,
                   .storage_bytes = sizeof(uint16_t),
                   .significant_bits = 12U,
                   .encoding = NX_ADC_UNSIGNED,
                   .byte_order = NX_ADC_LITTLE_ENDIAN,
                   .group = NX_ADC_INJECTED,
                   .trigger = NX_ADC_TRIGGER_TIMER},
        .tick_hz = c->pwm.tick_hz,
        .period_ticks = c->pwm.period_ticks,
        .trigger_ticks = c->trigger_ticks,
        .commit_before_ticks = c->commit_before_ticks,
        .compute_budget_ticks = c->compute_budget_ticks,
        .completed_cycles = s->completed_cycles,
        .duty_count = 3U,
        .running = s->running && fault(s) == NX_SUCCESS,
        .fault = fault(s) != NX_SUCCESS,
        .quarantined = s->quarantined};
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Reject lost cycles using absolute anchors before reading JDR data. */
static bool phase(nx_stm32_pwm_adc_loop_state_t* s, nx_time_us_t now) {
    const nx_stm32_pwm_adc_loop_config_t* c = s->config;
    if (!live(s)) {
        trip(s, NX_ERROR_STATE);
        return false;
    }
    if (now < s->last_observed || now < s->cycle_anchor ||
        now - s->cycle_anchor + 2U < c->trigger_ticks ||
        now - s->cycle_anchor > (uint64_t)c->trigger_ticks +
                                    s->sample_delay_us + c->max_irq_delay_us +
                                    2U ||
        now >= s->active->base.deadline) {
        trip(s, NX_ERROR_TIMEOUT);
        return false;
    }
    TIM_TypeDef* timer = c->pwm.registers;
    uint32_t count = NX_AV_READ(&timer->CNT);
    bool update = (NX_AV_READ(&timer->SR) & TIM_SR_UIF) != 0U;
    if (count < c->trigger_ticks || count >= c->commit_before_ticks ||
        update == s->first_cycle ||
        (NX_AV_READ(&timer->CR1) & TIM_CR1_CEN) == 0U ||
        (NX_AV_READ(&timer->BDTR) & TIM_BDTR_MOE) == 0U) {
        trip(s, NX_ERROR_STATE);
        return false;
    }
    NX_AV_WRITE(&timer->SR, ~(uint32_t)TIM_SR_UIF);
    return (NX_AV_READ(&timer->SR) & TIM_SR_UIF) == 0U;
}

/** \brief UDIS prevents mixed shadows; both modulo and absolute bounds apply.
 */
static bool commit(nx_stm32_pwm_adc_loop_state_t* s, nx_time_us_t began) {
    const nx_stm32_pwm_adc_loop_config_t* c = s->config;
    nx_pwm_adc_loop_session_t* session = s->active;
    TIM_TypeDef* timer = c->pwm.registers;
    if (fault(s) != NX_SUCCESS)
        return false;
    if (!live(s)) {
        trip(s, NX_ERROR_STATE);
        return false;
    }
    for (size_t i = 0U; i < 3U; ++i)
        if (session->duty_ticks[i] > c->pwm.period_ticks) {
            trip(s, NX_ERROR_INVALID);
            return false;
        }
    nx_time_us_t now = NX_AV_NOW();
    if (now >= session->base.deadline || now < began ||
        now - began > c->compute_budget_ticks || now < s->cycle_anchor ||
        now - s->cycle_anchor >= c->commit_before_ticks - 2U) {
        trip(s, NX_ERROR_TIMEOUT);
        return false;
    }
    if (!nx_stm32_av_bits(&timer->CR1, TIM_CR1_UDIS, TIM_CR1_UDIS)) {
        trip(s, NX_ERROR_IO);
        return false;
    }
    bool ok =
        nx_stm32_av_write(&timer->CCR1, session->duty_ticks[0], UINT32_MAX);
    ok = nx_stm32_av_write(&timer->CCR2, session->duty_ticks[1], UINT32_MAX) &&
         ok;
    ok = nx_stm32_av_write(&timer->CCR3, session->duty_ticks[2], UINT32_MAX) &&
         ok;
    now = NX_AV_NOW();
    uint32_t count = NX_AV_READ(&timer->CNT);
    if (now >= session->base.deadline || now < began ||
        now - began > c->compute_budget_ticks || now < s->cycle_anchor ||
        now - s->cycle_anchor >= c->commit_before_ticks - 2U ||
        count < c->trigger_ticks || count >= c->commit_before_ticks ||
        (NX_AV_READ(&timer->SR) & (TIM_SR_UIF | TIM_SR_BIF)) != 0U) {
        trip(s, NX_ERROR_TIMEOUT);
        return false;
    }
    if (!ok || !nx_stm32_av_bits(&timer->CR1, TIM_CR1_UDIS, 0U)) {
        trip(s, NX_ERROR_IO);
        return false;
    }
    for (size_t i = 0U; i < 3U; ++i)
        s->pwm.duties[i] = session->duty_ticks[i];
    return true;
}

void nx_stm32_pwm_adc_loop_adc_irq(nx_stm32_pwm_adc_loop_state_t* s) {
    if (s == NULL || !s->initialized || s->active == NULL || !s->running ||
        fault(s) != NX_SUCCESS)
        return;
    ADC_TypeDef* adc = s->config->analog.adc;
    uint32_t status = NX_AV_READ(&adc->SR);
    if ((status & ADC_SR_OVR) != 0U) {
        trip(s, NX_ERROR_OVERFLOW);
        return;
    }
    if ((status & ADC_SR_JEOC) == 0U)
        return;
    nx_time_us_t began = NX_AV_NOW();
    if (!phase(s, began)) {
        if (fault(s) == NX_SUCCESS)
            trip(s, NX_ERROR_IO);
        return;
    }
    if (s->completed_cycles == UINT64_MAX) {
        trip(s, NX_ERROR_EXHAUSTED);
        return;
    }
    NX_AV_WRITE(&adc->SR, ~(uint32_t)ADC_SR_JEOC);
    if ((NX_AV_READ(&adc->SR) & ADC_SR_JEOC) != 0U) {
        trip(s, NX_ERROR_OVERFLOW);
        return;
    }
    volatile uint32_t* data[4] = {&adc->JDR1, &adc->JDR2, &adc->JDR3,
                                  &adc->JDR4};
    nx_pwm_adc_loop_session_t* session = s->active;
    for (size_t i = 0U; i < session->sample_count; ++i)
        session->samples[i] = (uint16_t)NX_AV_READ(data[i]);
    if ((NX_AV_READ(&adc->SR) & ADC_SR_JEOC) != 0U) {
        trip(s, NX_ERROR_OVERFLOW);
        return;
    }
    nx_result_t result = session->compute(
        session->context, session->samples, session->sample_count,
        s->completed_cycles, session->duty_ticks, session->duty_count);
    if (result != NX_SUCCESS) {
        trip(s, result);
        return;
    }
    if ((NX_AV_READ(&adc->SR) & ADC_SR_JEOC) != 0U) {
        trip(s, NX_ERROR_OVERFLOW);
        return;
    }
    if (!commit(s, began))
        return;
    nx_time_us_t next;
    result = nx_deadline_finite_after(s->cycle_anchor,
                                      s->config->pwm.period_ticks, &next);
    if (result != NX_SUCCESS) {
        trip(s, result);
        return;
    }
    ++s->completed_cycles;
    s->cycle_anchor = next;
    s->last_observed = NX_AV_NOW();
    s->first_cycle = false;
}

void nx_stm32_pwm_adc_loop_break_irq(nx_stm32_pwm_adc_loop_state_t* s) {
    if (s == NULL || !s->owned || s->active == NULL)
        return;
    const nx_stm32_waveform_config_t* c = &s->config->pwm;
    if ((NX_AV_READ(&c->registers->SR) & TIM_SR_BIF) != 0U ||
        nx_stm32_timer_break_active(c->break_input, c->break_mask,
                                    c->break_active_high))
        trip(s, NX_ERROR_IO);
}

nx_result_t nx_stm32_pwm_adc_loop_close(nx_stm32_pwm_adc_loop_state_t* s) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (!s->owned)
        return NX_SUCCESS;
    if (s->initialized) {
        nx_result_t result = stop(s);
        if (result != NX_SUCCESS)
            return result;
    }
    if (s->pwm.owned) {
        nx_result_t result = nx_stm32_waveform_close(&s->pwm);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_result_t result = nx_stm32_control_adc_close(&s->analog);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_stm32_av_irq_priority_restore(ADC_IRQn, s->irq_priority))
        return NX_ERROR_IO;
    /* An incomplete close retains every field above. A proved joined close
     * retires the session so the same static assembly can prepare again. */
    *s = (nx_stm32_pwm_adc_loop_state_t){0};
    return NX_SUCCESS;
}

const nx_pwm_adc_loop_ops_t nx_stm32_pwm_adc_loop_ops = {
    .start = start, .service = service, .stop = stop, .state = state};
