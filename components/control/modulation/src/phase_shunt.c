/**
 * \file            phase_shunt.c
 * \brief           Direct two/three-shunt integer-carrier planning
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "modulation_private.h"

static uint8_t lows(uint8_t vector) {
    uint8_t result = 0U;
    for (size_t phase = 0U; phase < 3U; ++phase)
        if ((vector & (1U << (2U - phase))) == 0U)
            result |= (uint8_t)(1U << phase);
    return result;
}

static bool phase_samples(const nx_modulation_config_t* config,
                          nx_modulation_output_t* plan) {
    nx_modulation_window_t candidates[7];
    size_t count = nx_modulation_windows(config, plan, candidates);
    uint32_t best_span = 0U;
    bool found = false;
    for (size_t i = 0U; i < count; ++i) {
        uint8_t available = lows(candidates[i].vector) & config->sensor_mask;
        unsigned count_available = (unsigned)(available & 1U) +
                                   (unsigned)((available >> 1U) & 1U) +
                                   (unsigned)((available >> 2U) & 1U);
        if (count_available < config->samples_per_trigger)
            continue;
        nx_modulation_sample_t sample;
        if (!nx_modulation_aperture(config, &candidates[i], 0U, true, &sample))
            continue;
        uint32_t span = sample.window_end - sample.window_begin;
        if (found && span <= best_span)
            continue;
        uint8_t selected = 0U;
        unsigned remaining = config->samples_per_trigger;
        for (size_t phase = 0U; phase < 3U && remaining > 0U; ++phase) {
            uint8_t bit = (uint8_t)(1U << phase);
            if ((available & bit) != 0U) {
                selected |= bit;
                --remaining;
            }
        }
        sample.phase_mask = selected;
        plan->samples[0] = sample;
        plan->sample_count = 1U;
        best_span = span;
        found = true;
    }
    return found;
}

nx_result_t nx_modulation_phase_plan(const nx_modulation_config_t* config,
                                     nx_motor_ab_t requested_voltage,
                                     float bus_voltage_v,
                                     nx_modulation_output_t* out) {
    nx_modulation_output_t next;
    nx_result_t result = nx_modulation_begin(config, requested_voltage,
                                             bus_voltage_v, out, &next);
    if (result != NX_SUCCESS)
        return result;
    if (config->topology == NX_MODULATION_SINGLE_SHUNT)
        return NX_ERROR_INVALID;
    if (!phase_samples(config, &next))
        return NX_ERROR_UNSUPPORTED;
    *out = next;
    return NX_SUCCESS;
}
