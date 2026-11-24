/**
 * \file            motor.c
 * \brief           Explicit-time f32 sensored current control calculations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/control/motor.h"
#include <math.h>

#define NX_MOTOR_PI         3.14159265358979323846f
#define NX_MOTOR_TWO_PI     6.28318530717958647692f
#define NX_MOTOR_INV_SQRT3  0.57735026918962576451f
#define NX_MOTOR_SQRT3_HALF 0.86602540378443864676f

static bool overlaps(const void* first, size_t first_size, const void* second,
                     size_t second_size) {
    const uintptr_t a = (uintptr_t)first;
    const uintptr_t b = (uintptr_t)second;
    if (first_size > UINTPTR_MAX - a || second_size > UINTPTR_MAX - b) {
        return true;
    }
    return a < b + second_size && b < a + first_size;
}

static float bounded(float value, float limit) {
    return fminf(fmaxf(value, -limit), limit);
}

static float wrap_angle(float angle) {
    float result = fmodf(angle, NX_MOTOR_TWO_PI);
    if (result >= NX_MOTOR_PI) {
        result -= NX_MOTOR_TWO_PI;
    } else if (result < -NX_MOTOR_PI) {
        result += NX_MOTOR_TWO_PI;
    }
    return result;
}

static bool finite_dq(nx_motor_dq_t value) {
    return isfinite(value.d) && isfinite(value.q);
}

static bool valid_config(const nx_motor_config_t* c) {
    return c != NULL && c->pole_pairs > 0 && c->pole_pairs <= 65535 &&
           c->clock_domain != 0 && c->clock_epoch != 0 &&
           c->nominal_period_us > 0 &&
           c->maximum_gap_us >= c->nominal_period_us &&
           isfinite(c->electrical_offset_rad) && isfinite(c->kp_d) &&
           c->kp_d >= 0 && isfinite(c->ki_d) && c->ki_d >= 0 &&
           isfinite(c->kp_q) && c->kp_q >= 0 && isfinite(c->ki_q) &&
           c->ki_q >= 0 && isfinite(c->current_limit_a) &&
           c->current_limit_a > 0 && isfinite(c->phase_current_limit_a) &&
           c->phase_current_limit_a > 0 &&
           isfinite(c->current_sum_tolerance_a) &&
           c->current_sum_tolerance_a >= 0 && isfinite(c->integral_limit_v) &&
           c->integral_limit_v > 0 && isfinite(c->minimum_bus_voltage_v) &&
           c->minimum_bus_voltage_v > 0 && isfinite(c->maximum_bus_voltage_v) &&
           c->maximum_bus_voltage_v > c->minimum_bus_voltage_v &&
           isfinite(c->maximum_modulation) && c->maximum_modulation > 0 &&
           c->maximum_modulation <= 1 && isfinite(c->minimum_duty) &&
           c->minimum_duty >= 0 && c->minimum_duty < 0.5f &&
           isfinite(c->anti_windup_per_s) && c->anti_windup_per_s >= 0;
}

nx_result_t nx_motor_clarke(const float phase_a[3], nx_motor_ab_t* out) {
    if (phase_a == NULL || out == NULL ||
        overlaps(phase_a, 3 * sizeof(float), out, sizeof(*out)) ||
        !isfinite(phase_a[0]) || !isfinite(phase_a[1]) ||
        !isfinite(phase_a[2])) {
        return NX_ERROR_INVALID;
    }
    const nx_motor_ab_t next = {
        (2.0f / 3.0f) * phase_a[0] - phase_a[1] / 3.0f - phase_a[2] / 3.0f,
        phase_a[1] * NX_MOTOR_INV_SQRT3 - phase_a[2] * NX_MOTOR_INV_SQRT3};
    if (!isfinite(next.alpha) || !isfinite(next.beta)) {
        return NX_ERROR_EXHAUSTED;
    }
    *out = next;
    return NX_SUCCESS;
}

nx_result_t nx_motor_park(nx_motor_ab_t stationary, float angle_rad,
                          nx_motor_dq_t* out) {
    if (out == NULL || !isfinite(stationary.alpha) ||
        !isfinite(stationary.beta) || !isfinite(angle_rad)) {
        return NX_ERROR_INVALID;
    }
    const float sine = sinf(angle_rad);
    const float cosine = cosf(angle_rad);
    const nx_motor_dq_t next = {
        cosine * stationary.alpha + sine * stationary.beta,
        cosine * stationary.beta - sine * stationary.alpha};
    if (!finite_dq(next)) {
        return NX_ERROR_EXHAUSTED;
    }
    *out = next;
    return NX_SUCCESS;
}

nx_result_t nx_motor_inverse_park(nx_motor_dq_t rotating, float angle_rad,
                                  nx_motor_ab_t* out) {
    if (out == NULL || !finite_dq(rotating) || !isfinite(angle_rad)) {
        return NX_ERROR_INVALID;
    }
    const float sine = sinf(angle_rad);
    const float cosine = cosf(angle_rad);
    const nx_motor_ab_t next = {cosine * rotating.d - sine * rotating.q,
                                sine * rotating.d + cosine * rotating.q};
    if (!isfinite(next.alpha) || !isfinite(next.beta)) {
        return NX_ERROR_EXHAUSTED;
    }
    *out = next;
    return NX_SUCCESS;
}

nx_result_t nx_motor_initialize(nx_motor_t* motor,
                                const nx_motor_config_t* config) {
    if (motor == NULL || !valid_config(config) ||
        overlaps(motor, sizeof(*motor), config, sizeof(*config))) {
        return NX_ERROR_INVALID;
    }
    if (motor->initialized) {
        return NX_ERROR_STATE;
    }
    const nx_motor_t next = {.config = *config, .initialized = true};
    *motor = next;
    return NX_SUCCESS;
}

static nx_result_t fault(nx_motor_t* motor, nx_result_t error) {
    if (motor->fault == NX_SUCCESS) {
        motor->fault = error;
    }
    motor->track_pending = false;
    return error;
}

static nx_result_t validate_sample(const nx_motor_t* motor,
                                   const nx_motor_sample_t* s,
                                   const nx_motor_reference_t* r) {
    const nx_motor_config_t* c = &motor->config;
    const uint64_t angle_distance = s->sample_time_us >= s->angle_time_us
                                        ? s->sample_time_us - s->angle_time_us
                                        : s->angle_time_us - s->sample_time_us;
    if (!s->valid || s->clock_domain != c->clock_domain ||
        s->clock_epoch != c->clock_epoch || s->sample_time_us == UINT64_MAX ||
        s->apply_time_us == UINT64_MAX ||
        s->observed_at_us < s->sample_time_us ||
        s->observed_at_us > s->apply_time_us ||
        s->angle_time_us > s->observed_at_us ||
        angle_distance > c->maximum_angle_age_us ||
        s->apply_time_us - s->angle_time_us > c->maximum_prediction_us) {
        return NX_ERROR_STATE;
    }
    if (motor->have_sample &&
        (motor->last_sequence == UINT64_MAX ||
         s->sequence != motor->last_sequence + 1 ||
         s->sample_time_us <= motor->last_sample_time_us ||
         s->observed_at_us <= motor->last_observed_at_us)) {
        return NX_ERROR_STATE;
    }
    if (motor->have_sample &&
        s->sample_time_us - motor->last_sample_time_us > c->maximum_gap_us) {
        return NX_ERROR_TIMEOUT;
    }
    if (r->valid_until_us == UINT64_MAX ||
        r->valid_until_us <= s->apply_time_us) {
        return NX_ERROR_TIMEOUT;
    }
    if (!isfinite(s->mechanical_angle_rad) ||
        !isfinite(s->mechanical_velocity_rad_s) ||
        !isfinite(s->bus_voltage_v) ||
        s->bus_voltage_v < c->minimum_bus_voltage_v ||
        s->bus_voltage_v > c->maximum_bus_voltage_v || !isfinite(r->id_a) ||
        !isfinite(r->iq_a) || hypotf(r->id_a, r->iq_a) > c->current_limit_a) {
        return NX_ERROR_INVALID;
    }
    float sum = 0;
    for (size_t i = 0; i < 3; ++i) {
        if (!isfinite(s->phase_current_a[i]) ||
            fabsf(s->phase_current_a[i]) > c->phase_current_limit_a) {
            return NX_ERROR_INVALID;
        }
        sum += s->phase_current_a[i];
    }
    if (!isfinite(sum) || fabsf(sum) > c->current_sum_tolerance_a) {
        return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}

static nx_result_t sample_angles(const nx_motor_config_t* c,
                                 const nx_motor_sample_t* s,
                                 nx_motor_voltage_t* out) {
    const float pairs = (float)c->pole_pairs;
    const float phase =
        pairs * wrap_angle(s->mechanical_angle_rad) + c->electrical_offset_rad;
    const float speed = pairs * s->mechanical_velocity_rad_s;
    const float input_dt =
        s->sample_time_us >= s->angle_time_us
            ? (float)(s->sample_time_us - s->angle_time_us) * 0.000001f
            : -(float)(s->angle_time_us - s->sample_time_us) * 0.000001f;
    const float output_dt =
        (float)(s->apply_time_us - s->angle_time_us) * 0.000001f;
    const float input_angle = phase + speed * input_dt;
    const float output_angle = phase + speed * output_dt;
    if (!isfinite(input_angle) || !isfinite(output_angle)) {
        return NX_ERROR_EXHAUSTED;
    }
    out->sample_electrical_angle_rad = wrap_angle(input_angle);
    out->output_electrical_angle_rad = wrap_angle(output_angle);
    return NX_SUCCESS;
}

static nx_result_t voltage_plan(nx_motor_t* next,
                                const nx_motor_sample_t* sample,
                                const nx_motor_reference_t* reference,
                                bool fixed_window, nx_motor_voltage_t* out) {
    const nx_motor_config_t* c = &next->config;
    const float dt = next->last_dt_s;
    const float error_d = reference->id_a - out->measured_current.d;
    const float error_q = reference->iq_a - out->measured_current.q;
    const float integral_d = next->integral_d_v + c->ki_d * error_d * dt;
    const float integral_q = next->integral_q_v + c->ki_q * error_q * dt;
    if (!isfinite(error_d) || !isfinite(error_q) || !isfinite(integral_d) ||
        !isfinite(integral_q)) {
        return NX_ERROR_EXHAUSTED;
    }
    next->integral_d_v = bounded(integral_d, c->integral_limit_v);
    next->integral_q_v = bounded(integral_q, c->integral_limit_v);
    out->requested_voltage.d = c->kp_d * error_d + next->integral_d_v;
    out->requested_voltage.q = c->kp_q * error_q + next->integral_q_v;
    const float norm =
        hypotf(out->requested_voltage.d, out->requested_voltage.q);
    if (!finite_dq(out->requested_voltage) || !isfinite(norm)) {
        return NX_ERROR_EXHAUSTED;
    }
    const float window = fixed_window ? 1 - 2 * c->minimum_duty : 1;
    const float limit = sample->bus_voltage_v * NX_MOTOR_INV_SQRT3 *
                        c->maximum_modulation * window;
    if (!isfinite(limit) || limit <= 0) {
        return NX_ERROR_EXHAUSTED;
    }
    const float scale = norm > limit ? limit / norm : 1;
    out->limited = scale < 1;
    out->planned_voltage.d = out->requested_voltage.d * scale;
    out->planned_voltage.q = out->requested_voltage.q * scale;
    const float correction = c->anti_windup_per_s * dt;
    const float corrected_d =
        next->integral_d_v +
        correction * (out->planned_voltage.d - out->requested_voltage.d);
    const float corrected_q =
        next->integral_q_v +
        correction * (out->planned_voltage.q - out->requested_voltage.q);
    if (!isfinite(correction) || !isfinite(corrected_d) ||
        !isfinite(corrected_q)) {
        return NX_ERROR_EXHAUSTED;
    }
    next->integral_d_v = bounded(corrected_d, c->integral_limit_v);
    next->integral_q_v = bounded(corrected_q, c->integral_limit_v);
    return NX_SUCCESS;
}

/** \brief Validate external storage once before copying transactional state. */
static nx_result_t admit_step(nx_motor_t* motor,
                              const nx_motor_sample_t* sample,
                              const nx_motor_reference_t* reference,
                              const void* out, size_t out_bytes) {
    if (motor == NULL || sample == NULL || reference == NULL || out == NULL ||
        overlaps(motor, sizeof(*motor), sample, sizeof(*sample)) ||
        overlaps(motor, sizeof(*motor), reference, sizeof(*reference)) ||
        overlaps(motor, sizeof(*motor), out, out_bytes) ||
        overlaps(sample, sizeof(*sample), out, out_bytes) ||
        overlaps(reference, sizeof(*reference), out, out_bytes)) {
        return NX_ERROR_INVALID;
    }
    if (!motor->initialized || motor->fault != NX_SUCCESS) {
        return NX_ERROR_STATE;
    }
    nx_result_t result = validate_sample(motor, sample, reference);
    return result == NX_SUCCESS ? NX_SUCCESS : fault(motor, result);
}

