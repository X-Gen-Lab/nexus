/**
 * \file            axis.c
 * \brief           Single-owner rotary control with finite command authority
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/control/axis.h"
#include <float.h>
#include <math.h>

static bool disjoint(const void* first, size_t first_size, const void* second,
                     size_t second_size) {
    const uintptr_t a = (uintptr_t)first;
    const uintptr_t b = (uintptr_t)second;
    if (first == NULL || second == NULL || first_size == 0 ||
        second_size == 0 || first_size - 1U > UINTPTR_MAX - a ||
        second_size - 1U > UINTPTR_MAX - b) {
        return false;
    }
    return a > b ? a - b >= second_size : b - a >= first_size;
}

static float bounded(float value, float minimum, float maximum) {
    return fminf(maximum, fmaxf(minimum, value));
}

static bool valid_parameters(const nx_axis_parameters_t* p) {
    return p != NULL && p->revision != 0 && isfinite(p->velocity_kp) &&
           p->velocity_kp >= 0 && isfinite(p->velocity_ki) &&
           p->velocity_ki >= 0 && isfinite(p->tracking_rate_per_s) &&
           p->tracking_rate_per_s >= 0 && isfinite(p->integral_limit_a) &&
           p->integral_limit_a >= 0 && isfinite(p->position_kp) &&
           p->position_kp >= 0 && isfinite(p->current_limit_a) &&
           p->current_limit_a > 0 && isfinite(p->stop_feedforward_a) &&
           fabsf(p->stop_feedforward_a) <= p->current_limit_a &&
           isfinite(p->velocity_limit_rad_s) && p->velocity_limit_rad_s > 0 &&
           isfinite(p->acceleration_rad_s2) && p->acceleration_rad_s2 > 0 &&
           isfinite(p->minimum_position_rad) &&
           isfinite(p->maximum_position_rad) &&
           p->minimum_position_rad < p->maximum_position_rad &&
           isfinite(p->position_tolerance_rad) &&
           p->position_tolerance_rad >= 0 &&
           isfinite(p->velocity_tolerance_rad_s) &&
           p->velocity_tolerance_rad_s >= 0;
}

static bool valid_feedback(const nx_axis_t* axis,
                           const nx_axis_feedback_t* feedback,
                           nx_time_us_t now) {
    return feedback != NULL && feedback->valid && feedback->sequence != 0 &&
           feedback->clock_domain == axis->config.clock_domain &&
           feedback->epoch == axis->config.epoch &&
           feedback->interval_start <= feedback->sampled_at &&
           feedback->sampled_at <= now &&
           now - feedback->sampled_at <= axis->config.maximum_feedback_age_us &&
           isfinite(feedback->position_rad) &&
           isfinite(feedback->velocity_rad_s);
}

static nx_result_t fail_axis(nx_axis_t* axis, nx_result_t result) {
    if (axis->fault == NX_SUCCESS) {
        axis->fault = result;
    }
    axis->state = NX_AXIS_FAULT;
    axis->command_live = false;
    axis->command_pending = false;
    axis->settling = false;
    return result;
}

static void apply_parameters(nx_axis_t* axis) {
    if (axis->parameters_pending) {
        axis->parameters = axis->pending_parameters;
        axis->integral_a = axis->pending_integral_a;
        axis->parameters_pending = false;
        axis->settling = false;
    }
}

nx_result_t nx_axis_initialize(nx_axis_t* axis, const nx_axis_config_t* config,
                               const nx_axis_parameters_t* parameters) {
    if (axis == NULL || config == NULL ||
        !disjoint(axis, sizeof(*axis), config, sizeof(*config)) ||
        !disjoint(axis, sizeof(*axis), parameters, sizeof(*parameters)) ||
        config->clock_domain == 0 || config->epoch == 0 ||
        config->maximum_step_us == 0 || config->maximum_feedback_age_us == 0 ||
        (config->stop_policy != NX_AXIS_INHIBIT_AFTER_DWELL &&
         config->stop_policy != NX_AXIS_HOLD_ZERO_VELOCITY) ||
        !valid_parameters(parameters)) {
        return NX_ERROR_INVALID;
    }
    nx_axis_t next = {0};
    next.config = *config;
    next.parameters = *parameters;
    next.initialized = true;
    next.state = NX_AXIS_DISABLED;
    *axis = next;
    return NX_SUCCESS;
}

nx_result_t nx_axis_enable(nx_axis_t* axis, const nx_axis_feedback_t* feedback,
                           nx_time_us_t now) {
    if (axis == NULL || feedback == NULL || now == NX_DEADLINE_NEVER ||
        !disjoint(axis, sizeof(*axis), feedback, sizeof(*feedback))) {
        return NX_ERROR_INVALID;
    }
    if (!axis->initialized || axis->state != NX_AXIS_DISABLED ||
        axis->fault != NX_SUCCESS || !valid_feedback(axis, feedback, now)) {
        return NX_ERROR_STATE;
    }
    nx_axis_t next = *axis;
    apply_parameters(&next);
    next.feedback = *feedback;
    next.last_step = now;
    next.velocity_reference_rad_s = feedback->velocity_rad_s;
    next.integral_a = 0;
    next.last_error_rad_s = 0;
    next.last_feedforward_a = 0;
    next.last_unsaturated_a = 0;
    next.last_dt_s = 0;
    next.tracked_sample_sequence = 0;
    next.command_live = false;
    next.command_pending = false;
    next.command_expired = false;
    next.settling = false;
    next.state = NX_AXIS_READY;
    next.output =
        (nx_axis_output_t){.state = NX_AXIS_READY,
                           .parameters_revision = next.parameters.revision};
    *axis = next;
    return NX_SUCCESS;
}

nx_result_t nx_axis_accept(nx_axis_t* axis, const nx_axis_command_t* command,
                           nx_time_us_t now) {
    if (axis == NULL || command == NULL || now == NX_DEADLINE_NEVER ||
        !disjoint(axis, sizeof(*axis), command, sizeof(*command)) ||
        (command->mode != NX_AXIS_CURRENT &&
         command->mode != NX_AXIS_VELOCITY &&
         command->mode != NX_AXIS_POSITION) ||
        command->sequence == 0 || command->received_at > now ||
        command->valid_until == NX_DEADLINE_NEVER ||
        !isfinite(command->position_rad) ||
        !isfinite(command->velocity_rad_s) || !isfinite(command->current_a)) {
        return NX_ERROR_INVALID;
    }
    if (now >= command->valid_until) {
        return NX_ERROR_TIMEOUT;
    }
    if (!axis->initialized ||
        (axis->state != NX_AXIS_READY && axis->state != NX_AXIS_RUNNING) ||
        axis->fault != NX_SUCCESS || now < axis->last_step ||
        command->clock_domain != axis->config.clock_domain ||
        command->epoch != axis->config.epoch ||
        command->sequence <= axis->accepted_sequence ||
        (axis->accepted_sequence != 0 &&
         command->received_at < axis->pending_command.received_at)) {
        return NX_ERROR_STATE;
    }
    if (axis->parameters_pending) {
        return NX_ERROR_BUSY;
    }
    if (axis->command_live && command->mode != axis->command.mode) {
        return NX_ERROR_STATE;
    }
    nx_trajectory_plan_t plan = {0};
    if (command->mode == NX_AXIS_POSITION) {
        if (command->velocity_rad_s != 0 ||
            command->position_rad < axis->parameters.minimum_position_rad ||
            command->position_rad > axis->parameters.maximum_position_rad) {
            return NX_ERROR_INVALID;
        }
        if ((axis->command_live && !axis->output.trajectory_complete) ||
            fabsf(axis->feedback.velocity_rad_s) >
                axis->parameters.velocity_tolerance_rad_s) {
            return NX_ERROR_BUSY;
        }
        const nx_result_t result = nx_trajectory_plan(
            &plan, axis->feedback.position_rad, command->position_rad,
            axis->parameters.velocity_limit_rad_s,
            axis->parameters.acceleration_rad_s2);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    axis->pending_command = *command;
    axis->pending_trajectory = plan;
    axis->command_pending = true;
    axis->accepted_sequence = command->sequence;
    return NX_SUCCESS;
}

static nx_result_t velocity_reference(nx_axis_t* axis,
                                      const nx_axis_feedback_t* feedback,
                                      nx_time_us_t now, float dt_s,
                                      nx_axis_output_t* out) {
    const nx_axis_parameters_t* p = &axis->parameters;
    float requested = 0;
    if (axis->state != NX_AXIS_STOPPING && axis->state != NX_AXIS_HOLDING) {
        if (axis->command.mode == NX_AXIS_POSITION) {
            nx_trajectory_sample_t sample;
            const float elapsed = (float)(now - axis->adopted_at) * 1e-6F;
            const nx_result_t result =
                nx_trajectory_evaluate(&axis->trajectory, elapsed, &sample);
            if (result != NX_SUCCESS) {
                return result;
            }
            out->reference_position_rad = sample.position;
            out->trajectory_complete = sample.complete;
            requested =
                sample.velocity +
                p->position_kp * (sample.position - feedback->position_rad);
        } else {
            requested = axis->command.velocity_rad_s;
        }
    }
    const float maximum_change = p->acceleration_rad_s2 * dt_s;
    const float limited =
        bounded(requested, -p->velocity_limit_rad_s, p->velocity_limit_rad_s);
    const float change = limited - axis->velocity_reference_rad_s;
    if (!isfinite(requested) || !isfinite(maximum_change) ||
        !isfinite(change)) {
        return NX_ERROR_EXHAUSTED;
    }
    axis->velocity_reference_rad_s +=
        bounded(change, -maximum_change, maximum_change);
    out->requested_velocity_rad_s = requested;
    out->reference_velocity_rad_s = axis->velocity_reference_rad_s;
    return NX_SUCCESS;
}

static nx_result_t calculate_current(nx_axis_t* axis,
                                     const nx_axis_feedback_t* feedback,
                                     float minimum, float maximum, float dt_s,
                                     nx_axis_output_t* out) {
    const nx_axis_parameters_t* p = &axis->parameters;
    const float error =
        axis->velocity_reference_rad_s - feedback->velocity_rad_s;
    const float feedforward =
        axis->state == NX_AXIS_STOPPING || axis->state == NX_AXIS_HOLDING
            ? p->stop_feedforward_a
            : axis->command.current_a;
    const float proportional = p->velocity_kp * error;
    const float increment = p->velocity_ki * error * dt_s;
    const float candidate = axis->integral_a + increment;
    if (!isfinite(error) || !isfinite(proportional) || !isfinite(increment) ||
        !isfinite(candidate)) {
        return NX_ERROR_EXHAUSTED;
    }
    const float proposed =
        bounded(candidate, -p->integral_limit_a, p->integral_limit_a);
    const float trial = proportional + proposed + feedforward;
    if (!isfinite(trial)) {
        return NX_ERROR_EXHAUSTED;
    }
    /* The total actuator bound includes feed-forward and downstream derating.
     * Freeze only increments driving farther into the unavailable direction. */
    if (!((trial > maximum && proposed > axis->integral_a) ||
          (trial < minimum && proposed < axis->integral_a))) {
        axis->integral_a = proposed;
    }
    const float requested = proportional + axis->integral_a + feedforward;
    if (!isfinite(requested)) {
        return NX_ERROR_EXHAUSTED;
    }
    axis->last_error_rad_s = error;
    axis->last_feedforward_a = feedforward;
    axis->last_unsaturated_a = requested;
    out->requested_current_a = requested;
    out->current_a = bounded(requested, minimum, maximum);
    return NX_SUCCESS;
}

