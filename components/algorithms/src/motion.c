/**
 * \file            motion.c
 * \brief           Bounded feedback tracking and precomputed motion profiles
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/motion.h"
#include <float.h>
#include <math.h>

nx_result_t nx_angle_wrap_f32(float radians, float* out) {
    if (out == NULL || !isfinite(radians)) {
        return NX_ERROR_INVALID;
    }
    const float pi = 3.14159265358979323846F;
    const float tau = 2.0F * pi;
    float value = fmodf(radians, tau);
    if (value < -pi) {
        value += tau;
    } else if (value >= pi) {
        value -= tau;
    }
    *out = value;
    return NX_SUCCESS;
}

static bool valid_modulus(uint64_t modulus, uint32_t maximum_delta) {
    return modulus >= 2 && modulus <= (UINT64_C(1) << 32) &&
           (uint64_t)maximum_delta * 2 < modulus;
}

nx_result_t nx_encoder_tracker_init(nx_encoder_tracker_t* tracker,
                                    uint64_t modulus, uint32_t maximum_delta,
                                    int64_t initial_position) {
    if (tracker == NULL || !valid_modulus(modulus, maximum_delta)) {
        return NX_ERROR_INVALID;
    }
    const nx_encoder_tracker_t next = {.modulus = modulus,
                                       .position_counts = initial_position,
                                       .maximum_delta = maximum_delta};
    *tracker = next;
    return NX_SUCCESS;
}

nx_result_t nx_encoder_tracker_step(nx_encoder_tracker_t* tracker, uint32_t raw,
                                    float units_per_count, float dt_s,
                                    nx_encoder_estimate_t* out) {
    if (tracker == NULL || out == NULL || !isfinite(units_per_count) ||
        units_per_count == 0 || !isfinite(dt_s) || dt_s <= 0 ||
        !valid_modulus(tracker->modulus, tracker->maximum_delta) ||
        raw >= tracker->modulus) {
        return NX_ERROR_INVALID;
    }
    int64_t delta = 0;
    float velocity = 0;
    if (tracker->initialized) {
        if (tracker->previous >= tracker->modulus) {
            return NX_ERROR_STATE;
        }
        delta = (int64_t)raw - tracker->previous;
        const int64_t modulus = (int64_t)tracker->modulus;
        if (delta * 2 > modulus) {
            delta -= modulus;
        } else if (delta * 2 < -modulus) {
            delta += modulus;
        }
        const int64_t bound = (int64_t)tracker->maximum_delta;
        if (delta > bound || delta < -bound) {
            return NX_ERROR_STATE;
        }
        if ((delta > 0 && tracker->position_counts > INT64_MAX - delta) ||
            (delta < 0 && tracker->position_counts < INT64_MIN - delta)) {
            return NX_ERROR_EXHAUSTED;
        }
        /* The admitted half-modulus bound fits signed 32-bit conversion. */
        velocity = ((float)(int32_t)delta * units_per_count) / dt_s;
        if (!isfinite(velocity)) {
            return NX_ERROR_EXHAUSTED;
        }
    }
    const nx_encoder_estimate_t sample = {
        .position_counts = tracker->position_counts + delta,
        .delta_counts = (int32_t)delta,
        .velocity = velocity,
        .velocity_valid = tracker->initialized};
    tracker->previous = raw;
    tracker->position_counts = sample.position_counts;
    tracker->initialized = true;
    *out = sample;
    return NX_SUCCESS;
}

