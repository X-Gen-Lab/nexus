/**
 * \file            pwm_adc.c
 * \brief           Finite coherent TIM8 PWM / ADC1 DMA ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/pwm_adc/instance.h"
#include "drivers/timer/stm32f407_timer_output.h"
#include "stm32f4xx_ll_adc.h"
#include "stm32f4xx_ll_tim.h"

#define PWM_ADC_DONE  (1U << 0U)
#define PWM_ADC_ERROR (1U << 1U)

static unsigned channels(uint8_t mask) {
    return (unsigned)(mask & 1U) + (unsigned)((mask >> 1U) & 1U) +
           (unsigned)((mask >> 2U) & 1U);
}

static bool config_valid(const nx_stm32_pwm_adc_config_t* c) {
    if (c == NULL || (uintptr_t)c % _Alignof(nx_stm32_pwm_adc_config_t) != 0U ||
        !nx_stm32_control_adc_valid(&c->analog) || c->pwm.controller != 8U ||
        c->pwm.kind != NX_STM32_WAVE_GROUP || !c->pwm.internal_trigger ||
        c->pwm.master_mode != 7U || c->pwm.sync_mode != 0U ||
        c->pwm.channels > 7U || c->pwm.common.regions == NULL ||
        c->pwm.common.region_count == 0U || c->dma.lane != 4U ||
        c->dma.channel != 0U || c->dma.irq != DMA2_Stream4_IRQn ||
        !nx_stm32_dma_route_valid(&c->dma) || c->pwm.tick_hz == 0U ||
        c->trigger_ticks == 0U || c->trigger_ticks >= c->pwm.period_ticks)
        return false;
#ifndef NEXUS_STM32_MODEL
    if (c->dma.dma != DMA2 || c->dma.stream != DMA2_Stream4)
        return false;
#endif
    const uint16_t cycles[8] = {3U, 15U, 28U, 56U, 84U, 112U, 144U, 480U};
    uint32_t total = 0U;
    for (size_t i = 0U; i < c->analog.channel_count; ++i)
        total += (uint32_t)cycles[c->analog.sample_times[i]] + 12U;
    return (uint64_t)total * c->pwm.tick_hz <
           (uint64_t)c->pwm.period_ticks * c->analog.clock_hz;
}

static bool vector_idle(IRQn_Type irq) {
    return NX_AV_IRQ_ACTIVE(irq) == 0U && NX_AV_IRQ_ENABLED(irq) == 0U &&
           NX_AV_IRQ_PENDING(irq) == 0U;
}

nx_result_t nx_stm32_pwm_adc_admit(const nx_stm32_pwm_adc_state_t* s,
                                   const nx_stm32_pwm_adc_config_t* c) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || (uintptr_t)s % _Alignof(nx_stm32_pwm_adc_state_t) != 0U ||
        !config_valid(c) || nx_stm32_av_overlap(s, sizeof(*s), c, sizeof(*c)))
        return NX_ERROR_INVALID;
    if (s->owned || s->active != NULL)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32_waveform_admit(&s->pwm, &c->pwm);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32_control_adc_admit(&s->analog, &c->analog);
    if (result != NX_SUCCESS)
        return result;
    if ((NX_AV_READ(&c->dma.stream->CR) &
         (DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE |
          DMA_SxCR_DMEIE)) != 0U ||
        (NX_AV_READ(&c->dma.stream->FCR) & DMA_SxFCR_FEIE) != 0U ||
        !vector_idle(c->dma.irq) || !vector_idle(ADC_IRQn))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}

/** \brief The IRQ only suppresses sources; task service proves full release. */
static bool suppress(nx_stm32_pwm_adc_state_t* s) {
    const nx_stm32_pwm_adc_config_t* c = s->config;
    bool ok = nx_stm32_timer_output_stop(c->pwm.registers, true, UINT32_MAX,
                                         UINT32_MAX);
    ok = nx_stm32_av_bits(
             &c->analog.adc->CR2,
             ADC_CR2_EXTEN | ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_ADON, 0U) &&
         ok;
    return ok;
}