static bool dwell(nx_axis_t* axis, bool qualifies, nx_time_us_t now) {
    if (!qualifies) {
        axis->settling = false;
        return false;
    }
    if (!axis->settling) {
        axis->settling = true;
        axis->settling_since = now;
    }
    return now - axis->settling_since >= axis->parameters.settling_us;
}

nx_result_t nx_axis_step(nx_axis_t* axis, const nx_axis_feedback_t* feedback,
                         const nx_axis_limits_t* limits, nx_time_us_t now,
                         float dt_s, nx_axis_output_t* out) {
    if (axis == NULL || feedback == NULL || limits == NULL || out == NULL ||
        !disjoint(axis, sizeof(*axis), feedback, sizeof(*feedback)) ||
        !disjoint(axis, sizeof(*axis), limits, sizeof(*limits)) ||
        !disjoint(axis, sizeof(*axis), out, sizeof(*out)) ||
        !disjoint(out, sizeof(*out), feedback, sizeof(*feedback)) ||
        !disjoint(out, sizeof(*out), limits, sizeof(*limits))) {
        return NX_ERROR_INVALID;
    }
    if (!axis->initialized || axis->state == NX_AXIS_DISABLED ||
        axis->state == NX_AXIS_FAULT) {
        return NX_ERROR_STATE;
    }
    const nx_time_us_t elapsed = now - axis->last_step;
    const float expected_dt = (float)elapsed * 1e-6F;
    if (now == NX_DEADLINE_NEVER || now <= axis->last_step ||
        elapsed > axis->config.maximum_step_us || !isfinite(dt_s) ||
        dt_s <= 0 ||
        fabsf(dt_s - expected_dt) > 1e-6F + 4 * FLT_EPSILON * expected_dt ||
        !valid_feedback(axis, feedback, now) ||
        feedback->sampled_at <= axis->feedback.sampled_at ||
        axis->feedback.sequence == UINT32_MAX ||
        feedback->sequence != axis->feedback.sequence + 1U ||
        !isfinite(limits->minimum_current_a) ||
        !isfinite(limits->maximum_current_a) || limits->minimum_current_a > 0 ||
        limits->maximum_current_a < 0) {
        return fail_axis(axis, NX_ERROR_STATE);
    }
    nx_axis_t next = *axis;
    apply_parameters(&next);
    if (next.command_pending) {
        next.command_pending = false;
        next.settling = false;
        if (now >= next.pending_command.valid_until) {
            next.command_live = false;
            next.command_expired = true;
            next.fault = NX_ERROR_TIMEOUT;
            next.state = NX_AXIS_STOPPING;
        } else {
            next.command = next.pending_command;
            next.trajectory = next.pending_trajectory;
            next.command_live = true;
            next.adopted_at = now;
            next.state = NX_AXIS_RUNNING;
        }
    }
    if (next.command_live && now >= next.command.valid_until) {
        next.command_live = false;
        next.command_expired = true;
        next.fault = NX_ERROR_TIMEOUT;
        next.state = NX_AXIS_STOPPING;
        next.settling = false;
    }
    nx_axis_output_t result = {.state = next.state,
                               .fault = next.fault,
                               .command_sequence = next.command.sequence,
                               .sample_sequence = feedback->sequence,
                               .parameters_revision = next.parameters.revision,
                               .adopted_at = next.adopted_at,
                               .reference_position_rad = feedback->position_rad,
                               .command_expired = next.command_expired};
    const float minimum =
        fmaxf(limits->minimum_current_a, -next.parameters.current_limit_a);
    const float maximum =
        fminf(limits->maximum_current_a, next.parameters.current_limit_a);
    nx_result_t status = NX_SUCCESS;
    if (next.state == NX_AXIS_RUNNING || next.state == NX_AXIS_STOPPING ||
        next.state == NX_AXIS_HOLDING) {
        if (next.state == NX_AXIS_RUNNING &&
            next.command.mode == NX_AXIS_CURRENT) {
            result.requested_current_a = next.command.current_a;
            result.current_a =
                bounded(next.command.current_a, minimum, maximum);
            next.velocity_reference_rad_s = feedback->velocity_rad_s;
        } else {
            status = velocity_reference(&next, feedback, now, dt_s, &result);
            if (status == NX_SUCCESS) {
                status = calculate_current(&next, feedback, minimum, maximum,
                                           dt_s, &result);
            }
        }
        if (status != NX_SUCCESS) {
            return fail_axis(axis, status);
        }
        if (next.state == NX_AXIS_STOPPING) {
            const bool stopped = next.velocity_reference_rad_s == 0 &&
                                 fabsf(feedback->velocity_rad_s) <=
                                     next.parameters.velocity_tolerance_rad_s;
            if (dwell(&next, stopped, now)) {
                next.command_live = false;
                if (next.config.stop_policy == NX_AXIS_HOLD_ZERO_VELOCITY) {
                    next.state = NX_AXIS_HOLDING;
                } else {
                    next.state = next.fault == NX_SUCCESS ? NX_AXIS_READY
                                                          : NX_AXIS_FAULT;
                    next.integral_a = 0;
                    result.current_a = 0;
                }
            }
        } else if (next.state == NX_AXIS_RUNNING &&
                   next.command.mode == NX_AXIS_POSITION) {
            result.target_reached = dwell(
                &next,
                result.trajectory_complete &&
                    fabsf(next.command.position_rad - feedback->position_rad) <=
                        next.parameters.position_tolerance_rad &&
                    fabsf(feedback->velocity_rad_s) <=
                        next.parameters.velocity_tolerance_rad_s,
                now);
        }
    }
    next.last_step = now;
    next.last_dt_s = dt_s;
    next.feedback = *feedback;
    result.state = next.state;
    result.fault = next.fault;
    next.output = result;
    *axis = next;
    *out = result;
    return NX_SUCCESS;
}