nx_result_t nx_trajectory_plan(nx_trajectory_plan_t* plan, float start,
                               float target, float maximum_velocity,
                               float acceleration) {
    if (plan == NULL || !isfinite(start) || !isfinite(target) ||
        !isfinite(maximum_velocity) || maximum_velocity <= 0 ||
        !isfinite(acceleration) || acceleration <= 0) {
        return NX_ERROR_INVALID;
    }
    nx_trajectory_plan_t next = {
        .start = start, .target = target, .acceleration = acceleration};
    const double distance = fabs((double)target - start);
    if (distance > FLT_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    if (distance != 0) {
        double accel_s = (double)maximum_velocity / acceleration;
        double peak = maximum_velocity;
        double cruise_s = 0;
        const double ramp_distance = peak * accel_s;
        if (distance < ramp_distance) {
            accel_s = sqrt(distance / acceleration);
            peak = acceleration * accel_s;
        } else {
            cruise_s = (distance - ramp_distance) / peak;
        }
        const double total_s = 2 * accel_s + cruise_s;
        if (!isfinite(total_s) || total_s > FLT_MAX || accel_s > FLT_MAX) {
            return NX_ERROR_EXHAUSTED;
        }
        next.peak_velocity = (float)peak;
        next.accel_s = (float)accel_s;
        next.cruise_s = (float)cruise_s;
        next.total_s = (float)total_s;
        /* Each nonzero ramp must survive binary32 time quantization. */
        if (next.accel_s <= 0 || next.peak_velocity <= 0 ||
            next.total_s - next.accel_s >= next.total_s ||
            next.total_s - next.accel_s < next.accel_s) {
            return NX_ERROR_EXHAUSTED;
        }
    }
    *plan = next;
    return NX_SUCCESS;
}

static bool valid_plan(const nx_trajectory_plan_t* plan) {
    if (!isfinite(plan->start) || !isfinite(plan->target) ||
        !isfinite(plan->acceleration) || plan->acceleration <= 0 ||
        !isfinite(plan->peak_velocity) || plan->peak_velocity < 0 ||
        !isfinite(plan->accel_s) || plan->accel_s < 0 ||
        !isfinite(plan->cruise_s) || plan->cruise_s < 0 ||
        !isfinite(plan->total_s) || plan->total_s < 0) {
        return false;
    }
    if (plan->total_s == 0) {
        return plan->start == plan->target && plan->peak_velocity == 0 &&
               plan->accel_s == 0 && plan->cruise_s == 0;
    }
    return plan->start != plan->target && plan->peak_velocity > 0 &&
           plan->accel_s > 0 &&
           plan->total_s - plan->accel_s >= plan->accel_s &&
           plan->total_s - plan->accel_s < plan->total_s;
}

nx_result_t nx_trajectory_evaluate(const nx_trajectory_plan_t* plan,
                                   float elapsed_s,
                                   nx_trajectory_sample_t* out) {
    if (plan == NULL || out == NULL || !isfinite(elapsed_s) || elapsed_s < 0 ||
        !valid_plan(plan)) {
        return NX_ERROR_INVALID;
    }
    nx_trajectory_sample_t sample = {.position = plan->target,
                                     .complete = true};
    if (elapsed_s < plan->total_s) {
        const float direction = plan->target > plan->start ? 1.0F : -1.0F;
        sample.complete = false;
        if (elapsed_s < plan->accel_s) {
            const float velocity =
                plan->peak_velocity * (elapsed_s / plan->accel_s);
            sample.position =
                plan->start + direction * ((0.5F * velocity) * elapsed_s);
            sample.velocity = direction * velocity;
            sample.acceleration = direction * plan->acceleration;
        } else if (elapsed_s < plan->total_s - plan->accel_s) {
            const float distance =
                (0.5F * plan->peak_velocity) * plan->accel_s +
                plan->peak_velocity * (elapsed_s - plan->accel_s);
            sample.position = plan->start + direction * distance;
            sample.velocity = direction * plan->peak_velocity;
        } else {
            const float remaining = plan->total_s - elapsed_s;
            const float velocity =
                plan->peak_velocity * (remaining / plan->accel_s);
            sample.position =
                plan->target - direction * ((0.5F * velocity) * remaining);
            sample.velocity = direction * velocity;
            sample.acceleration = -direction * plan->acceleration;
        }
    }
    if (!isfinite(sample.position) || !isfinite(sample.velocity)) {
        return NX_ERROR_EXHAUSTED;
    }
    *out = sample;
    return NX_SUCCESS;
}