static bool live(nx_stm32_pwm_adc_state_t* s) {
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    bool ok = !s->history_lost && g_nx_stm32_system.started &&
              nx_stm32_clock_profile_matches(rcc, NULL) &&
              (NX_AV_READ(&rcc->APB2ENR) &
               (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_TIM8EN)) ==
                  (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_TIM8EN) &&
              (NX_AV_READ(&rcc->APB2RSTR) &
               (RCC_APB2RSTR_ADCRST | RCC_APB2RSTR_TIM8RST)) == 0U &&
              (NX_AV_READ(&rcc->AHB1ENR) & RCC_AHB1ENR_DMA2EN) != 0U &&
              (NX_AV_READ(&rcc->AHB1RSTR) & RCC_AHB1RSTR_DMA2RST) == 0U;
    if (s->active != NULL)
        ok = ok && nx_stm32_av_timebase() &&
             g_nx_stm32_system.timer_error == NX_SUCCESS &&
             s->time_epoch == nx_stm32_av_epoch();
    if (!ok && s->active != NULL)
        s->history_lost = true;
    return ok;
}

/** \brief Program stopped ADC ranks through official sequencer definitions. */
static bool sequence(const nx_stm32_pwm_adc_config_t* c) {
    uint32_t smpr1 = 0U, smpr2 = 0U, sqr1 = 0U, sqr2 = 0U, sqr3 = 0U;
    for (size_t i = 0U; i < c->analog.channel_count; ++i) {
        uint8_t channel = c->analog.channels[i];
        if (channel < 10U)
            smpr2 |= (uint32_t)c->analog.sample_times[i] << (channel * 3U);
        else
            smpr1 |= (uint32_t)c->analog.sample_times[i]
                     << ((channel - 10U) * 3U);
        if (i < 6U)
            sqr3 |= (uint32_t)channel << (i * 5U);
        else if (i < 12U)
            sqr2 |= (uint32_t)channel << ((i - 6U) * 5U);
        else
            sqr1 |= (uint32_t)channel << ((i - 12U) * 5U);
    }
    sqr1 |= (uint32_t)(c->analog.channel_count - 1U) << ADC_SQR1_L_Pos;
    bool ok = nx_stm32_av_write(&c->analog.adc->SMPR1, smpr1, UINT32_MAX);
    ok = nx_stm32_av_write(&c->analog.adc->SMPR2, smpr2, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&c->analog.adc->SQR1, sqr1, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&c->analog.adc->SQR2, sqr2, UINT32_MAX) && ok;
    return nx_stm32_av_write(&c->analog.adc->SQR3, sqr3, UINT32_MAX) && ok;
}

nx_result_t nx_stm32_pwm_adc_prepare(nx_stm32_pwm_adc_state_t* s,
                                     const nx_stm32_pwm_adc_config_t* c) {
    nx_result_t result = nx_stm32_pwm_adc_admit(s, c);
    if (result != NX_SUCCESS)
        return result;
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    if (!g_nx_stm32_system.started ||
        !nx_stm32_clock_profile_matches(rcc, NULL) ||
        (NX_AV_READ(&rcc->AHB1ENR) & RCC_AHB1ENR_DMA2EN) == 0U ||
        (NX_AV_READ(&rcc->AHB1RSTR) & RCC_AHB1RSTR_DMA2RST) != 0U)
        return NX_ERROR_STATE;
    s->config = c;
    s->owned = true;
    s->dma_priority = NX_AV_IRQ_PRIORITY_READ(c->dma.irq);
    s->adc_priority = NX_AV_IRQ_PRIORITY_READ(ADC_IRQn);
    result = nx_stm32_control_adc_prepare(&s->analog, &c->analog);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_stm32_av_write(&c->analog.adc->CR1,
                           c->analog.channel_count > 1U ? (uint32_t)ADC_CR1_SCAN
                                                        : 0U,
                           UINT32_MAX) ||
        !sequence(c))
        return NX_ERROR_IO;
    result = nx_stm32_waveform_prepare(&s->pwm, &c->pwm);
    if (result != NX_SUCCESS)
        return result;
    s->initialized = true;
    return NX_SUCCESS;
}

/** \brief All retained spans participate in alias rejection before admission.
 */
