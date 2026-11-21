/**
 * \file            pid.c
 * \brief           Caller-owned pid algorithms
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/pid.h"
#include <float.h>
#include <math.h>

static bool valid_config(const nx_pid_config_t* config) {
    return config != NULL && isfinite(config->kp) && isfinite(config->ki) &&
           isfinite(config->kd) && isfinite(config->tau_s) &&
           config->tau_s >= 0 && isfinite(config->output_minimum) &&
           isfinite(config->output_maximum) &&
           config->output_minimum <= config->output_maximum &&
           isfinite(config->integral_minimum) &&
           isfinite(config->integral_maximum) &&
           config->integral_minimum <= config->integral_maximum;
}

static double bounded(double value, float lower, float upper) {
    return value < lower ? lower : (value > upper ? upper : value);
}

nx_result_t nx_pid_initialize(nx_pid_t* pid, const nx_pid_config_t* config) {
    if (pid == NULL || !valid_config(config)) {
        return NX_ERROR_INVALID;
    }
    nx_pid_t next = {0};
    next.config = *config;
    next.integral =
        (float)bounded(0, config->integral_minimum, config->integral_maximum);
    next.last_output =
        (float)bounded(0, config->output_minimum, config->output_maximum);
    next.initialized = true;
    *pid = next;
    return NX_SUCCESS;
}

nx_result_t nx_pid_step(nx_pid_t* pid, float setpoint, float measurement,
                        float dt_s, float* out) {
    if (pid == NULL || out == NULL || !isfinite(setpoint) ||
        !isfinite(measurement) || !isfinite(dt_s) || dt_s <= 0) {
        return NX_ERROR_INVALID;
    }
    if (!pid->initialized || !valid_config(&pid->config)) {
        return NX_ERROR_STATE;
    }
    nx_pid_t next = *pid;
    float derivative = 0;
    if (next.config.kd == 0) {
        next.derivative.previous = measurement;
        next.derivative.filtered = 0;
        next.derivative.initialized = true;
    } else {
        const nx_result_t result =
            nx_derivative_step(&next.derivative, measurement, dt_s,
                               next.config.tau_s, &derivative);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    const nx_pid_config_t* config = &next.config;
    const double error = (double)setpoint - measurement;
    const double proposed =
        bounded((double)next.integral + config->ki * error * dt_s,
                config->integral_minimum, config->integral_maximum);
    const double proportional_derivative =
        config->kp * error - (double)config->kd * derivative;
    double raw = proportional_derivative + proposed;
    const bool further_high =
        raw > config->output_maximum && proposed > next.integral;
    const bool further_low =
        raw < config->output_minimum && proposed < next.integral;
    if (further_high) {
        const double available =
            config->output_maximum - proportional_derivative;
        if (available >= next.integral) {
            next.integral = (float)available;
        }
    } else if (further_low) {
        const double available =
            config->output_minimum - proportional_derivative;
        if (available <= next.integral) {
            next.integral = (float)available;
        }
    } else {
        next.integral = (float)proposed;
    }
    raw = proportional_derivative + next.integral;
    if (!isfinite(raw) || fabs(raw) > FLT_MAX || fabs(error) > FLT_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    next.last_error = (float)error;
    next.last_unsaturated = (float)raw;
    next.last_output =
        (float)bounded(raw, config->output_minimum, config->output_maximum);
    *pid = next;
    *out = next.last_output;
    return NX_SUCCESS;
}

nx_result_t nx_pid_track(nx_pid_t* pid, float setpoint, float measurement,
                         float output) {
    if (pid == NULL || !isfinite(setpoint) || !isfinite(measurement) ||
        !isfinite(output)) {
        return NX_ERROR_INVALID;
    }
    if (!pid->initialized) {
        return NX_ERROR_STATE;
    }
    const nx_pid_config_t* config = &pid->config;
    const double error = (double)setpoint - measurement;
    const double integral = output - config->kp * error;
    if (fabs(error) > FLT_MAX || !isfinite(integral) ||
        integral < config->integral_minimum ||
        integral > config->integral_maximum ||
        output < config->output_minimum || output > config->output_maximum) {
        return NX_ERROR_INVALID;
    }
    pid->integral = (float)integral;
    pid->derivative.previous = measurement;
    pid->derivative.filtered = 0;
    pid->derivative.initialized = true;
    pid->last_error = (float)error;
    pid->last_output = output;
    pid->last_unsaturated = output;
    return NX_SUCCESS;
}

nx_result_t nx_pid_reconfigure(nx_pid_t* pid, const nx_pid_config_t* config) {
    if (pid == NULL || !valid_config(config)) {
        return NX_ERROR_INVALID;
    }
    if (!pid->initialized || !pid->derivative.initialized) {
        return NX_ERROR_STATE;
    }
    const double integral = (double)pid->last_output -
                            (double)config->kp * pid->last_error +
                            (double)config->kd * pid->derivative.filtered;
    if (!isfinite(integral) || integral < config->integral_minimum ||
        integral > config->integral_maximum ||
        pid->last_output < config->output_minimum ||
        pid->last_output > config->output_maximum) {
        return NX_ERROR_INVALID;
    }
    const nx_pid_config_t next = *config;
    pid->config = next;
    pid->integral = (float)integral;
    pid->last_unsaturated = pid->last_output;
    return NX_SUCCESS;
}

nx_result_t nx_pid_backcalculate(nx_pid_t* pid, float actual_output,
                                 float rate_per_s, float dt_s) {
    if (pid == NULL || !isfinite(actual_output) || !isfinite(rate_per_s) ||
        !isfinite(dt_s) || rate_per_s < 0 || dt_s <= 0) {
        return NX_ERROR_INVALID;
    }
    if (!pid->initialized || !pid->derivative.initialized) {
        return NX_ERROR_STATE;
    }
    const double integral =
        pid->integral + (double)rate_per_s * dt_s *
                            ((double)actual_output - pid->last_unsaturated);
    if (!isfinite(integral)) {
        return NX_ERROR_EXHAUSTED;
    }
    pid->integral = (float)bounded(integral, pid->config.integral_minimum,
                                   pid->config.integral_maximum);
    return NX_SUCCESS;
}
