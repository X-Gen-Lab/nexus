/**
 * \file            attitude.c
 * \brief           Pure calibrated vectors and selected six-axis fusion
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/attitude.h"
#include "nexus/algorithms/calibration.h"
#include <math.h>

static bool finite_vector(nx_vec3f_t value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

nx_result_t nx_vec3_calibrate(const nx_vec3_calibration_t* calibration,
                              nx_vec3f_t raw, nx_vec3f_t* out) {
    if (calibration == NULL || out == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_vec3f_t next;
    const nx_calibration_f32_t x = {calibration->offset.x,
                                    calibration->scale.x};
    const nx_calibration_f32_t y = {calibration->offset.y,
                                    calibration->scale.y};
    const nx_calibration_f32_t z = {calibration->offset.z,
                                    calibration->scale.z};
    nx_result_t result = nx_calibration_apply_f32(&x, raw.x, &next.x);
    if (result == NX_SUCCESS) {
        result = nx_calibration_apply_f32(&y, raw.y, &next.y);
    }
    if (result == NX_SUCCESS) {
        result = nx_calibration_apply_f32(&z, raw.z, &next.z);
    }
    if (result == NX_SUCCESS) {
        *out = next;
    }
    return result;
}

nx_result_t nx_quatf_normalize(nx_quatf_t value, nx_quatf_t* out) {
    if (out == NULL || !isfinite(value.w) || !isfinite(value.x) ||
        !isfinite(value.y) || !isfinite(value.z)) {
        return NX_ERROR_INVALID;
    }
    const float scale = fmaxf(fmaxf(fabsf(value.w), fabsf(value.x)),
                              fmaxf(fabsf(value.y), fabsf(value.z)));
    if (scale == 0) {
        return NX_ERROR_INVALID;
    }
    value.w /= scale;
    value.x /= scale;
    value.y /= scale;
    value.z /= scale;
    const float norm = sqrtf(value.w * value.w + value.x * value.x +
                             value.y * value.y + value.z * value.z);
    const nx_quatf_t next = {value.w / norm, value.x / norm, value.y / norm,
                             value.z / norm};
    *out = next;
    return NX_SUCCESS;
}

nx_result_t nx_quatf_rotate(nx_quatf_t rotation, nx_vec3f_t vector,
                            nx_vec3f_t* out) {
    if (out == NULL || !finite_vector(vector)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_quatf_normalize(rotation, &rotation);
    if (result != NX_SUCCESS) {
        return result;
    }
    const float tx = 2 * (rotation.y * vector.z - rotation.z * vector.y);
    const float ty = 2 * (rotation.z * vector.x - rotation.x * vector.z);
    const float tz = 2 * (rotation.x * vector.y - rotation.y * vector.x);
    const nx_vec3f_t next = {
        vector.x + rotation.w * tx + rotation.y * tz - rotation.z * ty,
        vector.y + rotation.w * ty + rotation.z * tx - rotation.x * tz,
        vector.z + rotation.w * tz + rotation.x * ty - rotation.y * tx};
    if (!finite_vector(next)) {
        return NX_ERROR_EXHAUSTED;
    }
    *out = next;
    return NX_SUCCESS;
}

nx_result_t nx_attitude6_init(nx_attitude6_t* state, nx_quatf_t initial) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_quatf_t orientation;
    const nx_result_t result = nx_quatf_normalize(initial, &orientation);
    if (result == NX_SUCCESS) {
        state->orientation = orientation;
    }
    return result;
}

static float vector_norm(nx_vec3f_t vector) {
    const float scale =
        fmaxf(fmaxf(fabsf(vector.x), fabsf(vector.y)), fabsf(vector.z));
    if (scale == 0) {
        return 0;
    }
    vector.x /= scale;
    vector.y /= scale;
    vector.z /= scale;
    return scale * sqrtf(vector.x * vector.x + vector.y * vector.y +
                         vector.z * vector.z);
}

nx_result_t nx_attitude6_step(nx_attitude6_t* state,
                              const nx_attitude6_config_t* config,
                              nx_vec3f_t gyro_rad_s,
                              nx_vec3f_t acceleration_m_s2, float dt_s,
                              nx_attitude6_result_t* out) {
    if (state == NULL || config == NULL || out == NULL ||
        !finite_vector(gyro_rad_s) || !finite_vector(acceleration_m_s2) ||
        !isfinite(dt_s) || dt_s <= 0 || !isfinite(config->gain_per_s) ||
        config->gain_per_s < 0 || !isfinite(config->acceleration_min) ||
        config->acceleration_min <= 0 || !isfinite(config->acceleration_max) ||
        config->acceleration_max < config->acceleration_min ||
        !isfinite(config->maximum_step_rad) || config->maximum_step_rad <= 0 ||
        config->maximum_step_rad > 1) {
        return NX_ERROR_INVALID;
    }
    nx_quatf_t q;
    nx_result_t result = nx_quatf_normalize(state->orientation, &q);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_vec3f_t omega = gyro_rad_s;
    const float norm = vector_norm(acceleration_m_s2);
    const bool use_acceleration = config->gain_per_s > 0 &&
                                  norm >= config->acceleration_min &&
                                  norm <= config->acceleration_max;
    if (use_acceleration) {
        const nx_vec3f_t measured = {acceleration_m_s2.x / norm,
                                     acceleration_m_s2.y / norm,
                                     acceleration_m_s2.z / norm};
        /* World +Z expressed in body axes: inverse(q) * Z * q. */
        const nx_vec3f_t predicted = {
            2 * (q.x * q.z - q.w * q.y), 2 * (q.w * q.x + q.y * q.z),
            q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z};
        omega.x += config->gain_per_s *
                   (measured.y * predicted.z - measured.z * predicted.y);
        omega.y += config->gain_per_s *
                   (measured.z * predicted.x - measured.x * predicted.z);
        omega.z += config->gain_per_s *
                   (measured.x * predicted.y - measured.y * predicted.x);
    }
    const float rate = vector_norm(omega);
    const float angle = rate * dt_s;
    if (!finite_vector(omega) || !isfinite(rate) || !isfinite(angle)) {
        return NX_ERROR_EXHAUSTED;
    }
    if (angle > config->maximum_step_rad) {
        return NX_ERROR_STATE;
    }
    nx_quatf_t next = q;
    if (rate != 0) {
        const float sine = sinf(0.5F * angle);
        const nx_quatf_t delta = {cosf(0.5F * angle), (omega.x / rate) * sine,
                                  (omega.y / rate) * sine,
                                  (omega.z / rate) * sine};
        const nx_quatf_t product = {
            q.w * delta.w - q.x * delta.x - q.y * delta.y - q.z * delta.z,
            q.w * delta.x + q.x * delta.w + q.y * delta.z - q.z * delta.y,
            q.w * delta.y - q.x * delta.z + q.y * delta.w + q.z * delta.x,
            q.w * delta.z + q.x * delta.y - q.y * delta.x + q.z * delta.w};
        result = nx_quatf_normalize(product, &next);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    const nx_attitude6_result_t estimate = {
        .orientation = next, .acceleration_used = use_acceleration};
    state->orientation = next;
    *out = estimate;
    return NX_SUCCESS;
}