static bool disjoint(const nx_stm32_pwm_adc_state_t* s, const void* p,
                     size_t bytes) {
    const nx_stm32_pwm_adc_config_t* c = s->config;
    if (!nx_stm32_control_adc_disjoint(&c->analog, p, bytes) ||
        nx_stm32_av_overlap(p, bytes, s, sizeof(*s)) ||
        nx_stm32_av_overlap(p, bytes, c, sizeof(*c)) ||
        nx_stm32_av_overlap(p, bytes, c->analog.adc, sizeof(*c->analog.adc)) ||
        nx_stm32_av_overlap(p, bytes, c->analog.common,
                            sizeof(*c->analog.common)) ||
        nx_stm32_av_overlap(p, bytes, c->pwm.registers,
                            sizeof(*c->pwm.registers)) ||
        nx_stm32_av_overlap(p, bytes, c->dma.dma, sizeof(*c->dma.dma)) ||
        nx_stm32_av_overlap(p, bytes, c->dma.stream, sizeof(*c->dma.stream)) ||
        nx_stm32_av_overlap(p, bytes, c->analog.channels,
                            c->analog.channel_count) ||
        nx_stm32_av_overlap(p, bytes, c->analog.sample_times,
                            c->analog.channel_count) ||
        nx_stm32_av_overlap(p, bytes, c->analog.pins,
                            c->analog.channel_count *
                                sizeof(*c->analog.pins)) ||
        nx_stm32_av_overlap(p, bytes, c->pwm.common.pins,
                            c->pwm.common.pin_count *
                                sizeof(*c->pwm.common.pins)) ||
        nx_stm32_av_overlap(p, bytes, c->pwm.common.regions,
                            c->pwm.common.region_count *
                                sizeof(*c->pwm.common.regions)))
        return false;
    for (size_t i = 0U; i < c->pwm.common.pin_count; ++i) {
        const nx_stm32_av_pin_t* pin = &c->pwm.common.pins[i];
        if (nx_stm32_av_overlap(p, bytes, pin->registers,
                                sizeof(*pin->registers)) ||
            nx_stm32_av_overlap(p, bytes, pin->snapshot,
                                sizeof(*pin->snapshot)))
            return false;
    }
    for (size_t i = 0U; i < c->analog.channel_count; ++i) {
        const nx_stm32_av_pin_t* pin = &c->analog.pins[i];
        if (nx_stm32_av_overlap(p, bytes, pin->registers,
                                sizeof(*pin->registers)) ||
            nx_stm32_av_overlap(p, bytes, pin->snapshot,
                                sizeof(*pin->snapshot)))
            return false;
    }
    return true;
}

static nx_result_t request_valid(nx_stm32_pwm_adc_state_t* s,
                                 nx_pwm_adc_request_t* r) {
    const nx_stm32_pwm_adc_config_t* c = s->config;
    if (r == NULL || (uintptr_t)r % _Alignof(nx_pwm_adc_request_t) != 0U ||
        !disjoint(s, r, sizeof(*r)) || r->samples == NULL ||
        r->sample_count == 0U || r->sample_count > UINT16_MAX ||
        r->sample_count % c->analog.channel_count != 0U ||
        r->frame.duty_ticks == NULL ||
        r->frame.channel_count != channels(c->pwm.channels) ||
        r->frame.trigger_ticks != c->trigger_ticks)
        return NX_ERROR_INVALID;
    size_t sample_bytes = r->sample_count * sizeof(uint16_t);
    size_t duty_bytes = r->frame.channel_count * sizeof(uint32_t);
    if (!disjoint(s, r->samples, sample_bytes) ||
        !disjoint(s, r->frame.duty_ticks, duty_bytes) ||
        nx_stm32_av_overlap(r, sizeof(*r), r->samples, sample_bytes) ||
        nx_stm32_av_overlap(r, sizeof(*r), r->frame.duty_ticks, duty_bytes) ||
        nx_stm32_av_overlap(r->samples, sample_bytes, r->frame.duty_ticks,
                            duty_bytes))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < r->frame.channel_count; ++i)
        if (r->frame.duty_ticks[i] > c->pwm.period_ticks)
            return NX_ERROR_INVALID;
    return nx_dma_storage_validate(
        c->pwm.common.regions, c->pwm.common.region_count, r->samples,
        sample_bytes, sample_bytes, _Alignof(uint16_t), 0U, NX_DMA_FROM_DEVICE);
}

static nx_result_t service(void* context);