nx_result_t nx_axis_request_stop(nx_axis_t* axis) {
    if (axis == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!axis->initialized || axis->state == NX_AXIS_DISABLED ||
        axis->state == NX_AXIS_FAULT) {
        return NX_ERROR_STATE;
    }
    if (axis->state == NX_AXIS_READY && !axis->command_pending) {
        return NX_SUCCESS;
    }
    if (axis->state == NX_AXIS_STOPPING || axis->state == NX_AXIS_HOLDING) {
        return NX_SUCCESS;
    }
    axis->command_pending = false;
    axis->command_live = false;
    axis->settling = false;
    axis->state = NX_AXIS_STOPPING;
    return NX_SUCCESS;
}

nx_result_t nx_axis_disable(nx_axis_t* axis) {
    if (axis == NULL || !axis->initialized) {
        return NX_ERROR_INVALID;
    }
    axis->state = NX_AXIS_DISABLED;
    axis->command_live = false;
    axis->command_pending = false;
    axis->settling = false;
    axis->output.state = NX_AXIS_DISABLED;
    axis->output.current_a = 0;
    return NX_SUCCESS;
}

nx_result_t nx_axis_clear_fault(nx_axis_t* axis) {
    if (axis == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!axis->initialized || axis->state != NX_AXIS_DISABLED) {
        return NX_ERROR_STATE;
    }
    axis->fault = NX_SUCCESS;
    axis->command_expired = false;
    axis->output.fault = NX_SUCCESS;
    axis->output.command_expired = false;
    return NX_SUCCESS;
}

