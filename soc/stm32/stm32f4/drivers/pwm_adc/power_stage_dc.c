/**
 * \file            power_stage_dc.c
 * \brief           Bounded full-cycle DC-link capture bookkeeping
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/pwm_adc/power_stage_dc.h"

static bool object(const void* pointer, size_t bytes, size_t alignment) {
    const uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}

static bool selected(const nx_stm32_power_stage_dc_config_t* c) {
    if (!object(c, sizeof(*c), _Alignof(nx_stm32_power_stage_dc_config_t)) ||
        !nx_stm32_single_shunt_valid(&c->capture) ||
        !object(c->state, sizeof(*c->state),
                _Alignof(nx_stm32_power_stage_dc_t)) ||
        c->aperture_ticks == 0U ||
        c->settling_ticks >= c->capture.period_ticks ||
        c->aperture_ticks >= c->capture.period_ticks ||
        nx_stm32_av_overlap(c->state, sizeof(*c->state), c, sizeof(*c)) ||
        nx_stm32_av_overlap(c->state, sizeof(*c->state), c->capture.adc,
                            sizeof(*c->capture.adc)) ||
        nx_stm32_av_overlap(c->state, sizeof(*c->state), c->capture.timer,
                            sizeof(*c->capture.timer))) {
        return false;
    }
#ifndef NEXUS_STM32_MODEL
    if (c->capture.adc != ADC1)
        return false;
#endif
    return true;
}

bool nx_stm32_power_stage_dc_valid(const nx_stm32_power_stage_dc_config_t* c,
                                   const nx_power_stage_plan_t* plan) {
    if (!selected(c) ||
        !object(plan, sizeof(*plan), _Alignof(nx_power_stage_plan_t)) ||
        !object(plan->dc, sizeof(*plan->dc),
                _Alignof(nx_power_stage_dc_plan_t)) ||
        nx_stm32_av_overlap(c->state, sizeof(*c->state), plan, sizeof(*plan)) ||
        nx_stm32_av_overlap(c->state, sizeof(*c->state), plan->dc,
                            sizeof(*plan->dc)) ||
        plan->current_phase_mask != 0U ||
        plan->trigger_ticks != plan->dc->sample_ticks[0]) {
        return false;
    }
    const nx_power_stage_dc_plan_t* dc = plan->dc;
    if (dc->sample_ticks[0] == 0U ||
        dc->sample_ticks[0] >= dc->sample_ticks[1] ||
        dc->sample_ticks[1] >= c->capture.period_ticks ||
        (uint64_t)(dc->sample_ticks[1] - dc->sample_ticks[0]) <=
            (uint64_t)c->capture.maximum_lateness_ticks +
                c->capture.rearm_guard_ticks ||
        dc->switching_vector[0] == dc->switching_vector[1] ||
        (dc->switching_vector[0] ^ dc->switching_vector[1]) == 7U) {
        return false;
    }
    for (size_t sample = 0U; sample < 2U; ++sample) {
        const uint32_t tick = dc->sample_ticks[sample];
        uint32_t before = 0U, after = c->capture.period_ticks;
        uint8_t vector = 0U;
        for (size_t phase = 0U; phase < 3U; ++phase) {
            const uint32_t width = plan->duty_ticks[phase];
            if (plan->start_ticks[phase] != 0U ||
                width >= c->capture.period_ticks) {
                return false;
            }
            if (tick < width) {
                vector |= (uint8_t)(1U << (2U - phase));
                if (width < after)
                    after = width;
            } else if (width > before) {
                before = width;
            }
        }
        if (vector == 0U || vector == 7U ||
            vector != dc->switching_vector[sample] ||
            tick - before <= c->settling_ticks ||
            after - tick <= c->aperture_ticks) {
            return false;
        }
    }
    return true;
}

nx_result_t
nx_stm32_power_stage_dc_prepare(const nx_stm32_power_stage_dc_config_t* c) {
    if (!selected(c))
        return NX_ERROR_INVALID;
    const nx_result_t result = nx_stm32_single_shunt_prepare(&c->capture);
    if (result == NX_SUCCESS)
        *c->state = (nx_stm32_power_stage_dc_t){.prepared = true};
    return result;
}

nx_result_t
nx_stm32_power_stage_dc_stage(const nx_stm32_power_stage_dc_config_t* c,
                              const nx_power_stage_plan_t* plan) {
    if (!selected(c))
        return NX_ERROR_INVALID;
    nx_stm32_power_stage_dc_t* s = c->state;
    if (s->fault != NX_SUCCESS)
        return s->fault;
    if (!s->prepared)
        return NX_ERROR_STATE;
    if (s->armed || s->staged)
        return NX_ERROR_BUSY;
    if (!nx_stm32_power_stage_dc_valid(c, plan))
        return NX_ERROR_INVALID;
    if ((!s->history && plan->sequence != 0U) ||
        (s->history && (s->capture.sequence == UINT64_MAX ||
                        plan->sequence != s->capture.sequence + 1U))) {
        return NX_ERROR_STATE;
    }
    /* Copy before returning to the caller; its plan may be callback-local. */
    const nx_power_stage_dc_capture_t next = {
        .sample_ticks = {plan->dc->sample_ticks[0], plan->dc->sample_ticks[1]},
        .switching_vector = {plan->dc->switching_vector[0],
                             plan->dc->switching_vector[1]},
        .sequence = plan->sequence};
    s->capture = next;
    s->staged = true;
    s->history = true;
    return NX_SUCCESS;
}

