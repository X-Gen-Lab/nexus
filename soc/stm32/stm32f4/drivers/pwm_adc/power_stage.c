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

static uint32_t adc_gate(const nx_stm32_power_stage_config_t* c);
static uint32_t timer_gate(const nx_stm32_power_stage_config_t* c);
static size_t domain_index(const nx_stm32_power_stage_domain_t* d,
                           const nx_stm32_power_stage_t* s);

static bool domain_live(const nx_stm32_power_stage_domain_t* d);
static bool time_shared(const nx_stm32_power_stage_config_t* c) {
#if NEXUS_STM32_POWER_STAGE_TDM
    return c->domain != NULL && c->domain->config != NULL &&
           c->domain->config->time_shared;
#else
    (void)c;
    return false;
#endif
}
static bool tdm_withdraw(nx_stm32_power_stage_t* s);
static void tdm_abort(nx_stm32_power_stage_domain_t* d, nx_result_t reason);
static uint32_t converter_resources(const nx_stm32_power_stage_config_t* c) {
    uint32_t gates = adc_gate(c);
#if NEXUS_STM32_POWER_STAGE_PAIR
    if (c->pair != NULL)
        gates |= RCC_APB2ENR_ADC2EN;
#endif
    return gates;
}
static uint32_t common_mode(const nx_stm32_power_stage_config_t* c) {
#if NEXUS_STM32_POWER_STAGE_PAIR
    return c->pair != NULL ? LL_ADC_MULTI_DUAL_INJ_SIMULT : 0U;
#else
    (void)c;
    return 0U;
#endif
}
static uint32_t fault(const nx_stm32_power_stage_t* s) {
    return __atomic_load_n(&s->fault, __ATOMIC_ACQUIRE);
}
static uint32_t ticks_per_us(const nx_stm32_power_stage_config_t* c) {
    return c->pwm.tick_hz / 1000000U;
}
static IRQn_Type update_irq(const nx_stm32_power_stage_config_t* c) {
    return c->pwm.controller == 1U ? TIM1_UP_TIM10_IRQn : TIM8_UP_TIM13_IRQn;
}
static uint32_t carrier_phase(const nx_stm32_power_stage_config_t* c) {
    uint32_t count = NX_AV_READ(&c->pwm.registers->CNT);
    if (c->pwm.center_aligned &&
        (NX_AV_READ(&c->pwm.registers->CR1) & TIM_CR1_DIR) != 0U)
        return c->pwm.period_ticks - count;
    return count;
}
static uint32_t conversion_us(const nx_stm32_power_stage_config_t* c) {
    const uint16_t cycles[8] = {3U, 15U, 28U, 56U, 84U, 112U, 144U, 480U};
    uint64_t total = 0U;
    for (size_t i = 0U; i < c->analog.channel_count; ++i) {
        const uint64_t converted =
            (uint64_t)cycles[c->analog.sample_times[i]] + 12U;
        if (c->pair == NULL)
            total += converted;
        else if (converted > total)
            total = converted;
    }
    return (uint32_t)((total * 1000000U + c->analog.clock_hz - 1U) /
                      c->analog.clock_hz);
}
static bool dc_profile(const nx_stm32_power_stage_config_t* c) {
#if NEXUS_STM32_POWER_STAGE_DC
    return c->dc != NULL;
#else
    (void)c;
    return false;
#endif
}
static bool uses_update(const nx_stm32_power_stage_config_t* c) {
    return c->pwm.center_aligned || dc_profile(c);
}
static uint32_t observation_tick(const nx_stm32_power_stage_t* s) {
#if NEXUS_STM32_POWER_STAGE_DC
    if (dc_profile(s->config) && s->config->dc->state->armed)
        return s->config->dc->state->capture
            .sample_ticks[s->config->dc->state->index];
#endif
    return s->staged_trigger_ticks;
}
static bool config_valid(const nx_stm32_power_stage_config_t* c) {
    if (c == NULL ||
        (uintptr_t)c % _Alignof(nx_stm32_power_stage_config_t) != 0U ||
        !nx_stm32_control_adc_valid(&c->analog) ||
        (c->dc == NULL &&
         ((c->current_count != 2U && c->current_count != 3U) ||
          c->analog.channel_count !=
              (size_t)c->current_count + (c->sample_bus ? 1U : 0U) ||
          (c->current_count == 3U
               ? c->current_phase_mask != 7U
               : (c->current_phase_mask != 3U && c->current_phase_mask != 5U &&
                  c->current_phase_mask != 6U)))) ||
        c->adc_index > 2U || (c->trigger_selector & ~ADC_CR2_JEXTSEL) != 0U ||
        ((c->encoder == NULL) != (c->encoder_pins == NULL)) ||
        c->encoder_filter > 15U ||
        (c->pwm.controller != 1U && c->pwm.controller != 8U) ||
        c->pwm.kind != NX_STM32_WAVE_GROUP || c->pwm.channels != 7U ||
        !c->pwm.complementary || !c->pwm.internal_trigger ||
        c->pwm.master_mode != 7U || c->pwm.sync_mode != 0U ||
        c->pwm.timer_hz == 0U || c->pwm.tick_hz < 1000000U ||
        c->pwm.tick_hz % 1000000U != 0U || c->pwm.period_ticks > UINT16_MAX ||
        c->minimum_duty_ticks == 0U ||
        c->minimum_duty_ticks >= c->maximum_duty_ticks ||
        c->maximum_duty_ticks >= c->pwm.period_ticks ||
        c->settling_ticks == 0U || c->max_irq_delay_us == 0U ||
        c->compute_budget_us == 0U ||
        c->phase_offset_ticks >= c->pwm.period_ticks ||
        (c->phase_offset_ticks != 0U &&
         (!c->pwm.center_aligned || c->domain == NULL)))
        return false;
    if (c->dc != NULL) {
#if NEXUS_STM32_POWER_STAGE_DC
        const nx_stm32_power_stage_dc_config_t* dc = c->dc;
        if (c->pair != NULL || c->pwm.center_aligned ||
            !c->pwm.immediate_trigger || c->phase_offset_ticks != 0U ||
            c->adc_index != 0U || c->current_count != 0U ||
            c->current_phase_mask != 0U || c->sample_bus ||
            c->analog.channel_count != 1U ||
            !nx_stm32_single_shunt_valid(&dc->capture) ||
            dc->capture.timer != c->pwm.registers ||
            dc->capture.adc != c->analog.adc ||
            dc->capture.channel != c->analog.channels[0] ||
            dc->capture.sample_time != c->analog.sample_times[0] ||
            dc->capture.trigger_selector != c->trigger_selector ||
            dc->capture.period_ticks != c->pwm.period_ticks)
            return false;
#else
        return false;
#endif
    } else if (c->pwm.immediate_trigger) {
        return false;
    }
    if (c->pair != NULL) {
#if NEXUS_STM32_POWER_STAGE_PAIR
        if (!nx_stm32_adc_pair_valid(c->pair) || c->domain == NULL ||
            c->adc_index != 0U || c->current_count != 2U || c->sample_bus ||
            c->pair->master != c->analog.adc ||
            c->pair->slave != c->analog.peers[0] ||
            c->pair->common != c->analog.common ||
            c->pair->channels[0] != c->analog.channels[0] ||
            c->pair->channels[1] != c->analog.channels[1] ||
            c->pair->sample_times[0] != c->analog.sample_times[0] ||
            c->pair->sample_times[1] != c->analog.sample_times[1])
            return false;
#else
        return false;
#endif
    }
#ifndef NEXUS_STM32_MODEL
    if (c->encoder != NULL && c->encoder != TIM4)
        return false;
    ADC_TypeDef* converters[3] = {ADC1, ADC2, ADC3};
    if (c->analog.adc != converters[c->adc_index])
        return false;
#endif
    for (size_t i = 0U; c->encoder != NULL && i < 2U; ++i) {
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
    if (c->pwm.period_ticks < 4U * unit || c->pwm.period_ticks % unit != 0U ||
        c->phase_offset_ticks % unit != 0U ||
        c->commit_before_ticks > c->pwm.period_ticks - 2U * unit)
        return false;
    uint64_t deadtime = ((uint64_t)c->pwm.deadtime_ticks * c->pwm.tick_hz +
                         c->pwm.timer_hz - 1U) /
                        c->pwm.timer_hz;
    uint64_t conversion = (uint64_t)conversion_us(c) * unit;
    bool window;
    if (dc_profile(c)) {
#if NEXUS_STM32_POWER_STAGE_DC
        window = c->dc->settling_ticks == deadtime + c->settling_ticks &&
                 c->dc->aperture_ticks == conversion &&
                 c->trigger_ticks > c->dc->capture.rearm_guard_ticks;
#else
        window = false;
#endif
    } else if (c->pwm.center_aligned) {
        window = (c->pwm.period_ticks & 1U) == 0U &&
                 c->trigger_ticks >
                     c->pwm.period_ticks / 2U +
                         ((uint64_t)c->max_irq_delay_us + 2U) * unit &&
                 (uint64_t)c->maximum_duty_ticks / 2U + deadtime +
                         c->settling_ticks <
                     c->trigger_ticks &&
                 (uint64_t)c->trigger_ticks + conversion + c->settling_ticks <
                     c->pwm.period_ticks - c->maximum_duty_ticks / 2U;
    } else {
        window =
            (uint64_t)c->maximum_duty_ticks + deadtime + c->settling_ticks <
                c->trigger_ticks &&
            (uint64_t)c->trigger_ticks + conversion < c->pwm.period_ticks;
    }
    return window &&
           (uint64_t)c->trigger_ticks + conversion +
                   ((uint64_t)c->max_irq_delay_us + c->compute_budget_us + 4U) *
                       unit <
               c->commit_before_ticks;
}
/** \brief Selected mode storage cannot overwrite any static hardware owner. */
static bool dc_storage_valid(const nx_stm32_power_stage_t* s,
                             const nx_stm32_power_stage_config_t* c) {
#if NEXUS_STM32_POWER_STAGE_DC
    if (c->dc != NULL) {
        const nx_stm32_power_stage_dc_config_t* dc = c->dc;
        const size_t bytes = sizeof(*dc->state);
        if (dc->state == NULL ||
            (uintptr_t)dc->state % _Alignof(nx_stm32_power_stage_dc_t) != 0U ||
            bytes > UINTPTR_MAX - (uintptr_t)dc->state ||
            nx_stm32_av_overlap(dc->state, bytes, s, sizeof(*s)) ||
            nx_stm32_av_overlap(dc->state, bytes, c, sizeof(*c)) ||
            nx_stm32_av_overlap(dc->state, bytes, dc, sizeof(*dc)) ||
            nx_stm32_av_overlap(s, sizeof(*s), dc, sizeof(*dc)) ||
            !nx_stm32_control_adc_disjoint(&c->analog, dc->state, bytes) ||
            nx_stm32_av_overlap(dc->state, bytes, c->pwm.registers,
                                sizeof(*c->pwm.registers)) ||
            (c->domain != NULL &&
             nx_stm32_av_overlap(dc->state, bytes, c->domain,
                                 sizeof(*c->domain))))
            return false;
    }
#else
    (void)s;
    (void)c;
#endif
    return true;
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
        !config_valid(c) || !dc_storage_valid(s, c) ||
        nx_stm32_av_overlap(s, sizeof(*s), c, sizeof(*c)))
        return NX_ERROR_INVALID;
    if (s->owned || s->active != NULL)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32_waveform_admit(&s->pwm, &c->pwm);
    if (result == NX_SUCCESS && c->domain == NULL)
        result = nx_stm32_control_adc_admit(&s->analog, &c->analog);
    if (result == NX_SUCCESS && c->domain != NULL && c->domain->owned &&
        (domain_index(c->domain, s) == c->domain->config->count ||
         (NX_AV_READ(&c->analog.adc->CR2) & ADC_CR2_ADON) != 0U))
        result = NX_ERROR_BUSY;
    if (result != NX_SUCCESS)
        return result;
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    bool center_peer_busy = false;
    if (uses_update(c)) {
        center_peer_busy =
            c->pwm.controller == 1U
                ? ((NX_AV_READ(&rcc->APB2ENR) & RCC_APB2ENR_TIM10EN) != 0U ||
                   (NX_AV_READ(&rcc->APB2RSTR) & RCC_APB2RSTR_TIM10RST) != 0U)
                : ((NX_AV_READ(&rcc->APB1ENR) & RCC_APB1ENR_TIM13EN) != 0U ||
                   (NX_AV_READ(&rcc->APB1RSTR) & RCC_APB1RSTR_TIM13RST) != 0U);
    }
    if (center_peer_busy || (uses_update(c) && !irq_idle(update_irq(c))) ||
        (c->domain == NULL && !irq_idle(ADC_IRQn)) ||
        (c->encoder != NULL &&
         ((NX_AV_READ(&rcc->APB1ENR) & RCC_APB1ENR_TIM4EN) != 0U ||
          (NX_AV_READ(&rcc->APB1RSTR) & RCC_APB1RSTR_TIM4RST) != 0U ||
          !irq_idle(TIM4_IRQn))))
        return NX_ERROR_BUSY;
    for (size_t i = 0U; c->encoder != NULL && i < 2U; ++i)
        if ((NX_AV_READ(&rcc->AHB1ENR) & RCC_AHB1ENR_GPIODEN) != 0U &&
            (NX_AV_READ(&c->encoder_pins[i].registers->LCKR) &
             (1U << c->encoder_pins[i].pin)) != 0U)
            return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
static uint8_t phase_count(uint8_t mask) {
    return (uint8_t)((mask & 1U) + ((mask >> 1U) & 1U) + ((mask >> 2U) & 1U));
}
static uint32_t capture_sequence(const nx_stm32_power_stage_config_t* c,
                                 uint8_t mask) {
    const size_t count = (size_t)phase_count(mask) + (c->sample_bus ? 1U : 0U);
    uint32_t result = ((uint32_t)count - 1U) << ADC_JSQR_JL_Pos;
    size_t source = 0U, rank = 0U;
    for (size_t phase_index = 0U; phase_index < 3U; ++phase_index) {
        if ((c->current_phase_mask & (1U << phase_index)) == 0U)
            continue;
        if ((mask & (1U << phase_index)) != 0U) {
            result |= (uint32_t)c->analog.channels[source]
                      << ((4U - count + rank) * 5U);
            ++rank;
        }
        ++source;
    }
    if (c->sample_bus)
        result |= (uint32_t)c->analog.channels[c->current_count] << 15U;
    return result;
}
static bool sequence(const nx_stm32_power_stage_config_t* c) {
    uint32_t smpr = 0U;
    uint32_t ranks = ((uint32_t)c->analog.channel_count - 1U)
                     << ADC_JSQR_JL_Pos;
    uint32_t smpr_high = 0U;
    for (size_t i = 0U; i < c->analog.channel_count; ++i) {
        uint32_t channel = c->analog.channels[i];
        if (channel < 10U)
            smpr |= (uint32_t)c->analog.sample_times[i] << (channel * 3U);
        else
            smpr_high |= (uint32_t)c->analog.sample_times[i]
                         << ((channel - 10U) * 3U);
        ranks |= channel << ((4U - c->analog.channel_count + i) * 5U);
    }
    bool ok = nx_stm32_av_write(&c->analog.adc->SMPR1, smpr_high, UINT32_MAX);
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
    if (c->encoder == NULL)
        return NX_SUCCESS;
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
    if (uses_update(c))
        s->update_irq_priority = NX_AV_IRQ_PRIORITY_READ(update_irq(c));
    if (c->domain == NULL) {
        result = nx_stm32_control_adc_prepare(&s->analog, &c->analog);
    } else {
        if (!c->domain->owned || c->domain->config == NULL ||
            (NX_AV_READ(&g_nx_stm32_system.rcc->APB2ENR) & adc_gate(c)) == 0U)
            return NX_ERROR_STATE;
        s->analog.config = &c->analog;
        s->analog.touched = true;
        nx_stm32_av_config_t pads = {c->analog.pins,
                                     c->analog.channel_count,
                                     NULL,
                                     0U,
                                     c->adc_index + 1U,
                                     0U};
        result = nx_stm32_av_write(&c->analog.adc->CR1, 0U, UINT32_MAX) &&
                         nx_stm32_av_write(&c->analog.adc->CR2, 0U, UINT32_MAX)
                     ? nx_stm32_av_pins(&pads)
                     : NX_ERROR_IO;
    }
    if (result != NX_SUCCESS)
        return result;
    if (!dc_profile(c) && c->pair == NULL) {
        if (!sequence(c))
            return NX_ERROR_IO;
    }
#if NEXUS_STM32_POWER_STAGE_PAIR
    else if (c->pair != NULL) {
        result = nx_stm32_adc_pair_prepare(c->pair);
        if (result != NX_SUCCESS)
            return result;
    }
#endif
    result = encoder_prepare(s);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32_waveform_prepare(&s->pwm, &c->pwm);
#if NEXUS_STM32_POWER_STAGE_DC
    if (result == NX_SUCCESS && dc_profile(c))
        result = nx_stm32_power_stage_dc_prepare(c->dc);
#endif
    if (result == NX_SUCCESS)
        s->initialized = true;
    return result;
}
static bool live(nx_stm32_power_stage_t* s) {
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    bool ok = !s->history_lost && g_nx_stm32_system.started &&
              nx_stm32_clock_profile_matches(rcc, NULL) &&
              (NX_AV_READ(&rcc->APB2ENR) &
               (adc_gate(s->config) | timer_gate(s->config))) ==
                  (adc_gate(s->config) | timer_gate(s->config)) &&
              (NX_AV_READ(&rcc->APB2RSTR) &
               (RCC_APB2RSTR_ADCRST | timer_gate(s->config))) == 0U &&
              (s->config->encoder == NULL ||
               ((NX_AV_READ(&rcc->APB1ENR) & RCC_APB1ENR_TIM4EN) != 0U &&
                (NX_AV_READ(&rcc->APB1RSTR) & RCC_APB1RSTR_TIM4RST) == 0U));
    if (s->config->domain != NULL)
        ok = ok && domain_live(s->config->domain);
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
    if ((gates & timer_gate(c)) != 0U)
        quiet = nx_stm32_timer_output_stop(c->pwm.registers, true, UINT32_MAX,
                                           UINT32_MAX);
    if (time_shared(c))
        return tdm_withdraw(s) && quiet;
    bool slave_quiet = true;
#if NEXUS_STM32_POWER_STAGE_PAIR
    if (c->pair != NULL) {
        if ((gates & RCC_APB2ENR_ADC2EN) == 0U) {
            slave_quiet = false;
        } else {
            slave_quiet = nx_stm32_av_bits(&c->pair->slave->CR1,
                                           ADC_CR1_JEOCIE | ADC_CR1_OVRIE, 0U);
            slave_quiet = nx_stm32_av_bits(&c->pair->slave->CR2,
                                           ADC_CR2_JEXTEN | ADC_CR2_ADON, 0U) &&
                          slave_quiet;
        }
    }
#endif
    if ((gates & adc_gate(c)) == 0U)
        return false;
#if NEXUS_STM32_POWER_STAGE_DC
    if (dc_profile(c)) {
        const bool analog_quiet =
            nx_stm32_power_stage_dc_disable(c->dc) == NX_SUCCESS;
        return quiet && analog_quiet;
    }
#endif
    bool analog = nx_stm32_av_bits(&c->analog.adc->CR1,
                                   ADC_CR1_JEOCIE | ADC_CR1_OVRIE, 0U);
    analog = nx_stm32_av_bits(&c->analog.adc->CR2,
                              ADC_CR2_JEXTEN | ADC_CR2_ADON, 0U) &&
             analog;
    return quiet && analog && slave_quiet;
}
static bool domain_live(const nx_stm32_power_stage_domain_t* d) {
    if (!d->owned || d->config == NULL ||
        (NX_AV_READ(&g_nx_stm32_system.rcc->APB2ENR) & d->clocks) != d->clocks)
        return false;
    const uint32_t mask =
        ADC_CCR_MULTI | ADC_CCR_DMA | ADC_CCR_DDS | ADC_CCR_ADCPRE;
    const uint32_t expected =
        ((NEXUS_CLOCK_ADC_DIV / 2U - 1U) << ADC_CCR_ADCPRE_Pos) |
        common_mode(d->config->configurations[0]);
    return (NX_AV_READ(&d->config->configurations[0]->analog.common->CCR) &
            mask) == expected;
}
static bool tdm_withdraw(nx_stm32_power_stage_t* s) {
#if NEXUS_STM32_POWER_STAGE_TDM
    nx_stm32_power_stage_domain_t* d = s->config->domain;
    if (__atomic_load_n(&d->capture_owner, __ATOMIC_ACQUIRE) != s)
        return true;
    const nx_stm32_power_stage_config_t* c = s->config;
    if ((NX_AV_READ(&g_nx_stm32_system.rcc->APB2ENR) & adc_gate(c)) == 0U)
        return false;
    bool ok = nx_stm32_av_bits(&c->analog.adc->CR1, ADC_CR1_JEOCIE, 0U);
    ok = nx_stm32_av_bits(&c->analog.adc->CR2, ADC_CR2_JEXTEN, 0U) && ok;
    const uint32_t status = NX_AV_READ(&c->analog.adc->SR);
    if ((status & ADC_SR_JSTRT) != 0U && (status & ADC_SR_JEOC) == 0U)
        return false;
    NX_AV_WRITE(&c->analog.adc->SR,
                ~((uint32_t)ADC_SR_JSTRT | (uint32_t)ADC_SR_JEOC));
    if ((NX_AV_READ(&c->analog.adc->SR) & (ADC_SR_JSTRT | ADC_SR_JEOC)) != 0U)
        return false;
    if (ok) {
        __atomic_store_n(&d->capture_owner, NULL, __ATOMIC_RELEASE);
        s->capture_armed = false;
        s->overflow_seen = false;
    }
    return ok;
#else
    (void)s;
    return false;
#endif
}
static void tdm_abort(nx_stm32_power_stage_domain_t* d, nx_result_t reason) {
#if NEXUS_STM32_POWER_STAGE_TDM
    for (size_t i = 0U; i < d->config->count; ++i) {
        nx_stm32_power_stage_t* peer = d->config->members[i];
        if (peer->active == NULL)
            continue;
        uint32_t empty = NX_SUCCESS;
        (void)__atomic_compare_exchange_n(&peer->fault, &empty,
                                          (uint32_t)reason, false,
                                          __ATOMIC_RELEASE, __ATOMIC_RELAXED);
        if ((NX_AV_READ(&g_nx_stm32_system.rcc->APB2ENR) &
             timer_gate(peer->config)) != 0U)
            (void)nx_stm32_timer_output_stop(peer->config->pwm.registers, true,
                                             UINT32_MAX, UINT32_MAX);
    }
    const nx_stm32_power_stage_config_t* c = d->config->configurations[0];
    if ((NX_AV_READ(&g_nx_stm32_system.rcc->APB2ENR) & adc_gate(c)) != 0U) {
        bool ok = nx_stm32_av_bits(&c->analog.adc->CR1,
                                   ADC_CR1_JEOCIE | ADC_CR1_OVRIE, 0U);
        ok = nx_stm32_av_bits(&c->analog.adc->CR2,
                              ADC_CR2_JEXTEN | ADC_CR2_ADON, 0U) &&
             ok;
        NX_AV_WRITE(&c->analog.adc->SR, 0U);
        if (ok && NX_AV_READ(&c->analog.adc->SR) == 0U)
            __atomic_store_n(&d->capture_owner, NULL, __ATOMIC_RELEASE);
        else
            d->quarantined = true;
    } else {
        d->quarantined = true;
    }
#else
    (void)d;
    (void)reason;
#endif
}
static bool tdm_start_ready(nx_stm32_power_stage_t* s, nx_time_us_t now) {
#if NEXUS_STM32_POWER_STAGE_TDM
    nx_stm32_power_stage_domain_t* d = s->config->domain;
    for (size_t i = 0U; i < d->config->count; ++i) {
        const nx_stm32_power_stage_t* peer = d->config->members[i];
        if (peer->active == NULL || fault(peer) != NX_SUCCESS ||
            (peer->warming && now < peer->ready_at))
            return false;
        if (peer->warming && now >= peer->active->startup_deadline) {
            tdm_abort(d, NX_ERROR_TIMEOUT);
            return false;
        }
    }
    const nx_stm32_power_stage_config_t* c = s->config;
    if (!d->origin_valid)
        return c->phase_offset_ticks == 0U;
    if (now < d->origin)
        return false;
    const uint32_t unit = ticks_per_us(c);
    const uint64_t period = c->pwm.period_ticks / unit;
    const uint64_t phase = (now - d->origin) % period;
    const uint64_t expected = c->phase_offset_ticks / unit;
    return phase >= expected &&
           phase - expected <= d->config->start_tolerance_us;
#else
    (void)s;
    (void)now;
    return false;
#endif
}
static bool tdm_publish_start(nx_stm32_power_stage_t* s) {
#if NEXUS_STM32_POWER_STAGE_TDM
    nx_stm32_power_stage_domain_t* d = s->config->domain;
    const nx_stm32_power_stage_config_t* c = s->config;
    if (!d->origin_valid) {
        if (c->phase_offset_ticks != 0U)
            return false;
        d->origin = s->cycle_anchor;
        d->origin_valid = true;
        return true;
    }
    if (s->cycle_anchor < d->origin)
        return false;
    const uint32_t unit = ticks_per_us(c);
    const uint64_t phase =
        (s->cycle_anchor - d->origin) % (c->pwm.period_ticks / unit);
    const uint64_t expected = c->phase_offset_ticks / unit;
    return phase >= expected &&
           phase - expected <= d->config->start_tolerance_us;
#else
    (void)s;
    return false;
#endif
}
static bool tdm_claim(nx_stm32_power_stage_t* s) {
#if NEXUS_STM32_POWER_STAGE_TDM
    nx_stm32_power_stage_domain_t* d = s->config->domain;
    const nx_stm32_power_stage_config_t* c = s->config;
    if (__atomic_load_n(&d->capture_owner, __ATOMIC_ACQUIRE) != NULL ||
        (NX_AV_READ(&c->analog.adc->SR) &
         (ADC_SR_JSTRT | ADC_SR_JEOC | ADC_SR_OVR)) != 0U ||
        (NX_AV_READ(&c->analog.adc->CR2) & ADC_CR2_ADON) == 0U)
        return false;
    if (!nx_stm32_av_bits(&c->analog.adc->CR2, ADC_CR2_JEXTEN, 0U) ||
        !nx_stm32_av_bits(&c->analog.adc->CR1, ADC_CR1_JEOCIE, 0U) ||
        !sequence(c) ||
        !nx_stm32_av_write(&c->analog.adc->JSQR,
                           capture_sequence(c, s->staged_phase_mask),
                           UINT32_MAX) ||
        !nx_stm32_av_bits(&c->analog.adc->CR2, ADC_CR2_JEXTSEL,
                          c->trigger_selector))
        return false;
    __atomic_store_n(&d->capture_owner, s, __ATOMIC_RELEASE);
    return nx_stm32_av_bits(&c->analog.adc->CR1, ADC_CR1_JEOCIE,
                            ADC_CR1_JEOCIE);
#else
    (void)s;
    return false;
#endif
}
static void tdm_complete(nx_stm32_power_stage_t* s) {
#if NEXUS_STM32_POWER_STAGE_TDM
    if (!tdm_withdraw(s))
        tdm_abort(s->config->domain, NX_ERROR_STATE);
#else
    (void)s;
#endif
}
static void trip(nx_stm32_power_stage_t* s, nx_result_t result) {
    uint32_t clear = NX_SUCCESS;
    (void)__atomic_compare_exchange_n(&s->fault, &clear, (uint32_t)result,
                                      false, __ATOMIC_RELEASE,
                                      __ATOMIC_RELAXED);
    nx_stm32_power_stage_domain_t* d = s->config->domain;
    if (time_shared(s->config) &&
        (result != NX_ERROR_CANCELLED || !domain_live(d))) {
        tdm_abort(d, result);
        return;
    }
    (void)suppress(s);
    if (d != NULL && !domain_live(d)) {
        for (size_t i = 0U; i < d->config->count; ++i) {
            nx_stm32_power_stage_t* peer = d->config->members[i];
            if (peer != s && peer->owned && peer->active != NULL) {
                uint32_t empty = NX_SUCCESS;
                (void)__atomic_compare_exchange_n(
                    &peer->fault, &empty, (uint32_t)NX_ERROR_STATE, false,
                    __ATOMIC_RELEASE, __ATOMIC_RELAXED);
                (void)suppress(peer);
            }
        }
    }
}
static uint32_t adc_gate(const nx_stm32_power_stage_config_t* c) {
    return RCC_APB2ENR_ADC1EN << c->adc_index;
}
static uint32_t timer_gate(const nx_stm32_power_stage_config_t* c) {
    return c->pwm.controller == 1U ? RCC_APB2ENR_TIM1EN : RCC_APB2ENR_TIM8EN;
}
static size_t domain_index(const nx_stm32_power_stage_domain_t* d,
                           const nx_stm32_power_stage_t* s) {
    for (size_t i = 0U; i < d->config->count; ++i)
        if (d->config->members[i] == s)
            return i;
    return d->config->count;
}
nx_result_t nx_stm32_power_stage_domain_admit(
    const nx_stm32_power_stage_domain_t* d,
    const nx_stm32_power_stage_domain_config_t* c) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (d == NULL || c == NULL || c->members == NULL ||
        c->configurations == NULL || c->count == 0U || c->count > 3U)
        return NX_ERROR_INVALID;
    if (d->owned)
        return NX_ERROR_BUSY;
    if (c->time_shared) {
#if NEXUS_STM32_POWER_STAGE_TDM
        if (c->count != 2U || c->start_tolerance_us == 0U)
            return NX_ERROR_INVALID;
        const nx_stm32_power_stage_config_t* a = c->configurations[0];
        const nx_stm32_power_stage_config_t* b = c->configurations[1];
        if (a == NULL || b == NULL || !config_valid(a) || !config_valid(b) ||
            !a->pwm.center_aligned || !b->pwm.center_aligned ||
            a->pair != NULL || b->pair != NULL ||
            a->analog.adc != b->analog.adc || a->adc_index != b->adc_index ||
            a->pwm.period_ticks != b->pwm.period_ticks ||
            a->pwm.tick_hz != b->pwm.tick_hz || a->phase_offset_ticks != 0U ||
            b->phase_offset_ticks != a->pwm.period_ticks / 2U)
            return NX_ERROR_INVALID;
        const uint64_t unit = ticks_per_us(a);
        const uint64_t tolerance =
            ((uint64_t)c->start_tolerance_us + 2U) * unit;
        const uint64_t arm_a = a->pwm.period_ticks / 2U;
        const uint64_t arm_b = b->phase_offset_ticks + arm_a;
        const uint64_t end_a = a->trigger_ticks + ((uint64_t)conversion_us(a) +
                                                   a->max_irq_delay_us +
                                                   a->compute_budget_us + 4U) *
                                                      unit;
        const uint64_t end_b =
            b->phase_offset_ticks + b->trigger_ticks +
            ((uint64_t)conversion_us(b) + b->max_irq_delay_us +
             b->compute_budget_us + 4U) *
                unit;
        if (tolerance >= arm_a || end_a + tolerance >= arm_b - tolerance ||
            end_b + tolerance >= arm_a + a->pwm.period_ticks - tolerance)
            return NX_ERROR_INVALID;
#else
        return NX_ERROR_UNSUPPORTED;
#endif
    }
    uint32_t selected = 0U;
    for (size_t i = 0U; i < c->count; ++i) {
        const nx_stm32_power_stage_config_t* a = c->configurations[i];
        if (c->members[i] == NULL || a == NULL || a->domain != d ||
            !config_valid(a) || !dc_storage_valid(c->members[i], a) ||
            (dc_profile(a) && (c->time_shared || c->count != 1U)) ||
            (!c->time_shared && (selected & converter_resources(a)) != 0U))
            return NX_ERROR_INVALID;
        selected |= converter_resources(a);
        for (size_t j = 0U; j < i; ++j)
            if (c->members[i] == c->members[j] ||
                a->pwm.registers == c->configurations[j]->pwm.registers ||
                (!c->time_shared &&
                 a->analog.adc == c->configurations[j]->analog.adc) ||
                a->analog.common != c->configurations[j]->analog.common ||
                a->analog.clock_hz != c->configurations[j]->analog.clock_hz ||
                a->pwm.common.priority !=
                    c->configurations[j]->pwm.common.priority)
                return NX_ERROR_INVALID;
    }
    const uint32_t all =
        RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN | RCC_APB2ENR_ADC3EN;
    if ((NX_AV_READ(&g_nx_stm32_system.rcc->APB2ENR) & all) != 0U ||
        !irq_idle(ADC_IRQn))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
nx_result_t nx_stm32_power_stage_domain_prepare(
    nx_stm32_power_stage_domain_t* d,
    const nx_stm32_power_stage_domain_config_t* c) {
    nx_result_t result = nx_stm32_power_stage_domain_admit(d, c);
    if (result != NX_SUCCESS)
        return result;
    d->config = c;
    d->owned = true;
    d->saved_priority = NX_AV_IRQ_PRIORITY_READ(ADC_IRQn);
    for (size_t i = 0U; i < c->count; ++i)
        d->clocks |= converter_resources(c->configurations[i]);
    if (!nx_stm32_av_bits(&g_nx_stm32_system.rcc->APB2ENR, d->clocks,
                          d->clocks))
        return NX_ERROR_IO;
    for (size_t i = 0U; i < c->count; ++i)
        if ((NX_AV_READ(&c->configurations[i]->analog.adc->CR2) &
             ADC_CR2_ADON) != 0U)
            return NX_ERROR_BUSY;
    ADC_Common_TypeDef* common = c->configurations[0]->analog.common;
    d->saved_common = NX_AV_READ(&common->CCR);
    if ((d->saved_common & (ADC_CCR_MULTI | ADC_CCR_DMA | ADC_CCR_DDS |
                            ADC_CCR_TSVREFE | ADC_CCR_VBATE)) != 0U)
        return NX_ERROR_BUSY;
    d->common_touched = true;
    return nx_stm32_av_bits(
               &common->CCR, ADC_CCR_ADCPRE | ADC_CCR_MULTI,
               ((NEXUS_CLOCK_ADC_DIV / 2U - 1U) << ADC_CCR_ADCPRE_Pos) |
                   common_mode(c->configurations[0]))
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
nx_result_t
nx_stm32_power_stage_domain_close(nx_stm32_power_stage_domain_t* d) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (d == NULL)
        return NX_ERROR_INVALID;
    if (!d->owned)
        return NX_SUCCESS;
    for (size_t i = 0U; i < d->config->count; ++i)
        if (d->config->members[i]->owned ||
            d->config->members[i]->active != NULL)
            return NX_ERROR_BUSY;
    if (d->active_mask != 0U || !nx_stm32_av_irq_detach(ADC_IRQn)) {
        d->quarantined = true;
        return NX_ERROR_BUSY;
    }
    d->irq_owned = false;
    if (d->common_touched &&
        (NX_AV_READ(&g_nx_stm32_system.rcc->APB2ENR) & d->clocks) != d->clocks)
        return NX_ERROR_STATE;
    if (d->config->time_shared && d->common_touched) {
        ADC_TypeDef* adc = d->config->configurations[0]->analog.adc;
        if (!nx_stm32_av_write(&adc->CR1, 0U, UINT32_MAX) ||
            !nx_stm32_av_write(&adc->CR2, 0U, UINT32_MAX))
            return NX_ERROR_IO;
        NX_AV_WRITE(&adc->SR, 0U);
        if (NX_AV_READ(&adc->SR) != 0U)
            return NX_ERROR_IO;
    }
    if (d->common_touched) {
        ADC_Common_TypeDef* common =
            d->config->configurations[0]->analog.common;
        if (!nx_stm32_av_write(&common->CCR, d->saved_common, UINT32_MAX))
            return NX_ERROR_IO;
        d->common_touched = false;
    }
    if (d->clocks != 0U) {
        if (!nx_stm32_av_bits(&g_nx_stm32_system.rcc->APB2ENR, d->clocks, 0U))
            return NX_ERROR_IO;
        d->clocks = 0U;
    }
    if (!nx_stm32_av_irq_priority_restore(ADC_IRQn, d->saved_priority))
        return NX_ERROR_IO;
    *d = (nx_stm32_power_stage_domain_t){0};
    return NX_SUCCESS;
}
void nx_stm32_power_stage_domain_adc_irq(nx_stm32_power_stage_domain_t* d) {
    if (d == NULL || !d->owned || d->config == NULL || d->clocks == 0U)
        return;
#if NEXUS_STM32_POWER_STAGE_TDM
    if (d->config->time_shared) {
        if ((NX_AV_READ(&g_nx_stm32_system.rcc->APB2ENR) & d->clocks) !=
            d->clocks) {
            tdm_abort(d, NX_ERROR_STATE);
            return;
        }
        nx_stm32_power_stage_t* owner =
            __atomic_load_n(&d->capture_owner, __ATOMIC_ACQUIRE);
        if (owner != NULL) {
            nx_stm32_power_stage_adc_irq(owner);
        } else {
            const nx_stm32_power_stage_config_t* c =
                d->config->configurations[0];
            if ((NX_AV_READ(&c->analog.adc->SR) &
                 (ADC_SR_JEOC | ADC_SR_JSTRT | ADC_SR_OVR)) != 0U)
                tdm_abort(d, NX_ERROR_STATE);
        }
        return;
    }
#endif
    for (size_t i = 0U; i < d->config->count; ++i)
        if ((__atomic_load_n(&d->active_mask, __ATOMIC_ACQUIRE) & (1U << i)) !=
            0U)
            nx_stm32_power_stage_adc_irq(d->config->members[i]);
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
        (c->encoder != NULL &&
         nx_stm32_av_overlap(data, bytes, c->encoder, sizeof(*c->encoder))))
        return false;
#if NEXUS_STM32_POWER_STAGE_DC
    if (dc_profile(c) &&
        (nx_stm32_av_overlap(data, bytes, c->dc, sizeof(*c->dc)) ||
         nx_stm32_av_overlap(data, bytes, c->dc->state, sizeof(*c->dc->state))))
        return false;
#endif
    const nx_stm32_av_pin_t* lists[2] = {c->pwm.common.pins, c->encoder_pins};
    const size_t counts[2] = {c->pwm.common.pin_count,
                              c->encoder != NULL ? 2U : 0U};
    for (size_t list = 0U; list < 2U; ++list) {
        if (counts[list] == 0U)
            continue;
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
    if (c->pair != NULL &&
        nx_stm32_av_overlap(data, bytes, c->pair, sizeof(*c->pair)))
        return false;
    if (c->domain != NULL) {
        const nx_stm32_power_stage_domain_t* d = c->domain;
        const nx_stm32_power_stage_domain_config_t* dc = d->config;
        if (dc == NULL || nx_stm32_av_overlap(data, bytes, d, sizeof(*d)) ||
            nx_stm32_av_overlap(data, bytes, dc, sizeof(*dc)) ||
            nx_stm32_av_overlap(data, bytes, dc->members,
                                dc->count * sizeof(*dc->members)) ||
            nx_stm32_av_overlap(data, bytes, dc->configurations,
                                dc->count * sizeof(*dc->configurations)))
            return false;
        for (size_t i = 0U; i < dc->count; ++i) {
            const nx_stm32_power_stage_t* peer = dc->members[i];
            const nx_stm32_power_stage_config_t* pc = dc->configurations[i];
            if (nx_stm32_av_overlap(data, bytes, peer, sizeof(*peer)) ||
                nx_stm32_av_overlap(data, bytes, pc, sizeof(*pc)) ||
                !nx_stm32_control_adc_disjoint(&pc->analog, data, bytes) ||
                nx_stm32_av_overlap(data, bytes, pc->pwm.registers,
                                    sizeof(*pc->pwm.registers)) ||
                (peer->active != NULL &&
                 (nx_stm32_av_overlap(data, bytes, peer->active,
                                      sizeof(*peer->active)) ||
                  nx_stm32_av_overlap(data, bytes, peer->active->context,
                                      peer->active->context_bytes))))
                return false;
        }
    }
    return true;
}
static bool plan_valid(const nx_stm32_power_stage_t* s,
                       const nx_power_stage_plan_t* plan,
                       uint64_t expected_sequence) {
    const nx_stm32_power_stage_config_t* c = s->config;
    if (plan->sequence != expected_sequence ||
        (!dc_profile(c) &&
         (plan->dc != NULL ||
          (plan->current_phase_mask & ~c->current_phase_mask) != 0U ||
          phase_count(plan->current_phase_mask) < 2U)) ||
        plan->trigger_ticks == 0U || plan->trigger_ticks >= c->pwm.period_ticks)
        return false;
    uint32_t maximum = 0U;
    for (size_t i = 0U; i < 3U; ++i) {
        if (plan->duty_ticks[i] < c->minimum_duty_ticks ||
            plan->duty_ticks[i] > c->maximum_duty_ticks ||
            (c->pwm.center_aligned
                 ? ((plan->duty_ticks[i] & 1U) != 0U ||
                    plan->start_ticks[i] !=
                        c->pwm.period_ticks - plan->duty_ticks[i] / 2U)
                 : plan->start_ticks[i] != 0U))
            return false;
        if (plan->duty_ticks[i] > maximum)
            maximum = plan->duty_ticks[i];
    }
#if NEXUS_STM32_POWER_STAGE_DC
    if (dc_profile(c)) {
        return nx_stm32_power_stage_dc_valid(c->dc, plan) &&
               (uint64_t)plan->dc->sample_ticks[1] +
                       ((uint64_t)s->scan_span_us + c->max_irq_delay_us +
                        c->compute_budget_us + 4U) *
                           ticks_per_us(c) <
                   c->commit_before_ticks;
    }
#endif
    const uint64_t unit = ticks_per_us(c);
    const uint64_t deadtime =
        ((uint64_t)c->pwm.deadtime_ticks * c->pwm.tick_hz + c->pwm.timer_hz -
         1U) /
        c->pwm.timer_hz;
    const uint64_t conversion = (uint64_t)s->scan_span_us * unit;
    bool window;
    if (c->pwm.center_aligned) {
        window =
            plan->trigger_ticks >
                c->pwm.period_ticks / 2U +
                    ((uint64_t)c->max_irq_delay_us + 2U) * unit &&
            (uint64_t)maximum / 2U + deadtime + c->settling_ticks <
                plan->trigger_ticks &&
            (uint64_t)plan->trigger_ticks + conversion + c->settling_ticks <
                c->pwm.period_ticks - maximum / 2U;
    } else {
        window = (uint64_t)maximum + deadtime + c->settling_ticks <
                 plan->trigger_ticks;
    }
    return window &&
           (uint64_t)plan->trigger_ticks + conversion +
                   ((uint64_t)c->max_irq_delay_us + c->compute_budget_us + 4U) *
                       unit <
               c->commit_before_ticks;
}
static nx_result_t drain(nx_stm32_power_stage_t* s) {
    bool ok = suppress(s);
    ok = nx_stm32_pwm_group_ops.stop(&s->pwm) == NX_SUCCESS && ok;
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    if (s->config->encoder != NULL) {
        if ((NX_AV_READ(&rcc->APB1ENR) & RCC_APB1ENR_TIM4EN) == 0U)
            ok = false;
        else {
            ok = nx_stm32_av_bits(&s->config->encoder->CR1, TIM_CR1_CEN, 0U) &&
                 ok;
            ok = nx_stm32_av_write(&s->config->encoder->CCER, 0U, UINT32_MAX) &&
                 ok;
        }
    }
    if (s->update_irq_owned) {
        if (nx_stm32_av_irq_detach(update_irq(s->config)))
            s->update_irq_owned = false;
        else
            ok = false;
    }
    if (s->irq_owned) {
        nx_stm32_power_stage_domain_t* d = s->config->domain;
        if (d == NULL) {
            ok = nx_stm32_av_irq_detach(ADC_IRQn) && ok;
        } else if (NX_AV_IRQ_ACTIVE(ADC_IRQn) != 0U) {
            ok = false;
        } else {
            const size_t index = domain_index(d, s);
            if (index == d->config->count) {
                ok = false;
            } else {
                const uint32_t remaining =
                    __atomic_fetch_and(&d->active_mask, ~(1U << index),
                                       __ATOMIC_ACQ_REL) &
                    ~(1U << index);
                if (remaining == 0U) {
                    ok = nx_stm32_av_irq_detach(ADC_IRQn) && ok;
                    if (ok)
                        d->irq_owned = false;
                }
            }
        }
    }
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
    s->capture_armed = false;
    s->overflow_seen = false;
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
        !plan_valid(s, &session->initial, 0U))
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
    s->staged_trigger_ticks = session->initial.trigger_ticks;
    s->staged_sequence = session->initial.sequence;
    s->staged_phase_mask = session->initial.current_phase_mask;
    s->ready_at = ready;
    s->warming = true;
    nx_pwm_group_frame_t frame = {session->initial.duty_ticks, 3U,
                                  session->initial.trigger_ticks};
    result = nx_stm32_pwm_group_ops.commit(&s->pwm, &frame);
#if NEXUS_STM32_POWER_STAGE_DC
    if (result == NX_SUCCESS && dc_profile(s->config))
        result =
            nx_stm32_power_stage_dc_stage(s->config->dc, &session->initial);
#endif
    if (!dc_profile(s->config) && !time_shared(s->config) &&
        s->config->pair == NULL &&
        !nx_stm32_av_write(
            &s->config->analog.adc->JSQR,
            capture_sequence(s->config, session->initial.current_phase_mask),
            UINT32_MAX))
        result = NX_ERROR_IO;
    ADC_TypeDef* adc = s->config->analog.adc;
    if (!time_shared(s->config))
        NX_AV_WRITE(&adc->SR, 0U);
    bool ok;
    if (time_shared(s->config)) {
        ok = (NX_AV_READ(&adc->CR2) & ADC_CR2_ADON) != 0U ||
             nx_stm32_av_bits(&adc->CR2, ADC_CR2_ADON, ADC_CR2_ADON);
    } else
#if NEXUS_STM32_POWER_STAGE_PAIR
        if (s->config->pair != NULL)
        ok = nx_stm32_adc_pair_enable(
                 s->config->pair, s->config->trigger_selector) == NX_SUCCESS;
    else
#endif
        ok = nx_stm32_av_write(
            &adc->CR2, ADC_CR2_ADON | s->config->trigger_selector, UINT32_MAX);
    if (result == NX_SUCCESS && ok)
        result = nx_deadline_finite_after(NX_AV_NOW(), 4U, &s->ready_at);
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
        count >= s->staged_trigger_ticks ||
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
        uint64_t limit = observation_tick(s) / ticks_per_us(c) +
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
        if (time_shared(c) && !tdm_start_ready(s, NX_AV_NOW()))
            return fault(s) != NX_SUCCESS ? drain(s) : NX_SUCCESS;
        bool ok = time_shared(c) ||
                  (nx_stm32_av_bits(&c->analog.adc->CR1, ADC_CR1_JEOCIE, 0U) &&
                   nx_stm32_av_bits(&c->analog.adc->CR2, ADC_CR2_JEXTEN, 0U));
        if (c->encoder != NULL) {
            ok = nx_stm32_av_bits(&c->encoder->CCER,
                                  TIM_CCER_CC1E | TIM_CCER_CC2E,
                                  TIM_CCER_CC1E | TIM_CCER_CC2E) &&
                 ok;
            ok = nx_stm32_av_bits(&c->encoder->CR1, TIM_CR1_CEN, TIM_CR1_CEN) &&
                 ok;
        }
        nx_result_t result =
            ok ? nx_stm32_pwm_group_ops.start(&s->pwm) : NX_ERROR_IO;
        if (result != NX_SUCCESS || !anchor(s) ||
            (time_shared(c) && !tdm_publish_start(s)) ||
            fault(s) != NX_SUCCESS) {
            trip(s, result != NX_SUCCESS ? result : NX_ERROR_TIMEOUT);
            return drain(s);
        }
        /* A peer may already publish on ADC_IRQn. Establish every reader's
         * state before the final source enable; no Task-only method is called
         * while this short publication boundary masks interrupts. */
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        s->irq_owned = true;
        if (c->domain == NULL ||
            __atomic_load_n(&c->domain->active_mask, __ATOMIC_ACQUIRE) == 0U) {
            NX_AV_IRQ_PRIORITY(ADC_IRQn, c->pwm.common.priority);
            NX_AV_IRQ_CLEAR(ADC_IRQn);
        }
        ok = NX_AV_IRQ_PRIORITY_READ(ADC_IRQn) == c->pwm.common.priority;
        if (c->domain != NULL) {
            const size_t index = domain_index(c->domain, s);
            if (index == c->domain->config->count) {
                ok = false;
            } else {
                (void)__atomic_fetch_or(&c->domain->active_mask, 1U << index,
                                        __ATOMIC_RELEASE);
                c->domain->irq_owned = true;
            }
        }
        s->warming = false;
        s->running = true;
        nx_arch_dsb();
        if (!time_shared(c) && !dc_profile(c)) {
            ok = nx_stm32_av_bits(&c->analog.adc->CR1, ADC_CR1_JEOCIE,
                                  ADC_CR1_JEOCIE) &&
                 ok;
            ok = nx_stm32_av_bits(&c->analog.adc->CR2, ADC_CR2_JEXTEN,
                                  c->pwm.center_aligned ? 0U
                                                        : ADC_CR2_JEXTEN_0) &&
                 ok;
        }
#if NEXUS_STM32_POWER_STAGE_DC
        if (dc_profile(c)) {
            ok = nx_stm32_power_stage_dc_arm(c->dc) == NX_SUCCESS && ok;
            s->capture_armed = ok;
        }
#endif
        if (uses_update(c)) {
            s->update_irq_owned = true;
            NX_AV_IRQ_PRIORITY(update_irq(c), c->pwm.common.priority);
            NX_AV_IRQ_CLEAR(update_irq(c));
            ok = NX_AV_IRQ_PRIORITY_READ(update_irq(c)) ==
                     c->pwm.common.priority &&
                 ok;
            ok = nx_stm32_av_bits(&c->pwm.registers->DIER, TIM_DIER_UIE,
                                  TIM_DIER_UIE) &&
                 ok;
            if (ok)
                NX_AV_IRQ_ENABLE(update_irq(c));
        }
        if (ok)
            NX_AV_IRQ_ENABLE(ADC_IRQn);
        nx_arch_irq_restore(saved);
        if (!ok) {
            trip(s, NX_ERROR_IO);
            return drain(s);
        }
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
    *out = (nx_power_stage_state_t){
        .clock_epoch = nx_stm32_av_epoch(),
        .tick_hz = c->pwm.tick_hz,
        .period_ticks = c->pwm.period_ticks,
        .trigger_ticks =
            s->active != NULL ? s->staged_trigger_ticks : c->trigger_ticks,
        .minimum_duty_ticks = c->minimum_duty_ticks,
        .maximum_duty_ticks = c->maximum_duty_ticks,
        .scan_span_us = s->scan_span_us,
        .capture_begin_ticks =
            dc_profile(c) ? 0U
            : c->pwm.center_aligned
                ? c->pwm.period_ticks / 2U +
                      (c->max_irq_delay_us + 2U) * ticks_per_us(c) + 1U
                : c->maximum_duty_ticks + c->settling_ticks +
                      (uint32_t)(((uint64_t)c->pwm.deadtime_ticks *
                                      c->pwm.tick_hz +
                                  c->pwm.timer_hz - 1U) /
                                 c->pwm.timer_hz) +
                      1U,
        .capture_end_ticks = c->pwm.center_aligned
                                 ? c->pwm.period_ticks -
                                       c->maximum_duty_ticks / 2U -
                                       c->settling_ticks
                                 : c->pwm.period_ticks,
        .deadtime_ticks =
            (uint32_t)(((uint64_t)c->pwm.deadtime_ticks * c->pwm.tick_hz +
                        c->pwm.timer_hz - 1U) /
                       c->pwm.timer_hz),
        .settling_ticks = c->settling_ticks,
        .aperture_ticks = s->scan_span_us * ticks_per_us(c),
        .conversion_ticks = s->scan_span_us * ticks_per_us(c),
        .irq_ticks = c->max_irq_delay_us * ticks_per_us(c),
        .compute_ticks = c->compute_budget_us * ticks_per_us(c),
        .rearm_ticks = 4U * ticks_per_us(c),
        .commit_before_ticks = c->commit_before_ticks,
        .completed_cycles = s->completed_cycles,
        .current_count = c->current_count,
        .current_phase_mask = c->current_phase_mask,
        .bus_available = c->sample_bus,
        .encoder_available = c->encoder != NULL,
        .center_aligned = c->pwm.center_aligned,
        .dc_link = dc_profile(c),
        .fault = (nx_result_t)fault(s),
        .running = s->running && fault(s) == NX_SUCCESS,
        .quarantined = s->quarantined};
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
static bool phase(nx_stm32_power_stage_t* s, nx_time_us_t now,
                  uint32_t observed_tick) {
    const nx_stm32_power_stage_config_t* c = s->config;
    uint32_t unit = ticks_per_us(c);
    if (!live(s)) {
        trip(s, NX_ERROR_STATE);
        return false;
    }
    if (now < s->last_observed || now < s->cycle_anchor ||
        now - s->cycle_anchor + 2U < observed_tick / unit ||
        now - s->cycle_anchor > (uint64_t)observed_tick / unit +
                                    s->scan_span_us + c->max_irq_delay_us +
                                    2U) {
        trip(s, NX_ERROR_TIMEOUT);
        return false;
    }
    TIM_TypeDef* timer = c->pwm.registers;
    uint32_t count = carrier_phase(c);
    bool update = (NX_AV_READ(&timer->SR) & TIM_SR_UIF) != 0U;
    if (count < observed_tick || count >= c->commit_before_ticks ||
        (c->pwm.center_aligned ? !s->overflow_seen
         : dc_profile(c)       ? update
                               : update == s->first_cycle) ||
        (c->pwm.center_aligned &&
         (NX_AV_READ(&timer->CR1) & TIM_CR1_DIR) == 0U) ||
        (NX_AV_READ(&timer->CR1) & TIM_CR1_CEN) == 0U ||
        (NX_AV_READ(&timer->BDTR) & TIM_BDTR_MOE) == 0U ||
        (c->encoder != NULL &&
         ((NX_AV_READ(&c->encoder->CR1) & TIM_CR1_CEN) == 0U ||
          (NX_AV_READ(&c->encoder->CCER) & (TIM_CCER_CC1E | TIM_CCER_CC2E)) !=
              (TIM_CCER_CC1E | TIM_CCER_CC2E)))) {
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
    if (plan->dc != NULL &&
        ((uintptr_t)plan->dc % _Alignof(nx_power_stage_dc_plan_t) != 0U ||
         (uintptr_t)plan->dc < (uintptr_t)s->active->context ||
         sizeof(*plan->dc) > s->active->context_bytes ||
         (uintptr_t)plan->dc - (uintptr_t)s->active->context >
             s->active->context_bytes - sizeof(*plan->dc))) {
        trip(s, NX_ERROR_INVALID);
        return false;
    }
    if (!plan_valid(s, plan, s->completed_cycles + 1U)) {
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
    ADC_TypeDef* adc = c->analog.adc;
    const uint32_t trigger_edges = NX_AV_READ(&adc->CR2) & ADC_CR2_JEXTEN;
    if (!nx_stm32_av_bits(&adc->CR2, ADC_CR2_JEXTEN, 0U) ||
        (NX_AV_READ(&adc->SR) & (ADC_SR_JSTRT | ADC_SR_JEOC)) != 0U ||
        !nx_stm32_av_bits(&timer->CR1, TIM_CR1_UDIS, TIM_CR1_UDIS)) {
        trip(s, NX_ERROR_IO);
        return false;
    }
    const uint32_t divisor = c->pwm.center_aligned ? 2U : 1U;
    bool ok = nx_stm32_av_write(&timer->CCR1, plan->duty_ticks[0] / divisor,
                                UINT32_MAX);
    ok = nx_stm32_av_write(&timer->CCR2, plan->duty_ticks[1] / divisor,
                           UINT32_MAX) &&
         ok;
    ok = nx_stm32_av_write(&timer->CCR3, plan->duty_ticks[2] / divisor,
                           UINT32_MAX) &&
         ok;
    ok = nx_stm32_av_write(&timer->CCR4,
                           c->pwm.center_aligned
                               ? c->pwm.period_ticks - plan->trigger_ticks
                               : plan->trigger_ticks,
                           UINT32_MAX) &&
         ok;
    if (!dc_profile(c) && c->pair == NULL)
        ok = nx_stm32_av_write(&adc->JSQR,
                               capture_sequence(c, plan->current_phase_mask),
                               UINT32_MAX) &&
             ok;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    now = NX_AV_NOW();
    uint32_t count = carrier_phase(c);
    if (fault(s) != NX_SUCCESS || !live(s) || now < began ||
        now - began > c->compute_budget_us || now < s->cycle_anchor ||
        now - s->cycle_anchor >=
            c->commit_before_ticks / ticks_per_us(c) - 2U ||
        count < s->staged_trigger_ticks || count >= c->commit_before_ticks ||
        (NX_AV_READ(&timer->SR) & (TIM_SR_UIF | TIM_SR_BIF)) != 0U ||
        (NX_AV_READ(&c->analog.adc->SR) & (ADC_SR_JEOC | ADC_SR_JSTRT)) != 0U) {
        nx_arch_irq_restore(saved);
        trip(s, NX_ERROR_TIMEOUT);
        return false;
    }
#if NEXUS_STM32_POWER_STAGE_DC
    if (ok && dc_profile(c))
        ok = nx_stm32_power_stage_dc_stage(c->dc, plan) == NX_SUCCESS;
#endif
    if (!ok || !nx_stm32_av_bits(&adc->CR2, ADC_CR2_JEXTEN, trigger_edges) ||
        !nx_stm32_av_bits(&timer->CR1, TIM_CR1_UDIS, 0U)) {
        nx_arch_irq_restore(saved);
        trip(s, NX_ERROR_IO);
        return false;
    }
    for (size_t i = 0U; i < 3U; ++i)
        s->pwm.duties[i] = plan->duty_ticks[i];
    s->pwm.trigger_ticks = plan->trigger_ticks;
    s->staged_trigger_ticks = plan->trigger_ticks;
    s->staged_sequence = plan->sequence;
    s->staged_phase_mask = plan->current_phase_mask;
    nx_arch_irq_restore(saved);
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
#if NEXUS_STM32_POWER_STAGE_PAIR
    uint16_t paired[2] = {0U, 0U};
    if (s->config->pair != NULL) {
        nx_result_t result = nx_stm32_adc_pair_read(s->config->pair, paired);
        if (result == NX_ERROR_BUSY) {
            nx_time_us_t now = NX_AV_NOW();
            uint64_t latest =
                s->staged_trigger_ticks / ticks_per_us(s->config) +
                (uint64_t)s->scan_span_us + s->config->max_irq_delay_us + 2U;
            if (now < s->cycle_anchor || now - s->cycle_anchor > latest)
                trip(s, NX_ERROR_TIMEOUT);
            return;
        }
        if (result != NX_SUCCESS) {
            trip(s, result);
            return;
        }
    }
#endif
    if (s->config->pwm.center_aligned) {
        if (!s->capture_armed ||
            !nx_stm32_av_bits(&adc->CR2, ADC_CR2_JEXTEN, 0U)) {
            trip(s, NX_ERROR_STATE);
            return;
        }
        s->capture_armed = false;
    }
    nx_time_us_t began = NX_AV_NOW();
    const uint32_t observed_tick = observation_tick(s);
    if (!phase(s, began, observed_tick))
        return;
#if NEXUS_STM32_POWER_STAGE_DC
    const nx_power_stage_dc_capture_t* dc_capture = NULL;
    if (dc_profile(s->config)) {
        nx_result_t result =
            nx_stm32_power_stage_dc_read(s->config->dc, &dc_capture);
        if (result == NX_ERROR_BUSY) {
            s->last_observed = began;
            return;
        }
        if (result != NX_SUCCESS) {
            trip(s, result);
            return;
        }
        s->capture_armed = false;
    }
#endif
    if (s->completed_cycles == UINT64_MAX) {
        trip(s, NX_ERROR_EXHAUSTED);
        return;
    }
    NX_AV_WRITE(&adc->SR, ~((uint32_t)ADC_SR_JEOC | (uint32_t)ADC_SR_JSTRT));
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
    uint16_t raw[4] = {0U, 0U, 0U, 0U};
    const uint8_t current_count = phase_count(s->staged_phase_mask);
    const size_t rank_count =
        (size_t)current_count + (s->config->sample_bus ? 1U : 0U);
    for (size_t i = 0U; i < rank_count; ++i) {
        uint32_t value;
#if NEXUS_STM32_POWER_STAGE_PAIR
        if (s->config->pair != NULL)
            value = paired[i];
        else
#endif
            value = NX_AV_READ(ranks[i]);
        if (value > 4095U) {
            trip(s, NX_ERROR_INVALID);
            return;
        }
        raw[i] = (uint16_t)value;
    }
    nx_time_us_t angle_before = NX_AV_NOW();
    uint32_t encoder =
        s->config->encoder != NULL ? NX_AV_READ(&s->config->encoder->CNT) : 0U;
    nx_time_us_t angle_after = NX_AV_NOW();
    if (angle_after < angle_before || angle_after - angle_before > 1U ||
        encoder > UINT16_MAX) {
        trip(s, NX_ERROR_TIMEOUT);
        return;
    }
    nx_power_stage_sample_t sample = {
        .current_raw = {0U, 0U, 0U},
        .bus_raw = s->config->sample_bus ? raw[current_count] : 0U,
        .current_count = current_count,
        .current_phase_mask = s->staged_phase_mask,
        .bus_valid = s->config->sample_bus,
        .encoder_valid = s->config->encoder != NULL,
        .plan_sequence = s->staged_sequence,
        .encoder_count = encoder,
        .sequence = s->completed_cycles,
        .clock_epoch = s->time_epoch,
        .sample_at =
            s->cycle_anchor + s->staged_trigger_ticks / ticks_per_us(s->config),
        .observed_at = angle_after,
        .angle_at = angle_after,
        .planned_apply_at = next,
        .scan_span_us = s->scan_span_us,
        .trigger_ticks = s->staged_trigger_ticks};
#if NEXUS_STM32_POWER_STAGE_DC
    sample.dc = dc_capture;
    if (dc_capture != NULL)
        sample.scan_span_us +=
            (dc_capture->sample_ticks[1] - dc_capture->sample_ticks[0] +
             ticks_per_us(s->config) - 1U) /
            ticks_per_us(s->config);
#endif
    size_t code = 0U;
    for (size_t phase_index = 0U; phase_index < 3U; ++phase_index)
        if ((sample.current_phase_mask & (1U << phase_index)) != 0U)
            sample.current_raw[phase_index] = raw[code++];
    if ((NX_AV_READ(&adc->SR) & ADC_SR_JEOC) != 0U) {
        trip(s, NX_ERROR_OVERFLOW);
        return;
    }
    nx_power_stage_plan_t plan = {0};
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
    s->overflow_seen = false;
    if (time_shared(s->config))
        tdm_complete(s);
}
void nx_stm32_power_stage_update_irq(nx_stm32_power_stage_t* s) {
    if (s == NULL || !s->initialized || !s->running || s->active == NULL ||
        !uses_update(s->config) || fault(s) != NX_SUCCESS)
        return;
    const nx_stm32_power_stage_config_t* c = s->config;
    if (!live(s)) {
        trip(s, NX_ERROR_STATE);
        return;
    }
    TIM_TypeDef* timer = c->pwm.registers;
    if ((NX_AV_READ(&timer->SR) & TIM_SR_UIF) == 0U)
        return;
    NX_AV_WRITE(&timer->SR, ~(uint32_t)TIM_SR_UIF);
#if NEXUS_STM32_POWER_STAGE_DC
    if (dc_profile(c)) {
        nx_time_us_t now = NX_AV_NOW();
        if (s->capture_armed || now < s->cycle_anchor ||
            now - s->cycle_anchor > (uint64_t)c->max_irq_delay_us + 2U ||
            carrier_phase(c) >= s->staged_trigger_ticks ||
            nx_stm32_power_stage_dc_arm(c->dc) != NX_SUCCESS) {
            trip(s, NX_ERROR_TIMEOUT);
            return;
        }
        s->capture_armed = true;
        return;
    }
#endif
    if ((NX_AV_READ(&timer->CR1) & TIM_CR1_DIR) == 0U) {
        if (s->capture_armed)
            trip(s, NX_ERROR_TIMEOUT);
        return;
    }
    nx_time_us_t now = NX_AV_NOW();
    const uint64_t half_us = c->pwm.period_ticks / 2U / ticks_per_us(c);
    if (s->capture_armed || s->overflow_seen || now < s->cycle_anchor ||
        now - s->cycle_anchor + 1U < half_us ||
        now - s->cycle_anchor > half_us + c->max_irq_delay_us + 2U ||
        carrier_phase(c) >= s->staged_trigger_ticks ||
        (NX_AV_READ(&c->analog.adc->SR) & ADC_SR_JEOC) != 0U ||
        (time_shared(c) && !tdm_claim(s)) ||
        !nx_stm32_av_bits(&c->analog.adc->CR2, ADC_CR2_JEXTEN,
                          ADC_CR2_JEXTEN_0)) {
        trip(s, NX_ERROR_TIMEOUT);
        return;
    }
    s->capture_armed = true;
    s->overflow_seen = true;
}
void nx_stm32_power_stage_break_irq(nx_stm32_power_stage_t* s) {
    if (s == NULL || !s->owned || s->active == NULL)
        return;
    if ((NX_AV_READ(&g_nx_stm32_system.rcc->APB2ENR) & timer_gate(s->config)) ==
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
    nx_result_t result = NX_SUCCESS;
    if (s->config->domain == NULL) {
        result = nx_stm32_control_adc_close(&s->analog);
        if (result == NX_SUCCESS &&
            !nx_stm32_av_irq_priority_restore(ADC_IRQn, s->irq_priority))
            result = NX_ERROR_IO;
    } else if (time_shared(s->config)) {
        s->analog = (nx_stm32_control_adc_t){0};
    } else if (s->analog.config != NULL) {
#if NEXUS_STM32_POWER_STAGE_PAIR
        if (s->config->pair != NULL) {
            if ((NX_AV_READ(&g_nx_stm32_system.rcc->APB2ENR) &
                 converter_resources(s->config)) !=
                converter_resources(s->config))
                return NX_ERROR_STATE;
            result = nx_stm32_adc_pair_disable(s->config->pair);
            if (result != NX_SUCCESS)
                return result;
        }
#endif
        if ((NX_AV_READ(&g_nx_stm32_system.rcc->APB2ENR) &
             adc_gate(s->config)) == 0U)
            return NX_ERROR_STATE;
        if (!nx_stm32_av_write(&s->config->analog.adc->CR1, 0U, UINT32_MAX) ||
            !nx_stm32_av_write(&s->config->analog.adc->CR2, 0U, UINT32_MAX))
            return NX_ERROR_IO;
        s->analog = (nx_stm32_control_adc_t){0};
    }
    if (result != NX_SUCCESS)
        return result;
    if (uses_update(s->config) &&
        !nx_stm32_av_irq_priority_restore(update_irq(s->config),
                                          s->update_irq_priority))
        return NX_ERROR_IO;
    *s = (nx_stm32_power_stage_t){0};
    return NX_SUCCESS;
}
const nx_power_stage_ops_t nx_stm32_power_stage_ops = {
    .start = start, .service = service, .stop = stop, .state = state};