static nx_result_t submit(void* context, nx_pwm_adc_request_t* r) {
    nx_stm32_pwm_adc_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized || s->stopping || s->fault ||
        s->quarantined)
        return NX_ERROR_STATE;
    if (s->active != NULL)
        return NX_ERROR_BUSY;
    nx_result_t result = request_valid(s, r);
    if (result != NX_SUCCESS)
        return result;
    if (nx_request_state(&r->base) != NX_REQUEST_READY || !live(s))
        return NX_ERROR_STATE;
    nx_time_us_t now = NX_AV_NOW();
    if (r->base.deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (now >= r->base.deadline)
        return NX_ERROR_TIMEOUT;
    nx_time_us_t ready;
    result = nx_deadline_finite_after(now, 4U, &ready);
    if (result != NX_SUCCESS)
        return result;
    const nx_stm32_pwm_adc_config_t* c = s->config;
    if ((NX_AV_READ(&c->dma.stream->CR) & DMA_SxCR_EN) != 0U ||
        (NX_AV_READ(&c->analog.adc->CR2) & ADC_CR2_ADON) != 0U ||
        (NX_AV_READ(&c->pwm.registers->CR1) & TIM_CR1_CEN) != 0U)
        return NX_ERROR_BUSY;
    result = nx_request_admit(&r->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;
    s->active = r;
    s->time_epoch = nx_stm32_av_epoch();
    s->result = NX_SUCCESS;
    __atomic_store_n(&s->events, 0U, __ATOMIC_RELEASE);
    s->ready_at = ready;
    s->warming = true;
    result = nx_stm32_pwm_group_ops.commit(&s->pwm, &r->frame);
    if (result != NX_SUCCESS) {
        s->result = result;
        s->fault = true;
        (void)service(s);
        return NX_SUCCESS;
    }
    nx_stm32_av_dma_ack(&c->dma, 0x3dU);
    NX_AV_WRITE(&c->analog.adc->SR, 0U);
    bool ok =
        nx_stm32_av_write(&c->dma.stream->PAR,
                          (uint32_t)(uintptr_t)&c->analog.adc->DR, UINT32_MAX);
    ok = nx_stm32_av_write(&c->dma.stream->M0AR,
                           (uint32_t)(uintptr_t)r->samples, UINT32_MAX) &&
         ok;
    ok = nx_stm32_av_write(&c->dma.stream->M1AR, 0U, UINT32_MAX) && ok;
    ok = nx_stm32_av_write(&c->dma.stream->NDTR, (uint32_t)r->sample_count,
                           UINT32_MAX) &&
         ok;
    ok = nx_stm32_av_write(&c->dma.stream->FCR, DMA_SxFCR_FEIE,
                           DMA_SxFCR_FEIE | DMA_SxFCR_DMDIS) &&
         ok;
    uint32_t dma = DMA_SxCR_MINC | DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0 |
                   DMA_SxCR_PL_1 | DMA_SxCR_TCIE | DMA_SxCR_TEIE |
                   DMA_SxCR_DMEIE;
    ok = nx_stm32_av_write(&c->dma.stream->CR, dma, UINT32_MAX) && ok;
    s->dma_owned = true;
    s->adc_irq_owned = true;
    NX_AV_IRQ_PRIORITY(c->dma.irq, c->pwm.common.priority);
    NX_AV_IRQ_PRIORITY(ADC_IRQn, c->pwm.common.priority);
    NX_AV_IRQ_CLEAR(c->dma.irq);
    NX_AV_IRQ_CLEAR(ADC_IRQn);
    NX_AV_IRQ_ENABLE(c->dma.irq);
    NX_AV_IRQ_ENABLE(ADC_IRQn);
    ok = nx_stm32_av_bits(&c->analog.adc->CR1, ADC_CR1_OVRIE, ADC_CR1_OVRIE) &&
         ok;
    ok = nx_stm32_av_bits(&c->dma.stream->CR, DMA_SxCR_EN, DMA_SxCR_EN) && ok;
    /* MMS remains ENABLE with a stopped counter until warmup completes.
     * OC4REF is low at CNT=0; enabling its rising route cannot emit UG. */
    ok = nx_stm32_av_write(&c->analog.adc->CR2,
                           ADC_CR2_ADON | ADC_CR2_DMA | ADC_CR2_DDS |
                               (14U << ADC_CR2_EXTSEL_Pos),
                           UINT32_MAX) &&
         ok;
    if (!ok) {
        s->result = NX_ERROR_IO;
        s->fault = true;
        (void)service(s);
    }
    return NX_SUCCESS;
}

/** \brief Never publish completion until outputs, requests and IRQs are quiet.
 */
static nx_result_t drain(nx_stm32_pwm_adc_state_t* s) {
    const nx_stm32_pwm_adc_config_t* c = s->config;
    bool quiet = suppress(s);
    nx_result_t timer = nx_stm32_pwm_group_ops.stop(&s->pwm);
    quiet = timer == NX_SUCCESS && quiet;
    quiet = nx_stm32_av_bits(&c->analog.adc->CR1, ADC_CR1_OVRIE, 0U) && quiet;
    if (s->dma_owned) {
        quiet =
            nx_stm32_av_dma_detach(&c->dma, c->pwm.common.poll_limit) && quiet;
        if (quiet)
            quiet = nx_stm32_av_dma_addresses(&c->dma);
    }
    if (s->adc_irq_owned)
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
    s->dma_owned = false;
    s->adc_irq_owned = false;
    s->quarantined = false;
    s->warming = false;
    s->running = false;
    nx_pwm_adc_request_t* r = s->active;
    nx_result_t result = s->result;
    size_t transferred =
        result == NX_SUCCESS && r != NULL ? r->sample_count : 0U;
    s->active = NULL;
    if (r != NULL)
        nx_request_settle(&r->base, result, transferred);
    return result;
}

static nx_result_t service(void* context) {
    nx_stm32_pwm_adc_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized)
        return NX_ERROR_STATE;
    if (!live(s)) {
        s->fault = true;
        s->quarantined = s->active != NULL;
        if (s->active != NULL) {
            /* Suppression after a lost clock proof is useful but establishes
             * neither memory withdrawal nor permission to release the loan. */
            (void)suppress(s);
            (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
            (void)nx_request_transition(&s->active->base,
                                        NX_REQUEST_QUARANTINED);
        }
        return NX_ERROR_STATE;
    }
    if (s->active == NULL)
        return NX_SUCCESS;
    nx_stm32_pwm_adc_dma_irq(s);
    nx_stm32_pwm_adc_adc_irq(s);
    nx_stm32_pwm_adc_break_irq(s);
    if (s->result == NX_SUCCESS && NX_AV_NOW() >= s->active->base.deadline)
        s->result = NX_ERROR_TIMEOUT;
    if ((__atomic_load_n(&s->events, __ATOMIC_ACQUIRE) & PWM_ADC_ERROR) != 0U ||
        s->pwm.fault) {
        s->result = NX_ERROR_IO;
        s->fault = true;
    }
    if (s->result != NX_SUCCESS ||
        (__atomic_load_n(&s->events, __ATOMIC_ACQUIRE) & PWM_ADC_DONE) != 0U)
        return drain(s);
    if (s->warming && NX_AV_NOW() >= s->ready_at) {
        const nx_stm32_pwm_adc_config_t* c = s->config;
        bool ok = nx_stm32_av_bits(&c->analog.adc->CR2, ADC_CR2_EXTEN,
                                   ADC_CR2_EXTEN_0);
        nx_result_t result =
            ok ? nx_stm32_pwm_group_ops.start(&s->pwm) : NX_ERROR_IO;
        s->warming = false;
        s->running = result == NX_SUCCESS;
        if (result != NX_SUCCESS) {
            s->result = result;
            s->fault = true;
            return drain(s);
        }
    }
    return NX_SUCCESS;
}

static nx_result_t cancel(void* context, nx_pwm_adc_request_t* r) {
    nx_stm32_pwm_adc_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || r == NULL || s->active != r)
        return NX_ERROR_STATE;
    if (s->result == NX_SUCCESS)
        s->result = NX_ERROR_CANCELLED;
    return NX_SUCCESS;
}

