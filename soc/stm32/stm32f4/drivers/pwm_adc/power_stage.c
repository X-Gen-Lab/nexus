/**
 * \file            power_stage.c
 * \brief           Single-owner F407 complementary output and raw drive scan
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/pwm_adc/power_stage.h"
#include "drivers/timer/stm32f407_timer_output.h"
#include "stm32f4xx_ll_adc.h"

static uint32_t fault(const nx_stm32_power_stage_t* s) {
    return __atomic_load_n(&s->fault, __ATOMIC_ACQUIRE);
}
static uint32_t ticks_per_us(const nx_stm32_power_stage_config_t* c) {
    return c->pwm.tick_hz / 1000000U;
}
static uint32_t conversion_us(const nx_stm32_power_stage_config_t* c) {
    const uint16_t cycles[8] = {3U, 15U, 28U, 56U, 84U, 112U, 144U, 480U};
    uint64_t total = 0U;
    for (size_t i = 0U; i < 4U; ++i)
        total += (uint64_t)cycles[c->analog.sample_times[i]] + 12U;
    return (uint32_t)((total * 1000000U + c->analog.clock_hz - 1U) /
                      c->analog.clock_hz);
}
static bool config_valid(const nx_stm32_power_stage_config_t* c) {
    if (c == NULL ||
        (uintptr_t)c % _Alignof(nx_stm32_power_stage_config_t) != 0U ||
        !nx_stm32_control_adc_valid(&c->analog) ||
        c->analog.channel_count != 4U || c->encoder == NULL ||
        c->encoder_pins == NULL || c->encoder_filter > 15U ||
        c->pwm.controller != 8U || c->pwm.kind != NX_STM32_WAVE_GROUP ||
        c->pwm.channels != 7U || !c->pwm.complementary ||
        !c->pwm.internal_trigger || c->pwm.master_mode != 7U ||
        c->pwm.sync_mode != 0U || c->pwm.timer_hz == 0U ||
        c->pwm.tick_hz < 1000000U || c->pwm.tick_hz % 1000000U != 0U ||
        c->pwm.period_ticks > UINT16_MAX || c->minimum_duty_ticks == 0U ||
        c->minimum_duty_ticks >= c->maximum_duty_ticks ||
        c->maximum_duty_ticks >= c->pwm.period_ticks ||
        c->settling_ticks == 0U || c->max_irq_delay_us == 0U ||
        c->compute_budget_us == 0U)
        return false;
#ifndef NEXUS_STM32_MODEL
    if (c->encoder != TIM4)
        return false;
#endif
    for (size_t i = 0U; i < 4U; ++i)
        if (c->analog.channels[i] != i)
            return false;
    for (size_t i = 0U; i < 2U; ++i) {
        const nx_stm32_av_pin_t* pin = &c->encoder_pins[i];
        if (pin->registers == NULL || pin->snapshot == NULL ||
            pin->bank != 3U || pin->pin != 12U + i || pin->af != 2U)
            return false;
#ifndef NEXUS_STM32_MODEL
        if (pin->registers != GPIOD)
            return false;
#endif
    }
    uint32_t unit = ticks_per_us(c);
    if (c->pwm.period_ticks < 4U * unit || c->trigger_ticks % unit != 0U ||
        c->pwm.period_ticks % unit != 0U ||
        c->commit_before_ticks % unit != 0U ||
        c->commit_before_ticks > c->pwm.period_ticks - 2U * unit)
        return false;
    uint64_t deadtime = ((uint64_t)c->pwm.deadtime_ticks * c->pwm.tick_hz +
                         c->pwm.timer_hz - 1U) /
                        c->pwm.timer_hz;
    uint64_t conversion = (uint64_t)conversion_us(c) * unit;
    return (uint64_t)c->maximum_duty_ticks + deadtime + c->settling_ticks <
               c->trigger_ticks &&
           (uint64_t)c->trigger_ticks + conversion < c->pwm.period_ticks &&
           (uint64_t)c->trigger_ticks + conversion +
                   ((uint64_t)c->max_irq_delay_us + c->compute_budget_us + 4U) *
                       unit <
               c->commit_before_ticks;
}
static bool irq_idle(IRQn_Type irq) {
    return NX_AV_IRQ_ACTIVE(irq) == 0U && NX_AV_IRQ_ENABLED(irq) == 0U &&
           NX_AV_IRQ_PENDING(irq) == 0U;
}
nx_result_t nx_stm32_power_stage_admit(const nx_stm32_power_stage_t* s,
                                       const nx_stm32_power_stage_config_t* c) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || (uintptr_t)s % _Alignof(nx_stm32_power_stage_t) != 0U ||
        !config_valid(c) || nx_stm32_av_overlap(s, sizeof(*s), c, sizeof(*c)))
        return NX_ERROR_INVALID;
    if (s->owned || s->active != NULL)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32_waveform_admit(&s->pwm, &c->pwm);
    if (result == NX_SUCCESS)
        result = nx_stm32_control_adc_admit(&s->analog, &c->analog);
    if (result != NX_SUCCESS)
        return result;
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    if ((NX_AV_READ(&rcc->APB1ENR) & RCC_APB1ENR_TIM4EN) != 0U ||
        (NX_AV_READ(&rcc->APB1RSTR) & RCC_APB1RSTR_TIM4RST) != 0U ||
        !irq_idle(TIM4_IRQn) || !irq_idle(ADC_IRQn))
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < 2U; ++i)
        if ((NX_AV_READ(&rcc->AHB1ENR) & RCC_AHB1ENR_GPIODEN) != 0U &&
            (NX_AV_READ(&c->encoder_pins[i].registers->LCKR) &
             (1U << c->encoder_pins[i].pin)) != 0U)
            return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
static bool sequence(const nx_stm32_power_stage_config_t* c) {
    uint32_t smpr = 0U;
    uint32_t ranks = 3U << ADC_JSQR_JL_Pos;
    for (size_t i = 0U; i < 4U; ++i) {
        smpr |= (uint32_t)c->analog.sample_times[i] << (i * 3U);
        ranks |= (uint32_t)c->analog.channels[i] << (i * 5U);
    }
    bool ok = nx_stm32_av_write(&c->analog.adc->SMPR1, 0U, UINT32_MAX);
    ok = nx_stm32_av_write(&c->analog.adc->SMPR2, smpr, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&c->analog.adc->JSQR, ranks, UINT32_MAX) && ok;
    /* SCAN selects the declared injected sequence; offsets must not survive
     * an earlier inactive owner into the unsigned raw-data contract. */
    ok =
        nx_stm32_av_bits(&c->analog.adc->CR1, ADC_CR1_SCAN, ADC_CR1_SCAN) && ok;
    volatile uint32_t* offsets[4] = {
        &c->analog.adc->JOFR1, &c->analog.adc->JOFR2, &c->analog.adc->JOFR3,
        &c->analog.adc->JOFR4};
    for (size_t i = 0U; i < 4U; ++i)
        ok = nx_stm32_av_write(offsets[i], 0U, UINT32_MAX) && ok;
    return ok;
}
static nx_result_t encoder_prepare(nx_stm32_power_stage_t* s) {
    const nx_stm32_power_stage_config_t* c = s->config;
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    if ((NX_AV_READ(&rcc->AHB1ENR) & RCC_AHB1ENR_GPIODEN) == 0U)
        return NX_ERROR_STATE;
    s->encoder_owned = true;
    if (!nx_stm32_av_bits(&rcc->APB1ENR, RCC_APB1ENR_TIM4EN,
                          RCC_APB1ENR_TIM4EN))
        return NX_ERROR_IO;
    TIM_TypeDef* t = c->encoder;
    if ((NX_AV_READ(&t->CR1) & TIM_CR1_CEN) != 0U ||
        NX_AV_READ(&t->DIER) != 0U || NX_AV_READ(&t->CCER) != 0U)
        return NX_ERROR_BUSY;
    s->encoder_touched = true;
    uint32_t inputs = 0x101U | ((uint32_t)c->encoder_filter << 4U) |
                      ((uint32_t)c->encoder_filter << 12U);
    bool ok = nx_stm32_av_write(&t->CR1, 0U, UINT32_MAX);
    ok = nx_stm32_av_write(&t->CR2, 1U << TIM_CR2_MMS_Pos, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&t->DIER, 0U, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&t->SMCR, 3U, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&t->CCMR1, inputs, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&t->CCMR2, 0U, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&t->PSC, 0U, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&t->ARR, UINT16_MAX, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&t->CNT, 0U, UINT32_MAX) && ok;
    if (!ok)
        return NX_ERROR_IO;
    /* With CEN=0, MMS=ENABLE keeps UG off TRGO while latching PSC/ARR. */
    NX_AV_WRITE(&t->EGR, TIM_EGR_UG);
    NX_AV_WRITE(&t->SR, 0U);
    if (NX_AV_READ(&t->SR) != 0U || !nx_stm32_av_write(&t->CR2, 0U, UINT32_MAX))
        return NX_ERROR_IO;
    nx_stm32_av_config_t pads = {c->encoder_pins, 2U, NULL, 0U, 1U, 0U};
    return nx_stm32_av_pins(&pads);
}
nx_result_t
nx_stm32_power_stage_prepare(nx_stm32_power_stage_t* s,
                             const nx_stm32_power_stage_config_t* c) {
    nx_result_t result = nx_stm32_power_stage_admit(s, c);
    if (result != NX_SUCCESS)
        return result;
    if (!g_nx_stm32_system.started ||
        !nx_stm32_clock_profile_matches(g_nx_stm32_system.rcc, NULL))
        return NX_ERROR_STATE;
    s->config = c;
    s->owned = true;
    s->irq_priority = NX_AV_IRQ_PRIORITY_READ(ADC_IRQn);
    s->scan_span_us = conversion_us(c);
    result = nx_stm32_control_adc_prepare(&s->analog, &c->analog);
    if (result != NX_SUCCESS)
        return result;
    if (!sequence(c))
        return NX_ERROR_IO;
    result = encoder_prepare(s);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32_waveform_prepare(&s->pwm, &c->pwm);
    if (result == NX_SUCCESS)
        s->initialized = true;
    return result;
}
static bool live(nx_stm32_power_stage_t* s) {
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    bool ok = !s->history_lost && g_nx_stm32_system.started &&
              nx_stm32_clock_profile_matches(rcc, NULL) &&
              (NX_AV_READ(&rcc->APB2ENR) &
               (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_TIM8EN)) ==
                  (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_TIM8EN) &&
              (NX_AV_READ(&rcc->APB2RSTR) &
               (RCC_APB2RSTR_ADCRST | RCC_APB2RSTR_TIM8RST)) == 0U &&
              (NX_AV_READ(&rcc->APB1ENR) & RCC_APB1ENR_TIM4EN) != 0U &&
              (NX_AV_READ(&rcc->APB1RSTR) & RCC_APB1RSTR_TIM4RST) == 0U;
    if (s->active != NULL)
        ok = ok && nx_stm32_av_timebase() &&
             g_nx_stm32_system.timer_error == NX_SUCCESS &&
             s->time_epoch == nx_stm32_av_epoch();
    if (!ok && s->active != NULL)
        s->history_lost = true;
    return ok;
}
/** \brief Prove gates before MMIO; suppression never revokes the session loan.
 */