/** \brief Advance only a private transaction, shared by both public modes. */
static nx_result_t compute_voltage(nx_motor_t* next,
                                   const nx_motor_sample_t* sample,
                                   const nx_motor_reference_t* reference,
                                   bool fixed_window,
                                   nx_motor_voltage_t* plan) {
    const uint64_t elapsed =
        next->have_sample ? sample->sample_time_us - next->last_sample_time_us
                          : next->config.nominal_period_us;
    next->last_dt_s = (float)elapsed * 0.000001f;
    nx_result_t result = sample_angles(&next->config, sample, plan);
    nx_motor_ab_t current;
    if (result == NX_SUCCESS) {
        result = nx_motor_clarke(sample->phase_current_a, &current);
    }
    if (result == NX_SUCCESS) {
        result = nx_motor_park(current, plan->sample_electrical_angle_rad,
                               &plan->measured_current);
    }
    if (result == NX_SUCCESS) {
        result = voltage_plan(next, sample, reference, fixed_window, plan);
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    next->last_planned_voltage = plan->planned_voltage;
    next->last_sample_time_us = sample->sample_time_us;
    next->last_observed_at_us = sample->observed_at_us;
    next->last_sequence = sample->sequence;
    next->have_sample = true;
    next->track_pending = true;
    plan->sequence = sample->sequence;
    return NX_SUCCESS;
}

nx_result_t nx_motor_voltage_step(nx_motor_t* motor,
                                  const nx_motor_sample_t* sample,
                                  const nx_motor_reference_t* reference,
                                  nx_motor_voltage_t* out) {
    nx_result_t result =
        admit_step(motor, sample, reference, out, sizeof(*out));
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_motor_t next = *motor;
    nx_motor_voltage_t plan = {0};
    result = compute_voltage(&next, sample, reference, false, &plan);
    if (result != NX_SUCCESS) {
        return fault(motor, result);
    }
    *motor = next;
    *out = plan;
    return NX_SUCCESS;
}

nx_result_t nx_motor_step(nx_motor_t* motor, const nx_motor_sample_t* sample,
                          const nx_motor_reference_t* reference,
                          nx_motor_output_t* out) {
    nx_result_t result =
        admit_step(motor, sample, reference, out, sizeof(*out));
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_motor_t next = *motor;
    nx_motor_voltage_t decision = {0};
    result = compute_voltage(&next, sample, reference, true, &decision);
    if (result != NX_SUCCESS) {
        return fault(motor, result);
    }
    nx_motor_ab_t voltage;
    result =
        nx_motor_inverse_park(decision.planned_voltage,
                              decision.output_electrical_angle_rad, &voltage);
    if (result != NX_SUCCESS) {
        return fault(motor, result);
    }
    nx_motor_output_t plan = {
        .measured_current = decision.measured_current,
        .requested_voltage = decision.requested_voltage,
        .planned_voltage = decision.planned_voltage,
        .sample_electrical_angle_rad = decision.sample_electrical_angle_rad,
        .output_electrical_angle_rad = decision.output_electrical_angle_rad,
        .sequence = decision.sequence,
        .limited = decision.limited};
    const float phase[3] = {
        voltage.alpha,
        -0.5f * voltage.alpha + NX_MOTOR_SQRT3_HALF * voltage.beta,
        -0.5f * voltage.alpha - NX_MOTOR_SQRT3_HALF * voltage.beta};
    const float maximum = fmaxf(phase[0], fmaxf(phase[1], phase[2]));
    const float minimum = fminf(phase[0], fminf(phase[1], phase[2]));
    const float common = 0.5f * maximum + 0.5f * minimum;
    for (size_t i = 0; i < 3; ++i) {
        const float duty = 0.5f + (phase[i] - common) / sample->bus_voltage_v;
        if (!isfinite(duty)) {
            return fault(motor, NX_ERROR_EXHAUSTED);
        }
        plan.duty[i] = fminf(fmaxf(duty, next.config.minimum_duty),
                             1 - next.config.minimum_duty);
    }
    *motor = next;
    *out = plan;
    return NX_SUCCESS;
}

nx_result_t nx_motor_track_voltage(nx_motor_t* motor, uint64_t sequence,
                                   nx_motor_dq_t planned_voltage) {
    if (motor == NULL || !finite_dq(planned_voltage)) {
        return NX_ERROR_INVALID;
    }
    if (!motor->initialized || motor->fault != NX_SUCCESS ||
        !motor->track_pending || sequence != motor->last_sequence) {
        return NX_ERROR_STATE;
    }
    const float rate = motor->config.anti_windup_per_s * motor->last_dt_s;
    const float integral_d =
        motor->integral_d_v +
        rate * (planned_voltage.d - motor->last_planned_voltage.d);
    const float integral_q =
        motor->integral_q_v +
        rate * (planned_voltage.q - motor->last_planned_voltage.q);
    if (!isfinite(integral_d) || !isfinite(integral_q)) {
        return fault(motor, NX_ERROR_EXHAUSTED);
    }
    motor->integral_d_v = bounded(integral_d, motor->config.integral_limit_v);
    motor->integral_q_v = bounded(integral_q, motor->config.integral_limit_v);
    motor->last_planned_voltage = planned_voltage;
    motor->track_pending = false;
    return NX_SUCCESS;
}

nx_result_t nx_motor_latch_fault(nx_motor_t* motor, nx_result_t reason) {
    if (motor == NULL || reason == NX_SUCCESS) {
        return NX_ERROR_INVALID;
    }
    if (!motor->initialized) {
        return NX_ERROR_STATE;
    }
    (void)fault(motor, reason);
    return NX_SUCCESS;
}

nx_result_t nx_motor_clear_fault(nx_motor_t* motor) {
    if (motor == NULL || !motor->initialized) {
        return NX_ERROR_STATE;
    }
    const nx_motor_t next = {.config = motor->config, .initialized = true};
    *motor = next;
    return NX_SUCCESS;
}

nx_result_t nx_motor_offset_begin(nx_motor_offset_t* calibration,
                                  uint32_t required_samples,
                                  float maximum_span) {
    if (calibration == NULL || required_samples == 0 ||
        required_samples > 65535 || !isfinite(maximum_span) ||
        maximum_span < 0) {
        return NX_ERROR_INVALID;
    }
    const nx_motor_offset_t next = {.maximum_span = maximum_span,
                                    .required_samples = required_samples,
                                    .initialized = true};
    *calibration = next;
    return NX_SUCCESS;
}

nx_result_t nx_motor_offset_add(nx_motor_offset_t* calibration,
                                const float raw[3]) {
    if (calibration == NULL || raw == NULL ||
        overlaps(calibration, sizeof(*calibration), raw, 3 * sizeof(float))) {
        return NX_ERROR_INVALID;
    }
    if (!calibration->initialized || calibration->rejected ||
        calibration->count >= calibration->required_samples) {
        return NX_ERROR_STATE;
    }
    nx_motor_offset_t next = *calibration;
    for (size_t i = 0; i < 3; ++i) {
        if (!isfinite(raw[i])) {
            calibration->rejected = true;
            return NX_ERROR_INVALID;
        }
        if (next.count == 0) {
            next.minimum[i] = raw[i];
            next.maximum[i] = raw[i];
            next.mean[i] = raw[i];
        } else {
            next.minimum[i] = fminf(next.minimum[i], raw[i]);
            next.maximum[i] = fmaxf(next.maximum[i], raw[i]);
            const float span = next.maximum[i] - next.minimum[i];
            if (!isfinite(span) || span > next.maximum_span) {
                calibration->rejected = true;
                return NX_ERROR_STATE;
            }
            next.mean[i] += (raw[i] - next.mean[i]) / (float)(next.count + 1);
            if (!isfinite(next.mean[i])) {
                calibration->rejected = true;
                return NX_ERROR_EXHAUSTED;
            }
        }
    }
    ++next.count;
    *calibration = next;
    return NX_SUCCESS;
}

nx_result_t nx_motor_offset_finish(const nx_motor_offset_t* calibration,
                                   float offsets[3]) {
    if (calibration == NULL || offsets == NULL ||
        overlaps(calibration, sizeof(*calibration), offsets,
                 3 * sizeof(float))) {
        return NX_ERROR_INVALID;
    }
    if (!calibration->initialized || calibration->rejected) {
        return NX_ERROR_STATE;
    }
    if (calibration->count != calibration->required_samples) {
        return NX_ERROR_BUSY;
    }
    for (size_t i = 0; i < 3; ++i) {
        offsets[i] = calibration->mean[i];
    }
    return NX_SUCCESS;
}
