/**
 * \file            modulation.c
 * \brief           Coherent integer-carrier modulation implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "modulation_private.h"
#include <math.h>

#define NX_MODULATION_INV_SQRT3  0.57735026918962576451F
#define NX_MODULATION_SQRT3_HALF 0.86602540378443864676F

typedef struct {
    uint32_t tick;
    uint8_t mask;
} edge_t;

static bool object(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}

static bool overlap(const void* first, size_t first_bytes, const void* second,
                    size_t second_bytes) {
    return (uintptr_t)first < (uintptr_t)second + second_bytes &&
           (uintptr_t)second < (uintptr_t)first + first_bytes;
}

static unsigned bits(uint8_t mask) {
    return (unsigned)(mask & 1U) + (unsigned)((mask >> 1U) & 1U) +
           (unsigned)((mask >> 2U) & 1U);
}

nx_result_t nx_modulation_check(const nx_modulation_config_t* config) {
    if (!object(config, sizeof(*config), _Alignof(nx_modulation_config_t)) ||
        (uint32_t)config->topology > (uint32_t)NX_MODULATION_SINGLE_SHUNT ||
        (uint32_t)config->alignment > (uint32_t)NX_MODULATION_CENTER_ALIGNED ||
        config->period_ticks < 8U || config->period_ticks > (1U << 24U) ||
        config->minimum_duty_ticks == 0U ||
        config->minimum_duty_ticks >= config->maximum_duty_ticks ||
        config->maximum_duty_ticks >= config->period_ticks ||
        config->aperture_ticks == 0U ||
        config->conversion_ticks < config->aperture_ticks ||
        config->compute_ticks == 0U ||
        config->commit_before_ticks > config->period_ticks ||
        (uint64_t)config->deadtime_ticks + config->settling_ticks +
                config->aperture_ticks >=
            config->period_ticks ||
        (uint64_t)config->conversion_ticks + config->irq_ticks +
                config->compute_ticks >=
            config->commit_before_ticks ||
        (uint64_t)config->conversion_ticks + config->irq_ticks +
                config->rearm_ticks >=
            config->period_ticks)
        return NX_ERROR_INVALID;
    uint32_t capture_end = config->capture_end_ticks != 0U
                               ? config->capture_end_ticks
                               : config->period_ticks;
    if (capture_end > config->period_ticks ||
        config->capture_begin_ticks >= capture_end ||
        (uint64_t)config->capture_begin_ticks + config->conversion_ticks +
                config->irq_ticks + config->rearm_ticks >
            capture_end)
        return NX_ERROR_INVALID;
    if (config->alignment == NX_MODULATION_CENTER_ALIGNED &&
        ((config->period_ticks | config->minimum_duty_ticks |
          config->maximum_duty_ticks) &
         1U) != 0U)
        return NX_ERROR_INVALID;
    switch (config->topology) {
        case NX_MODULATION_THREE_SHUNT:
            if (config->sensor_mask != 7U ||
                (config->samples_per_trigger != 2U &&
                 config->samples_per_trigger != 3U) ||
                config->single_shunt_shift_ticks != 0U)
                return NX_ERROR_INVALID;
            break;
        case NX_MODULATION_TWO_SHUNT:
            if (config->sensor_mask > 7U || bits(config->sensor_mask) != 2U ||
                config->samples_per_trigger != 2U ||
                config->single_shunt_shift_ticks != 0U)
                return NX_ERROR_INVALID;
            break;
        case NX_MODULATION_SINGLE_SHUNT:
            if (config->sensor_mask != 0U ||
                config->samples_per_trigger != 1U ||
                config->single_shunt_shift_ticks > config->period_ticks / 4U)
                return NX_ERROR_INVALID;
            break;
        default:
            return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}

static nx_result_t pulses(const nx_modulation_config_t* config,
                          nx_motor_ab_t requested, float bus,
                          nx_modulation_output_t* output) {
    const float phase[3] = {
        requested.alpha,
        -0.5F * requested.alpha + NX_MODULATION_SQRT3_HALF * requested.beta,
        -0.5F * requested.alpha - NX_MODULATION_SQRT3_HALF * requested.beta};
    const float maximum = fmaxf(phase[0], fmaxf(phase[1], phase[2]));
    const float minimum = fminf(phase[0], fminf(phase[1], phase[2]));
    const float span = maximum - minimum;
    const float low =
        (float)config->minimum_duty_ticks / (float)config->period_ticks;
    const float high =
        (float)config->maximum_duty_ticks / (float)config->period_ticks;
    const float width_v = (high - low) * bus;
    if (!isfinite(phase[0]) || !isfinite(phase[1]) || !isfinite(phase[2]) ||
        !isfinite(span) || !isfinite(width_v) || width_v <= 0.0F)
        return NX_ERROR_EXHAUSTED;
    const float scale = span > width_v ? width_v / span : 1.0F;
    const float common = 0.5F * maximum + 0.5F * minimum;
    const uint32_t quantum =
        config->alignment == NX_MODULATION_CENTER_ALIGNED ? 2U : 1U;
    for (size_t phase_index = 0U; phase_index < 3U; ++phase_index) {
        float duty = 0.5F * low + 0.5F * high +
                     ((phase[phase_index] - common) * scale) / bus;
        float ticks = duty * (float)config->period_ticks / (float)quantum;
        if (!isfinite(ticks) || ticks < 0.0F ||
            ticks > (float)config->period_ticks)
            return NX_ERROR_EXHAUSTED;
        uint32_t count = (uint32_t)floorf(ticks + 0.5F) * quantum;
        /* Only endpoint rounding is repaired here; vector scaling and all
         * capture windows are determined from these final pulse widths. */
        count = count < config->minimum_duty_ticks ? config->minimum_duty_ticks
                                                   : count;
        count = count > config->maximum_duty_ticks ? config->maximum_duty_ticks
                                                   : count;
        output->duty_ticks[phase_index] = count;
        output->start_ticks[phase_index] =
            config->alignment == NX_MODULATION_CENTER_ALIGNED
                ? config->period_ticks - count / 2U
                : 0U;
    }
    const float normalized[3] = {
        (float)output->duty_ticks[0] / (float)config->period_ticks,
        (float)output->duty_ticks[1] / (float)config->period_ticks,
        (float)output->duty_ticks[2] / (float)config->period_ticks};
    output->voltage.alpha =
        bus * ((2.0F * normalized[0] - normalized[1] - normalized[2]) / 3.0F);
    output->voltage.beta =
        bus * ((normalized[1] - normalized[2]) * NX_MODULATION_INV_SQRT3);
    output->limited = scale < 1.0F;
    return isfinite(output->voltage.alpha) && isfinite(output->voltage.beta)
               ? NX_SUCCESS
               : NX_ERROR_EXHAUSTED;
}