static bool suppress(nx_stm32_power_stage_t* s) {
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    uint32_t gates = NX_AV_READ(&rcc->APB2ENR);
    const nx_stm32_power_stage_config_t* c = s->config;
    bool quiet = false;
    if ((gates & RCC_APB2ENR_TIM8EN) != 0U)
        quiet = nx_stm32_timer_output_stop(c->pwm.registers, true, UINT32_MAX,
                                           UINT32_MAX);
    if ((gates & RCC_APB2ENR_ADC1EN) == 0U)
        return false;
    bool analog = nx_stm32_av_bits(&c->analog.adc->CR1,
                                   ADC_CR1_JEOCIE | ADC_CR1_OVRIE, 0U);
    analog = nx_stm32_av_bits(&c->analog.adc->CR2,
                              ADC_CR2_JEXTEN | ADC_CR2_ADON, 0U) &&
             analog;
    return quiet && analog;
}
static void trip(nx_stm32_power_stage_t* s, nx_result_t result) {
    uint32_t clear = NX_SUCCESS;
    (void)__atomic_compare_exchange_n(&s->fault, &clear, (uint32_t)result,
                                      false, __ATOMIC_RELEASE,
                                      __ATOMIC_RELAXED);
    (void)suppress(s);
}
static bool disjoint(const nx_stm32_power_stage_t* s, const void* data,
                     size_t bytes) {
    const nx_stm32_power_stage_config_t* c = s->config;
    if (data == NULL || bytes == 0U || bytes > UINTPTR_MAX - (uintptr_t)data ||
        !nx_stm32_control_adc_disjoint(&c->analog, data, bytes) ||
        nx_stm32_av_overlap(data, bytes, s, sizeof(*s)) ||
        nx_stm32_av_overlap(data, bytes, c, sizeof(*c)) ||
        nx_stm32_av_overlap(data, bytes, c->pwm.registers,
                            sizeof(*c->pwm.registers)) ||
        nx_stm32_av_overlap(data, bytes, c->encoder, sizeof(*c->encoder)))
        return false;
    const nx_stm32_av_pin_t* lists[2] = {c->pwm.common.pins, c->encoder_pins};
    const size_t counts[2] = {c->pwm.common.pin_count, 2U};
    for (size_t list = 0U; list < 2U; ++list) {
        if (nx_stm32_av_overlap(data, bytes, lists[list],
                                counts[list] * sizeof(*lists[list])))
            return false;
        for (size_t i = 0U; i < counts[list]; ++i)
            if (nx_stm32_av_overlap(data, bytes, lists[list][i].registers,
                                    sizeof(*lists[list][i].registers)) ||
                nx_stm32_av_overlap(data, bytes, lists[list][i].snapshot,
                                    sizeof(*lists[list][i].snapshot)))
                return false;
    }
    return true;
}
static bool plan_valid(const nx_stm32_power_stage_t* s,
                       const nx_power_stage_plan_t* plan) {
    for (size_t i = 0U; i < 3U; ++i)
        if (plan->duty_ticks[i] < s->config->minimum_duty_ticks ||
            plan->duty_ticks[i] > s->config->maximum_duty_ticks)
            return false;
    return true;
}
static nx_result_t drain(nx_stm32_power_stage_t* s) {
    bool ok = suppress(s);
    ok = nx_stm32_pwm_group_ops.stop(&s->pwm) == NX_SUCCESS && ok;
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    if ((NX_AV_READ(&rcc->APB1ENR) & RCC_APB1ENR_TIM4EN) == 0U)
        ok = false;
    else {
        ok = nx_stm32_av_bits(&s->config->encoder->CR1, TIM_CR1_CEN, 0U) && ok;
        ok = nx_stm32_av_write(&s->config->encoder->CCER, 0U, UINT32_MAX) && ok;
    }
    if (s->irq_owned)
        ok = nx_stm32_av_irq_detach(ADC_IRQn) && ok;
    if (!ok) {
        s->quarantined = true;
        if (s->active != NULL) {
            (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
            (void)nx_request_transition(&s->active->base,
                                        NX_REQUEST_QUARANTINED);
        }
        return NX_ERROR_BUSY;
    }
    s->irq_owned = false;
    s->running = false;
    s->warming = false;
    s->quarantined = false;
    nx_power_stage_session_t* session = s->active;
    s->active = NULL;
    nx_result_t result = (nx_result_t)fault(s);
    if (session != NULL)
        nx_request_settle(&session->base, result,
                          s->completed_cycles > SIZE_MAX
                              ? SIZE_MAX
                              : (size_t)s->completed_cycles);
    return result;
}
static nx_result_t service(void* context);
static nx_result_t start(void* context, nx_power_stage_session_t* session) {
    nx_stm32_power_stage_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized || s->stopping || fault(s) != NX_SUCCESS ||
        s->quarantined)
        return NX_ERROR_STATE;
    if (s->active != NULL)
        return NX_ERROR_BUSY;
    if (session == NULL ||
        (uintptr_t)session % _Alignof(nx_power_stage_session_t) != 0U ||
        !disjoint(s, session, sizeof(*session)) || session->compute == NULL ||
        !disjoint(s, session->context, session->context_bytes) ||
        nx_stm32_av_overlap(session, sizeof(*session), session->context,
                            session->context_bytes) ||
        !plan_valid(s, &session->initial))
        return NX_ERROR_INVALID;
    if (nx_request_state(&session->base) != NX_REQUEST_READY || !live(s))
        return NX_ERROR_STATE;
    nx_time_us_t now = NX_AV_NOW(), ready;
    if (session->base.deadline != NX_DEADLINE_NEVER ||
        session->startup_deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (now >= session->startup_deadline)
        return NX_ERROR_TIMEOUT;
    nx_result_t result = nx_deadline_finite_after(now, 4U, &ready);
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
    nx_pwm_group_frame_t frame = {session->initial.duty_ticks, 3U,
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
static bool anchor(nx_stm32_power_stage_t* s) {
    nx_time_us_t before = NX_AV_NOW();
    uint32_t count = NX_AV_READ(&s->config->pwm.registers->CNT);
    nx_time_us_t after = NX_AV_NOW();
    uint32_t unit = ticks_per_us(s->config);
    uint32_t count_us = count / unit;
    if (after < before || after - before > 1U || after < count_us ||
        count >= s->config->trigger_ticks ||
        (NX_AV_READ(&s->config->pwm.registers->SR) & TIM_SR_UIF) != 0U)
        return false;
    s->cycle_anchor = after - count_us;
    s->last_observed = after;
    return true;
}
static nx_result_t service(void* context) {
    nx_stm32_power_stage_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized)
        return NX_ERROR_STATE;
    if (!live(s)) {
        if (s->active != NULL) {
            trip(s, NX_ERROR_STATE);
            return drain(s);
        }
        return NX_ERROR_STATE;
    }
    if (s->active == NULL)
        return NX_SUCCESS;
    if (fault(s) != NX_SUCCESS)
        return drain(s);
    const nx_stm32_power_stage_config_t* c = s->config;
    if (s->warming && NX_AV_NOW() >= s->active->startup_deadline) {
        trip(s, NX_ERROR_TIMEOUT);
        return drain(s);
    }
    if (s->running) {
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        nx_time_us_t now = NX_AV_NOW();
        uint64_t limit = c->trigger_ticks / ticks_per_us(c) +
                         (uint64_t)s->scan_span_us + c->max_irq_delay_us + 2U;
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
        bool ok =
            nx_stm32_av_bits(&c->encoder->CCER, TIM_CCER_CC1E | TIM_CCER_CC2E,
                             TIM_CCER_CC1E | TIM_CCER_CC2E);
        ok = nx_stm32_av_bits(&c->encoder->CR1, TIM_CR1_CEN, TIM_CR1_CEN) && ok;
        ok = nx_stm32_av_bits(&c->analog.adc->CR1, ADC_CR1_JEOCIE,
                              ADC_CR1_JEOCIE) &&
             ok;
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
    nx_stm32_power_stage_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized)
        return NX_ERROR_STATE;
    bool identity = live(s);
    s->stopping = true;
    if (s->active != NULL)
        trip(s, identity ? NX_ERROR_CANCELLED : NX_ERROR_STATE);
    nx_result_t result = drain(s);
    return s->active == NULL && !s->quarantined ? NX_SUCCESS : result;
}
static nx_result_t state(const void* context, nx_power_stage_state_t* out) {
    const nx_stm32_power_stage_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized || out == NULL ||
        (uintptr_t)out % _Alignof(nx_power_stage_state_t) != 0U ||
        !disjoint(s, out, sizeof(*out)) ||
        (s->active != NULL &&
         (nx_stm32_av_overlap(out, sizeof(*out), s->active,
                              sizeof(*s->active)) ||
          nx_stm32_av_overlap(out, sizeof(*out), s->active->context,
                              s->active->context_bytes))))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    const nx_stm32_power_stage_config_t* c = s->config;
    *out = (nx_power_stage_state_t){.tick_hz = c->pwm.tick_hz,
                                    .period_ticks = c->pwm.period_ticks,
                                    .trigger_ticks = c->trigger_ticks,
                                    .minimum_duty_ticks = c->minimum_duty_ticks,
                                    .maximum_duty_ticks = c->maximum_duty_ticks,
                                    .scan_span_us = s->scan_span_us,
                                    .completed_cycles = s->completed_cycles,
                                    .fault = (nx_result_t)fault(s),
                                    .running =
                                        s->running && fault(s) == NX_SUCCESS,
                                    .quarantined = s->quarantined};
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
static bool phase(nx_stm32_power_stage_t* s, nx_time_us_t now) {
    const nx_stm32_power_stage_config_t* c = s->config;
    uint32_t unit = ticks_per_us(c);
    if (!live(s)) {
        trip(s, NX_ERROR_STATE);
        return false;
    }
    if (now < s->last_observed || now < s->cycle_anchor ||
        now - s->cycle_anchor + 2U < c->trigger_ticks / unit ||
        now - s->cycle_anchor > (uint64_t)c->trigger_ticks / unit +
                                    s->scan_span_us + c->max_irq_delay_us +
                                    2U) {
        trip(s, NX_ERROR_TIMEOUT);
        return false;
    }
    TIM_TypeDef* timer = c->pwm.registers;
    uint32_t count = NX_AV_READ(&timer->CNT);
    bool update = (NX_AV_READ(&timer->SR) & TIM_SR_UIF) != 0U;
    if (count < c->trigger_ticks || count >= c->commit_before_ticks ||
        update == s->first_cycle ||
        (NX_AV_READ(&timer->CR1) & TIM_CR1_CEN) == 0U ||
        (NX_AV_READ(&timer->BDTR) & TIM_BDTR_MOE) == 0U ||
        (NX_AV_READ(&c->encoder->CR1) & TIM_CR1_CEN) == 0U ||
        (NX_AV_READ(&c->encoder->CCER) & (TIM_CCER_CC1E | TIM_CCER_CC2E)) !=
            (TIM_CCER_CC1E | TIM_CCER_CC2E)) {
        trip(s, NX_ERROR_STATE);
        return false;
    }
    NX_AV_WRITE(&timer->SR, ~(uint32_t)TIM_SR_UIF);
    return (NX_AV_READ(&timer->SR) & TIM_SR_UIF) == 0U;
}
/** \brief A complete validated shadow group is released only inside its phase.
 */
static bool commit(nx_stm32_power_stage_t* s, const nx_power_stage_plan_t* plan,
                   nx_time_us_t began) {
    if (fault(s) != NX_SUCCESS)
        return false;
    if (!plan_valid(s, plan)) {
        trip(s, NX_ERROR_INVALID);
        return false;
    }
    const nx_stm32_power_stage_config_t* c = s->config;
    TIM_TypeDef* timer = c->pwm.registers;
    nx_time_us_t now = NX_AV_NOW();
    if (!live(s) || now < began || now - began > c->compute_budget_us ||
        now < s->cycle_anchor ||
        now - s->cycle_anchor >=
            c->commit_before_ticks / ticks_per_us(c) - 2U) {
        trip(s, NX_ERROR_TIMEOUT);
        return false;
    }
    if (!nx_stm32_av_bits(&timer->CR1, TIM_CR1_UDIS, TIM_CR1_UDIS)) {
        trip(s, NX_ERROR_IO);
        return false;
    }
    bool ok = nx_stm32_av_write(&timer->CCR1, plan->duty_ticks[0], UINT32_MAX);
    ok = nx_stm32_av_write(&timer->CCR2, plan->duty_ticks[1], UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&timer->CCR3, plan->duty_ticks[2], UINT32_MAX) && ok;
    now = NX_AV_NOW();
    uint32_t count = NX_AV_READ(&timer->CNT);
    if (now < began || now - began > c->compute_budget_us ||
        now < s->cycle_anchor ||
        now - s->cycle_anchor >=
            c->commit_before_ticks / ticks_per_us(c) - 2U ||
        count < c->trigger_ticks || count >= c->commit_before_ticks ||
        (NX_AV_READ(&timer->SR) & (TIM_SR_UIF | TIM_SR_BIF)) != 0U ||
        (NX_AV_READ(&c->analog.adc->SR) & ADC_SR_JEOC) != 0U) {
        trip(s, NX_ERROR_TIMEOUT);
        return false;
    }
    if (!ok || !nx_stm32_av_bits(&timer->CR1, TIM_CR1_UDIS, 0U)) {
        trip(s, NX_ERROR_IO);
        return false;
    }
    for (size_t i = 0U; i < 3U; ++i)
        s->pwm.duties[i] = plan->duty_ticks[i];
    return true;
}
void nx_stm32_power_stage_adc_irq(nx_stm32_power_stage_t* s) {
    if (s == NULL || !s->initialized || s->active == NULL || !s->running ||
        fault(s) != NX_SUCCESS)
        return;
    if (!live(s)) {
        trip(s, NX_ERROR_STATE);
        return;
    }
    ADC_TypeDef* adc = s->config->analog.adc;
    uint32_t status = NX_AV_READ(&adc->SR);
    if ((status & ADC_SR_OVR) != 0U) {
        trip(s, NX_ERROR_OVERFLOW);
        return;
    }
    if ((status & ADC_SR_JEOC) == 0U)
        return;
    nx_time_us_t began = NX_AV_NOW();
    if (!phase(s, began))
        return;
    if (s->completed_cycles == UINT64_MAX) {
        trip(s, NX_ERROR_EXHAUSTED);
        return;
    }
    NX_AV_WRITE(&adc->SR, ~(uint32_t)ADC_SR_JEOC);
    if ((NX_AV_READ(&adc->SR) & ADC_SR_JEOC) != 0U) {
        trip(s, NX_ERROR_OVERFLOW);
        return;
    }
    nx_time_us_t next;
    if (nx_deadline_finite_after(s->cycle_anchor,
                                 s->config->pwm.period_ticks /
                                     ticks_per_us(s->config),
                                 &next) != NX_SUCCESS) {
        trip(s, NX_ERROR_EXHAUSTED);
        return;
    }
    volatile uint32_t* ranks[4] = {&adc->JDR1, &adc->JDR2, &adc->JDR3,
                                   &adc->JDR4};
    uint16_t raw[4];
    for (size_t i = 0U; i < 4U; ++i) {
        uint32_t value = NX_AV_READ(ranks[i]);
        if (value > 4095U) {
            trip(s, NX_ERROR_INVALID);
            return;
        }
        raw[i] = (uint16_t)value;
    }
    nx_time_us_t angle_before = NX_AV_NOW();
    uint32_t encoder = NX_AV_READ(&s->config->encoder->CNT);
    nx_time_us_t angle_after = NX_AV_NOW();
    if (angle_after < angle_before || angle_after - angle_before > 1U ||
        encoder > UINT16_MAX) {
        trip(s, NX_ERROR_TIMEOUT);
        return;
    }
    nx_power_stage_sample_t sample = {.current_raw = {raw[0], raw[1], raw[2]},
                                      .bus_raw = raw[3],
                                      .encoder_count = encoder,
                                      .sequence = s->completed_cycles,
                                      .clock_epoch = s->time_epoch,
                                      .sample_at = s->cycle_anchor +
                                                   s->config->trigger_ticks /
                                                       ticks_per_us(s->config),
                                      .observed_at = angle_after,
                                      .angle_at = angle_after,
                                      .planned_apply_at = next,
                                      .scan_span_us = s->scan_span_us};
    if ((NX_AV_READ(&adc->SR) & ADC_SR_JEOC) != 0U) {
        trip(s, NX_ERROR_OVERFLOW);
        return;
    }
    nx_power_stage_plan_t plan = {{0U, 0U, 0U}};
    nx_result_t result = s->active->compute(s->active->context, &sample, &plan);
    if (result != NX_SUCCESS) {
        trip(s, result);
        return;
    }
    if (!commit(s, &plan, began))
        return;
    ++s->completed_cycles;
    s->cycle_anchor = next;
    s->last_observed = NX_AV_NOW();
    s->first_cycle = false;
}
void nx_stm32_power_stage_break_irq(nx_stm32_power_stage_t* s) {
    if (s == NULL || !s->owned || s->active == NULL)
        return;
    if ((NX_AV_READ(&g_nx_stm32_system.rcc->APB2ENR) & RCC_APB2ENR_TIM8EN) ==
        0U)
        return;
    const nx_stm32_waveform_config_t* c = &s->config->pwm;
    if ((NX_AV_READ(&c->registers->SR) & TIM_SR_BIF) != 0U ||
        nx_stm32_timer_break_active(c->break_input, c->break_mask,
                                    c->break_active_high))
        trip(s, NX_ERROR_IO);
}
nx_result_t nx_stm32_power_stage_close(nx_stm32_power_stage_t* s) {
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
    /* A proved hot join is not repeated during fallible cold restoration.
     * Retain configuration and each remaining receipt until final close. */
    s->initialized = false;
    if (s->pwm.owned) {
        nx_result_t result = nx_stm32_waveform_close(&s->pwm);
        if (result != NX_SUCCESS)
            return result;
    }
    if (s->encoder_owned) {
        const nx_stm32_power_stage_config_t* c = s->config;
        RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
        if (s->encoder_touched) {
            if ((NX_AV_READ(&rcc->APB1ENR) & RCC_APB1ENR_TIM4EN) == 0U)
                return NX_ERROR_IO;
            if (!nx_stm32_av_write(&c->encoder->CR1, 0U, UINT32_MAX) ||
                !nx_stm32_av_write(&c->encoder->CCER, 0U, UINT32_MAX) ||
                !nx_stm32_av_write(&c->encoder->DIER, 0U, UINT32_MAX))
                return NX_ERROR_IO;
        }
        if (!nx_stm32_av_bits(&rcc->APB1ENR, RCC_APB1ENR_TIM4EN, 0U))
            return NX_ERROR_IO;
        s->encoder_owned = false;
    }
    nx_result_t result = nx_stm32_control_adc_close(&s->analog);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_stm32_av_irq_priority_restore(ADC_IRQn, s->irq_priority))
        return NX_ERROR_IO;
    *s = (nx_stm32_power_stage_t){0};
    return NX_SUCCESS;
}
const nx_power_stage_ops_t nx_stm32_power_stage_ops = {
    .start = start, .service = service, .stop = stop, .state = state};