/** \brief Rearm failure is fatal, unlike a read before conversion completion.
 */
static nx_result_t failed(nx_stm32_power_stage_dc_t* s, nx_result_t reason) {
    s->fault = reason == NX_ERROR_BUSY ? NX_ERROR_STATE : reason;
    return s->fault;
}

nx_result_t
nx_stm32_power_stage_dc_arm(const nx_stm32_power_stage_dc_config_t* c) {
    if (!selected(c))
        return NX_ERROR_INVALID;
    nx_stm32_power_stage_dc_t* s = c->state;
    if (s->fault != NX_SUCCESS)
        return s->fault;
    if (!s->prepared)
        return NX_ERROR_STATE;
    if (s->armed)
        return NX_ERROR_BUSY;
    if (!s->staged)
        return NX_ERROR_STATE;
    /* Publish callback metadata before the leaf can enable JEOC/trigger. */
    s->index = 0U;
    s->staged = false;
    s->armed = true;
    nx_arch_dmb();
    const nx_result_t result =
        nx_stm32_single_shunt_arm(&c->capture, s->capture.sample_ticks[0]);
    return result == NX_SUCCESS ? NX_SUCCESS : failed(s, result);
}

nx_result_t
nx_stm32_power_stage_dc_read(const nx_stm32_power_stage_dc_config_t* c,
                             const nx_power_stage_dc_capture_t** out) {
    if (!selected(c) ||
        !object(out, sizeof(*out),
                _Alignof(const nx_power_stage_dc_capture_t*)) ||
        nx_stm32_av_overlap(out, sizeof(*out), c->state, sizeof(*c->state)) ||
        nx_stm32_av_overlap(out, sizeof(*out), c, sizeof(*c)) ||
        nx_stm32_av_overlap(out, sizeof(*out), c->capture.adc,
                            sizeof(*c->capture.adc)) ||
        nx_stm32_av_overlap(out, sizeof(*out), c->capture.timer,
                            sizeof(*c->capture.timer))) {
        return NX_ERROR_INVALID;
    }
    nx_stm32_power_stage_dc_t* s = c->state;
    if (s->fault != NX_SUCCESS)
        return s->fault;
    if (!s->prepared || !s->armed || s->index > 1U)
        return NX_ERROR_STATE;
    uint16_t code;
    nx_result_t result = nx_stm32_single_shunt_read(
        &c->capture, s->capture.sample_ticks[s->index], &code);
    if (result == NX_ERROR_BUSY)
        return result;
    if (result != NX_SUCCESS)
        return failed(s, result);
    s->capture.code[s->index] = code;
    ++s->index;
    if (s->index == 1U) {
        nx_arch_dmb();
        result =
            nx_stm32_single_shunt_arm(&c->capture, s->capture.sample_ticks[1]);
        return result == NX_SUCCESS ? NX_ERROR_BUSY : failed(s, result);
    }
    s->armed = false;
    *out = &s->capture;
    return NX_SUCCESS;
}

nx_result_t
nx_stm32_power_stage_dc_disable(const nx_stm32_power_stage_dc_config_t* c) {
    if (!selected(c))
        return NX_ERROR_INVALID;
    const nx_result_t result = nx_stm32_single_shunt_disable(&c->capture);
    if (result == NX_SUCCESS)
        *c->state = (nx_stm32_power_stage_dc_t){0};
    else if (c->state->fault == NX_SUCCESS)
        c->state->fault = result;
    return result;
}
