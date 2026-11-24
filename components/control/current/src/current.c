/**
 * \file            current.c
 * \brief           Calibrated current reconstruction implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/control/current.h"
#include <math.h>

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

nx_result_t
nx_current_calibration_check(const nx_current_calibration_t* calibration) {
    if (!object(calibration, sizeof(*calibration),
                _Alignof(nx_current_calibration_t)) ||
        calibration->identity == 0U || calibration->revision == 0U ||
        calibration->maximum_code < 3U ||
        calibration->maximum_code > 0xFFFFFFU ||
        !isfinite(calibration->maximum_current_a) ||
        calibration->maximum_current_a <= 0.0F ||
        !isfinite(calibration->maximum_sum_a) ||
        calibration->maximum_sum_a < 0.0F)
        return NX_ERROR_INVALID;
    bool present = false;
    for (size_t phase = 0U; phase < 3U; ++phase) {
        if (calibration->zero_code[phase] == 0.0F &&
            calibration->a_per_code[phase] == 0.0F &&
            calibration->direction[phase] == 0)
            continue;
        present = true;
        if (!isfinite(calibration->zero_code[phase]) ||
            calibration->zero_code[phase] <= 0.0F ||
            calibration->zero_code[phase] >= (float)calibration->maximum_code ||
            !isfinite(calibration->a_per_code[phase]) ||
            calibration->a_per_code[phase] <= 0.0F ||
            (calibration->direction[phase] != 1 &&
             calibration->direction[phase] != -1))
            return NX_ERROR_INVALID;
    }
    return present ? NX_SUCCESS : NX_ERROR_INVALID;
}

static nx_result_t amperes(const nx_current_calibration_t* calibration,
                           size_t channel, uint32_t code, float* out) {
    if (calibration->a_per_code[channel] == 0.0F)
        return NX_ERROR_INVALID;
    if (code == 0U || code >= calibration->maximum_code)
        return NX_ERROR_OVERFLOW;
    float value = ((float)code - calibration->zero_code[channel]) *
                  calibration->a_per_code[channel] *
                  (float)calibration->direction[channel];
    if (!isfinite(value) || fabsf(value) > calibration->maximum_current_a)
        return NX_ERROR_IO;
    *out = value;
    return NX_SUCCESS;
}

static nx_result_t reconstruct(const nx_current_calibration_t* calibration,
                               nx_phase_current_t* current) {
    float sum = 0.0F;
    size_t missing = 3U;
    for (size_t phase = 0U; phase < 3U; ++phase) {
        if ((current->measured_mask & (1U << phase)) == 0U)
            missing = phase;
        else
            sum += current->phase_a[phase];
    }
    if (!isfinite(sum))
        return NX_ERROR_IO;
    if (missing == 3U)
        return fabsf(sum) <= calibration->maximum_sum_a ? NX_SUCCESS
                                                        : NX_ERROR_IO;
    if (fabsf(sum) > calibration->maximum_current_a)
        return NX_ERROR_IO;
    current->phase_a[missing] = -sum;
    current->reconstructed_mask = (uint8_t)(1U << missing);
    return NX_SUCCESS;
}

static nx_result_t timing(const nx_current_calibration_t* calibration,
                          nx_current_timing_t quality, uint32_t reference,
                          uint32_t bound, uint32_t first, uint32_t last,
                          nx_phase_current_t* out) {
    if ((uint32_t)quality > (uint32_t)NX_CURRENT_APERTURES_BOUNDED)
        return NX_ERROR_INVALID;
    if (quality == NX_CURRENT_APERTURES_BOUNDED) {
        if (bound == 0U)
            return NX_ERROR_STATE;
        out->reference_tick = reference;
    } else {
        uint32_t span = last - first;
        if (bound != 0U && bound < span)
            return NX_ERROR_STATE;
        bound = bound != 0U ? bound : span;
        out->reference_tick = first;
    }
    if (bound > calibration->maximum_skew_ticks)
        return NX_ERROR_STATE;
    out->skew_ticks = bound;
    out->timing = quality;
    return NX_SUCCESS;
}

nx_result_t nx_current_convert(const nx_current_calibration_t* calibration,
                               const nx_current_capture_t* capture,
                               nx_phase_current_t* out) {
    if (!object(capture, sizeof(*capture), _Alignof(nx_current_capture_t)) ||
        !object(out, sizeof(*out), _Alignof(nx_phase_current_t)) ||
        nx_current_calibration_check(calibration) != NX_SUCCESS ||
        overlap(out, sizeof(*out), calibration, sizeof(*calibration)) ||
        overlap(out, sizeof(*out), capture, sizeof(*capture)))
        return NX_ERROR_INVALID;
    uint8_t mask = capture->measured_mask;
    if ((mask != 3U && mask != 5U && mask != 6U && mask != 7U) ||
        (uint32_t)capture->timing > (uint32_t)NX_CURRENT_APERTURES_BOUNDED)
        return NX_ERROR_INVALID;
    if (!capture->valid ||
        capture->calibration_identity != calibration->identity ||
        capture->calibration_revision != calibration->revision)
        return NX_ERROR_STATE;
    nx_phase_current_t next = {.measured_mask = mask};
    uint32_t first = UINT32_MAX;
    uint32_t last = 0U;
    for (size_t phase = 0U; phase < 3U; ++phase) {
        if ((mask & (1U << phase)) == 0U)
            continue;
        nx_result_t result = amperes(calibration, phase, capture->code[phase],
                                     &next.phase_a[phase]);
        if (result != NX_SUCCESS)
            return result;
        if (capture->timing == NX_CURRENT_APERTURES_EXPLICIT) {
            uint32_t tick = capture->aperture_tick[phase];
            first = tick < first ? tick : first;
            last = tick > last ? tick : last;
        }
    }
    nx_result_t result =
        timing(calibration, capture->timing, capture->reference_tick,
               capture->skew_bound_ticks, first, last, &next);
    if (result == NX_SUCCESS)
        result = reconstruct(calibration, &next);
    if (result == NX_SUCCESS)
        *out = next;
    return result;
}

nx_result_t
nx_current_reconstruct_bus(const nx_current_calibration_t* calibration,
                           const nx_current_bus_capture_t* capture,
                           nx_phase_current_t* out) {
    if (!object(capture, sizeof(*capture),
                _Alignof(nx_current_bus_capture_t)) ||
        !object(out, sizeof(*out), _Alignof(nx_phase_current_t)) ||
        nx_current_calibration_check(calibration) != NX_SUCCESS ||
        overlap(out, sizeof(*out), calibration, sizeof(*calibration)) ||
        overlap(out, sizeof(*out), capture, sizeof(*capture)))
        return NX_ERROR_INVALID;
    if ((uint32_t)capture->timing > (uint32_t)NX_CURRENT_APERTURES_BOUNDED)
        return NX_ERROR_INVALID;
    if (!capture->valid ||
        capture->calibration_identity != calibration->identity ||
        capture->calibration_revision != calibration->revision ||
        (capture->timing == NX_CURRENT_APERTURES_EXPLICIT &&
         capture->aperture_tick[1] <= capture->aperture_tick[0]))
        return NX_ERROR_STATE;
    /* With no neutral current, an active vector observes one signed phase.
     * The two observations must identify distinct phases of the actual plan. */
    static const uint8_t phase_by_vector[8] = {0U, 2U, 1U, 0U, 0U, 1U, 2U, 0U};
    static const int8_t sign_by_vector[8] = {0, 1, 1, -1, 1, -1, -1, 0};
    uint8_t first = capture->switching_vector[0];
    uint8_t second = capture->switching_vector[1];
    if (first == 0U || first >= 7U || second == 0U || second >= 7U ||
        phase_by_vector[first] == phase_by_vector[second])
        return NX_ERROR_INVALID;
    nx_phase_current_t next = {0};
    nx_result_t result =
        timing(calibration, capture->timing, capture->reference_tick,
               capture->skew_bound_ticks,
               capture->timing == NX_CURRENT_APERTURES_EXPLICIT
                   ? capture->aperture_tick[0]
                   : 0U,
               capture->timing == NX_CURRENT_APERTURES_EXPLICIT
                   ? capture->aperture_tick[1]
                   : 0U,
               &next);
    if (result != NX_SUCCESS)
        return result;
    for (size_t sample = 0U; sample < 2U; ++sample) {
        uint8_t vector = capture->switching_vector[sample];
        uint8_t phase = phase_by_vector[vector];
        float value;
        result = amperes(calibration, 0U, capture->code[sample], &value);
        if (result != NX_SUCCESS)
            return result;
        next.phase_a[phase] = value * (float)sign_by_vector[vector];
        next.measured_mask |= (uint8_t)(1U << phase);
    }
    result = reconstruct(calibration, &next);
    if (result == NX_SUCCESS)
        *out = next;
    return result;
}