static nx_result_t stop(void* context) {
    nx_stm32_pwm_adc_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized || !live(s))
        return NX_ERROR_STATE;
    s->stopping = true;
    if (s->active != NULL && s->result == NX_SUCCESS)
        s->result = NX_ERROR_CANCELLED;
    nx_result_t result = drain(s);
    return s->active == NULL && !s->quarantined ? NX_SUCCESS : result;
}

static nx_result_t state(const void* context, nx_pwm_adc_state_t* out) {
    const nx_stm32_pwm_adc_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized || out == NULL ||
        (uintptr_t)out % _Alignof(nx_pwm_adc_state_t) != 0U ||
        !disjoint(s, out, sizeof(*out)))
        return NX_ERROR_INVALID;
    if (s->active != NULL) {
        const nx_pwm_adc_request_t* r = s->active;
        if (nx_stm32_av_overlap(out, sizeof(*out), r, sizeof(*r)) ||
            nx_stm32_av_overlap(out, sizeof(*out), r->samples,
                                r->sample_count * sizeof(*r->samples)) ||
            nx_stm32_av_overlap(out, sizeof(*out), r->frame.duty_ticks,
                                r->frame.channel_count * sizeof(uint32_t)))
            return NX_ERROR_INVALID;
    }
    const nx_stm32_pwm_adc_config_t* c = s->config;
    *out = (nx_pwm_adc_state_t){
        .format = {.sample_stride = sizeof(uint16_t),
                   .scan_stride = sizeof(uint16_t) * c->analog.channel_count,
                   .trigger_identity = 8U,
                   .channel_count = (uint16_t)c->analog.channel_count,
                   .storage_bytes = sizeof(uint16_t),
                   .significant_bits = 12U,
                   .encoding = NX_ADC_UNSIGNED,
                   .byte_order = NX_ADC_LITTLE_ENDIAN,
                   .group = NX_ADC_REGULAR,
                   .trigger = NX_ADC_TRIGGER_TIMER},
        .tick_hz = c->pwm.tick_hz,
        .period_ticks = c->pwm.period_ticks,
        .trigger_ticks = c->trigger_ticks,
        .deadtime_ticks = c->pwm.deadtime_ticks,
        .deadtime_hz = c->pwm.timer_hz,
        .pwm_channels = channels(c->pwm.channels),
        .max_samples = UINT16_MAX - UINT16_MAX % c->analog.channel_count,
        .running = s->running,
        .fault = s->fault,
        .quarantined = s->quarantined};
    return NX_SUCCESS;
}