size_t nx_modulation_windows(const nx_modulation_config_t* config,
                             const nx_modulation_output_t* plan,
                             nx_modulation_window_t out[7]) {
    edge_t edges[6];
    size_t count = 0U;
    uint8_t vector = 0U;
    for (size_t phase = 0U; phase < 3U; ++phase) {
        uint32_t start = plan->start_ticks[phase];
        uint32_t end = (start + plan->duty_ticks[phase]) % config->period_ticks;
        uint8_t bit = (uint8_t)(1U << (2U - phase));
        if ((config->period_ticks - start) % config->period_ticks <
            plan->duty_ticks[phase])
            vector |= bit;
        if (start != 0U)
            edges[count++] = (edge_t){start, bit};
        if (end != 0U)
            edges[count++] = (edge_t){end, bit};
    }
    for (size_t i = 1U; i < count; ++i) {
        edge_t edge = edges[i];
        size_t at = i;
        while (at > 0U && edges[at - 1U].tick > edge.tick) {
            edges[at] = edges[at - 1U];
            --at;
        }
        edges[at] = edge;
    }
    size_t written = 0U;
    size_t at = 0U;
    uint32_t began = 0U;
    while (at < count) {
        uint32_t tick = edges[at].tick;
        if (tick > began)
            out[written++] = (nx_modulation_window_t){began, tick, vector};
        do {
            vector ^= edges[at].mask;
            ++at;
        } while (at < count && edges[at].tick == tick);
        began = tick;
    }
    if (began < config->period_ticks)
        out[written++] =
            (nx_modulation_window_t){began, config->period_ticks, vector};
    return written;
}

bool nx_modulation_aperture(const nx_modulation_config_t* config,
                            const nx_modulation_window_t* window,
                            uint32_t not_before, bool midpoint,
                            nx_modulation_sample_t* out) {
    uint64_t begin = (uint64_t)window->begin + config->deadtime_ticks +
                     config->settling_ticks;
    begin = begin > config->capture_begin_ticks ? begin
                                                : config->capture_begin_ticks;
    const uint32_t capture_end = config->capture_end_ticks != 0U
                                     ? config->capture_end_ticks
                                     : config->period_ticks;
    const uint32_t window_end =
        window->end < capture_end ? window->end : capture_end;
    const uint64_t tail = (uint64_t)config->conversion_ticks +
                          config->irq_ticks + config->compute_ticks;
    if (begin + config->aperture_ticks > window_end ||
        tail >= config->commit_before_ticks)
        return false;
    uint32_t last = window_end - config->aperture_ticks;
    uint32_t resource_last = capture_end - config->conversion_ticks -
                             config->irq_ticks - config->rearm_ticks;
    last = last < resource_last ? last : resource_last;
    uint32_t deadline_last = config->commit_before_ticks - (uint32_t)tail - 1U;
    last = last < deadline_last ? last : deadline_last;
    uint32_t first = (uint32_t)begin;
    first = first > not_before ? first : not_before;
    if (first > last)
        return false;
    uint32_t tick = midpoint ? first + (last - first) / 2U : first;
    *out = (nx_modulation_sample_t){tick, (uint32_t)begin, window_end, 0U,
                                    window->vector};
    return true;
}

nx_result_t nx_modulation_begin(const nx_modulation_config_t* config,
                                nx_motor_ab_t requested_voltage,
                                float bus_voltage_v,
                                const nx_modulation_output_t* destination,
                                nx_modulation_output_t* next) {
    if (nx_modulation_check(config) != NX_SUCCESS ||
        !object(destination, sizeof(*destination),
                _Alignof(nx_modulation_output_t)) ||
        overlap(destination, sizeof(*destination), config, sizeof(*config)) ||
        !isfinite(requested_voltage.alpha) ||
        !isfinite(requested_voltage.beta) || !isfinite(bus_voltage_v) ||
        bus_voltage_v <= 0.0F)
        return NX_ERROR_INVALID;
    *next = (nx_modulation_output_t){0};
    return pulses(config, requested_voltage, bus_voltage_v, next);
}