nx_result_t nx_axis_stage_parameters(nx_axis_t* axis,
                                     const nx_axis_parameters_t* parameters) {
    if (axis == NULL || parameters == NULL ||
        !disjoint(axis, sizeof(*axis), parameters, sizeof(*parameters)) ||
        !valid_parameters(parameters)) {
        return NX_ERROR_INVALID;
    }
    if (!axis->initialized ||
        parameters->revision <= axis->parameters.revision ||
        axis->state == NX_AXIS_STOPPING || axis->state == NX_AXIS_HOLDING ||
        axis->state == NX_AXIS_FAULT) {
        return NX_ERROR_STATE;
    }
    if (axis->parameters_pending || axis->command_pending) {
        return NX_ERROR_BUSY;
    }
    if (axis->command_live && axis->command.mode == NX_AXIS_POSITION &&
        ((axis->command.position_rad < parameters->minimum_position_rad ||
          axis->command.position_rad > parameters->maximum_position_rad) ||
         axis->feedback.position_rad < parameters->minimum_position_rad ||
         axis->feedback.position_rad > parameters->maximum_position_rad ||
         (!axis->output.trajectory_complete &&
          (parameters->velocity_limit_rad_s !=
               axis->parameters.velocity_limit_rad_s ||
           parameters->acceleration_rad_s2 !=
               axis->parameters.acceleration_rad_s2)))) {
        return NX_ERROR_STATE;
    }
    float integral = 0;
    if (axis->command_live && axis->command.mode != NX_AXIS_CURRENT) {
        integral = axis->output.current_a -
                   parameters->velocity_kp * axis->last_error_rad_s -
                   axis->last_feedforward_a;
        if (!isfinite(integral) ||
            fabsf(integral) > parameters->integral_limit_a ||
            fabsf(axis->output.current_a) > parameters->current_limit_a) {
            return NX_ERROR_INVALID;
        }
    }
    axis->pending_parameters = *parameters;
    axis->pending_integral_a = integral;
    axis->parameters_pending = true;
    return NX_SUCCESS;
}