void nx_stm32_pwm_adc_dma_irq(nx_stm32_pwm_adc_state_t* s) {
    if (s == NULL || !s->owned || s->active == NULL)
        return;
    uint32_t flags = nx_stm32_av_dma_flags(&s->config->dma);
    if ((flags & 0x0dU) != 0U)
        (void)__atomic_fetch_or(&s->events, PWM_ADC_ERROR, __ATOMIC_RELEASE);
    if ((flags & (1U << 5U)) != 0U) {
        if (NX_AV_READ(&s->config->dma.stream->NDTR) == 0U)
            (void)__atomic_fetch_or(&s->events, PWM_ADC_DONE, __ATOMIC_RELEASE);
        else
            (void)__atomic_fetch_or(&s->events, PWM_ADC_ERROR,
                                    __ATOMIC_RELEASE);
    }
    nx_stm32_av_dma_ack(&s->config->dma, flags);
    if (__atomic_load_n(&s->events, __ATOMIC_ACQUIRE) != 0U && !suppress(s))
        (void)__atomic_fetch_or(&s->events, PWM_ADC_ERROR, __ATOMIC_RELEASE);
}

void nx_stm32_pwm_adc_adc_irq(nx_stm32_pwm_adc_state_t* s) {
    if (s == NULL || !s->owned || s->active == NULL)
        return;
    if ((NX_AV_READ(&s->config->analog.adc->SR) & ADC_SR_OVR) != 0U) {
        (void)__atomic_fetch_or(&s->events, PWM_ADC_ERROR, __ATOMIC_RELEASE);
        (void)suppress(s);
    }
}

void nx_stm32_pwm_adc_break_irq(nx_stm32_pwm_adc_state_t* s) {
    if (s == NULL || !s->owned || s->active == NULL)
        return;
    const nx_stm32_waveform_config_t* c = &s->config->pwm;
    if ((NX_AV_READ(&c->registers->SR) & TIM_SR_BIF) != 0U ||
        nx_stm32_timer_break_active(c->break_input, c->break_mask,
                                    c->break_active_high)) {
        (void)__atomic_fetch_or(&s->events, PWM_ADC_ERROR, __ATOMIC_RELEASE);
        (void)suppress(s);
    }
}

nx_result_t nx_stm32_pwm_adc_close(nx_stm32_pwm_adc_state_t* s) {
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
    const nx_stm32_pwm_adc_config_t* c = s->config;
    if (s->pwm.owned) {
        nx_result_t result = nx_stm32_waveform_close(&s->pwm);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_result_t analog = nx_stm32_control_adc_close(&s->analog);
    if (analog != NX_SUCCESS)
        return analog;
    if (!nx_stm32_av_irq_priority_restore(c->dma.irq, s->dma_priority) ||
        !nx_stm32_av_irq_priority_restore(ADC_IRQn, s->adc_priority))
        return NX_ERROR_IO;
    /* All source, vector and cold receipts are now returned. Only this point
     * may retire terminal flags and reset the same generated static identity.
     */
    *s = (nx_stm32_pwm_adc_state_t){0};
    return NX_SUCCESS;
}

const nx_pwm_adc_ops_t nx_stm32_pwm_adc_ops = {.submit = submit,
                                               .cancel = cancel,
                                               .service = service,
                                               .stop = stop,
                                               .state = state};
