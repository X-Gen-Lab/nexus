/**
 * \file            single_shunt.c
 * \brief           Direct DC-link active-vector current-sampling planning
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "modulation_private.h"

static uint8_t phase_mask(uint8_t vector) {
    static const uint8_t masks[8] = {0U, 4U, 2U, 1U, 1U, 2U, 4U, 0U};
    return masks[vector];
}

static bool bus_samples(const nx_modulation_config_t* config,
                        nx_modulation_output_t* plan) {
    nx_modulation_window_t candidates[7];
    size_t count = nx_modulation_windows(config, plan, candidates);
    uint32_t best_span = 0U;
    bool found = false;
    for (size_t first = 0U; first < count; ++first) {
        uint8_t mask = phase_mask(candidates[first].vector);
        nx_modulation_sample_t a;
        if (mask == 0U ||
            !nx_modulation_aperture(config, &candidates[first], 0U, false, &a))
            continue;
        uint64_t not_before = (uint64_t)a.tick + config->conversion_ticks +
                              config->irq_ticks + config->rearm_ticks;
        if (not_before >= config->period_ticks)
            continue;
        for (size_t second = first + 1U; second < count; ++second) {
            uint8_t other = phase_mask(candidates[second].vector);
            nx_modulation_sample_t b;
            if (other == 0U || other == mask ||
                !nx_modulation_aperture(config, &candidates[second],
                                        (uint32_t)not_before, true, &b))
                continue;
            uint32_t span_a = a.window_end - a.window_begin;
            uint32_t span_b = b.window_end - b.window_begin;
            uint32_t span = span_a < span_b ? span_a : span_b;
            if (found && span <= best_span)
                continue;
            a.phase_mask = mask;
            b.phase_mask = other;
            nx_modulation_sample_t centered = a;
            (void)nx_modulation_aperture(config, &candidates[first], 0U, true,
                                         &centered);
            uint32_t latest = b.tick - config->conversion_ticks -
                              config->irq_ticks - config->rearm_ticks;
            centered.tick = centered.tick < latest ? centered.tick : latest;
            centered.phase_mask = mask;
            plan->samples[0] = centered;
            plan->samples[1] = b;
            plan->sample_count = 2U;
            best_span = span;
            found = true;
        }
    }
    return found;
}

static void shift(const nx_modulation_config_t* config,
                  nx_modulation_output_t* plan) {
    size_t rank[3] = {0U, 1U, 2U};
    for (size_t i = 1U; i < 3U; ++i) {
        size_t phase = rank[i];
        size_t at = i;
        while (at > 0U &&
               plan->duty_ticks[rank[at - 1U]] > plan->duty_ticks[phase]) {
            rank[at] = rank[at - 1U];
            --at;
        }
        rank[at] = phase;
    }
    uint32_t period = config->period_ticks;
    uint32_t amount = config->single_shunt_shift_ticks;
    plan->start_ticks[rank[0]] =
        (plan->start_ticks[rank[0]] + period - amount) % period;
    plan->start_ticks[rank[2]] = (plan->start_ticks[rank[2]] + amount) % period;
    plan->phase_shifted = true;
}

nx_result_t nx_modulation_bus_plan(const nx_modulation_config_t* config,
                                   nx_motor_ab_t requested_voltage,
                                   float bus_voltage_v,
                                   nx_modulation_output_t* out) {
    nx_modulation_output_t next;
    nx_result_t result = nx_modulation_begin(config, requested_voltage,
                                             bus_voltage_v, out, &next);
    if (result != NX_SUCCESS)
        return result;
    if (config->topology != NX_MODULATION_SINGLE_SHUNT)
        return NX_ERROR_INVALID;
    bool found = bus_samples(config, &next);
    if (!found && config->single_shunt_shift_ticks != 0U) {
        shift(config, &next);
        found = bus_samples(config, &next);
    }
    if (!found)
        return NX_ERROR_UNSUPPORTED;
    *out = next;
    return NX_SUCCESS;
}