nx_result_t nx_axis_track_current(nx_axis_t* axis, uint32_t sample_sequence,
                                  float accepted_current_a, float dt_s) {
    if (axis == NULL || !isfinite(accepted_current_a) || !isfinite(dt_s) ||
        dt_s <= 0) {
        return NX_ERROR_INVALID;
    }
    if (axis->initialized && axis->parameters_pending) {
        return NX_ERROR_BUSY;
    }
    if (!axis->initialized ||
        (axis->state != NX_AXIS_RUNNING && axis->state != NX_AXIS_STOPPING &&
         axis->state != NX_AXIS_HOLDING) ||
        sample_sequence == 0 ||
        sample_sequence != axis->output.sample_sequence ||
        sample_sequence == axis->tracked_sample_sequence ||
        fabsf(dt_s - axis->last_dt_s) >
            1e-6F + 4 * FLT_EPSILON * axis->last_dt_s ||
        fabsf(accepted_current_a) > axis->parameters.current_limit_a) {
        return NX_ERROR_STATE;
    }
    const float correction = axis->parameters.tracking_rate_per_s * dt_s *
                             (accepted_current_a - axis->last_unsaturated_a);
    const float integral = axis->integral_a + correction;
    if (!isfinite(correction) || !isfinite(integral)) {
        return NX_ERROR_EXHAUSTED;
    }
    if (axis->state == NX_AXIS_STOPPING || axis->state == NX_AXIS_HOLDING ||
        axis->command.mode != NX_AXIS_CURRENT) {
        axis->integral_a = bounded(integral, -axis->parameters.integral_limit_a,
                                   axis->parameters.integral_limit_a);
    }
    axis->tracked_sample_sequence = sample_sequence;
    return NX_SUCCESS;
}
