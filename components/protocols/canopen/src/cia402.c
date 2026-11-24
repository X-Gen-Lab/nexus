/**
 * \file            cia402.c
 * \brief           Observed drive states and fixed asynchronous PV objects
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/cia402.h"
#include "cia402_private.h"
#include "nexus/components/cia402_position.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

static bool overlaps(const void* first, size_t first_bytes, const void* second,
                     size_t second_bytes) {
    const uintptr_t a = (uintptr_t)first;
    const uintptr_t b = (uintptr_t)second;
    return first_bytes > UINTPTR_MAX - a || second_bytes > UINTPTR_MAX - b ||
           (a < b + second_bytes && b < a + first_bytes);
}

static bool valid_config(const nx_cia402_config_t* config) {
    return config != NULL && isfinite(config->velocity_unit_rad_s) &&
           config->velocity_unit_rad_s > 0 && config->maximum_velocity > 0 &&
           config->maximum_velocity <= INT32_MAX && config->acceleration > 0 &&
           config->deceleration > 0 && config->quick_stop_deceleration > 0 &&
           (double)config->velocity_unit_rad_s * config->maximum_velocity <=
               FLT_MAX &&
           (double)config->velocity_unit_rad_s * config->acceleration <=
               FLT_MAX &&
           (double)config->velocity_unit_rad_s * config->deceleration <=
               FLT_MAX &&
           (double)config->velocity_unit_rad_s *
                   config->quick_stop_deceleration <=
               FLT_MAX;
}

static void initialize_core(nx_cia402_core_t* core,
                            const nx_cia402_config_t* config, int8_t mode) {
    memset(core, 0, sizeof(*core));
    core->config = *config;
    core->statusword = 0x0040;
    core->state = NX_CIA402_SWITCH_ON_DISABLED;
    core->selected_mode = mode;
    core->supported_modes = mode == NX_CIA402_PROFILE_POSITION ? 1U : 4U;
    core->initialized = true;
}

nx_result_t nx_cia402_initialize(nx_cia402_t* drive,
                                 const nx_cia402_config_t* config) {
    if (drive == NULL || config == NULL ||
        overlaps(drive, sizeof(*drive), config, sizeof(*config)) ||
        !valid_config(config)) {
        return NX_ERROR_INVALID;
    }
    if (drive->core.initialized) {
        return NX_ERROR_BUSY;
    }
    memset(drive, 0, sizeof(*drive));
    initialize_core(&drive->core, config, NX_CIA402_PROFILE_VELOCITY);
    nx_cia402_core_t* core = &drive->core;
    const nx_canopen_object_t objects[NX_CIA402_OBJECT_COUNT] = {
        {0x603f, &core->error_code, 2, false, false, false},
        {0x6040, &core->controlword, 2, true, true, false},
        {0x6041, &core->statusword, 2, false, false, true},
        {0x6060, &core->requested_mode, 1, true, true, false},
        {0x6061, &core->actual_mode, 1, false, false, true},
        {0x606c, &core->actual_velocity, 4, false, false, true},
        {0x607f, &core->config.maximum_velocity, 4, false, false, false},
        {0x6083, &core->config.acceleration, 4, false, false, false},
        {0x6084, &core->config.deceleration, 4, false, false, false},
        {0x6085, &core->config.quick_stop_deceleration, 4, false, false, false},
        {0x60ff, &core->target_velocity, 4, true, true, false},
        {0x6502, &core->supported_modes, 4, false, false, false}};
    memcpy(drive->objects, objects, sizeof(objects));
    return NX_SUCCESS;
}

static nx_result_t bind_node(nx_cia402_core_t* core,
                             const nx_canopen_object_t* objects, size_t count,
                             nx_canopen_t* node,
                             const nx_canopen_config_t* transport,
                             void* workspace, size_t bytes) {
    if (node == NULL || transport == NULL || !core->initialized ||
        core->node != NULL || transport->objects != NULL ||
        transport->object_count != 0) {
        return NX_ERROR_INVALID;
    }
    nx_canopen_config_t config = *transport;
    config.objects = objects;
    config.object_count = count;
    const nx_result_t result =
        nx_canopen_init_drive(node, &config, workspace, bytes);
    if (result == NX_SUCCESS) {
        core->node = node;
    }
    return result;
}

nx_result_t nx_cia402_node_init(nx_cia402_t* drive, nx_canopen_t* node,
                                const nx_canopen_config_t* transport,
                                void* workspace, size_t bytes) {
    if (drive == NULL || node == NULL || transport == NULL ||
        overlaps(drive, sizeof(*drive), node, sizeof(*node)) ||
        overlaps(drive, sizeof(*drive), transport, sizeof(*transport)) ||
        overlaps(drive, sizeof(*drive), workspace, bytes)) {
        return NX_ERROR_INVALID;
    }
    return bind_node(&drive->core, drive->objects, NX_CIA402_OBJECT_COUNT, node,
                     transport, workspace, bytes);
}

static void latch_fault(nx_cia402_core_t* drive, uint16_t code) {
    if (!drive->fault_latched) {
        drive->error_code = code;
    }
    drive->fault_latched = true;
}

static uint16_t state_bits(nx_cia402_state_t state) {
    switch (state) {
        case NX_CIA402_READY_TO_SWITCH_ON:
            return 0x0021;
        case NX_CIA402_SWITCHED_ON:
            return 0x0023;
        case NX_CIA402_OPERATION_ENABLED:
            return 0x0027;
        case NX_CIA402_QUICK_STOP_ACTIVE:
            return 0x0007;
        case NX_CIA402_FAULT_REACTION_ACTIVE:
            return 0x000f;
        case NX_CIA402_FAULT:
            return 0x0008;
        default:
            return 0x0040;
    }
}

static bool feedback_velocity(nx_cia402_core_t* drive,
                              const nx_cia402_feedback_t* feedback) {
    if (!feedback->valid || !isfinite(feedback->velocity_rad_s)) {
        return false;
    }
    const double raw = round((double)feedback->velocity_rad_s /
                             drive->config.velocity_unit_rad_s);
    if (raw < INT32_MIN || raw > INT32_MAX) {
        return false;
    }
    drive->actual_velocity = (int32_t)raw;
    drive->actual_mode = feedback->mode;
    return true;
}

static void normal_request(nx_cia402_core_t* drive,
                           const nx_cia402_feedback_t* feedback,
                           nx_cia402_action_t* action) {
    const uint16_t control = drive->controlword;
    const bool shutdown = (control & 0x0087U) == 0x0006U;
    const bool switch_on = (control & 0x008fU) == 0x0007U;
    const bool enable = (control & 0x008fU) == 0x000fU;
    const bool quick_stop = (control & 0x0086U) == 0x0002U;
    const bool mode = drive->requested_mode == drive->selected_mode &&
                      drive->actual_mode == drive->selected_mode;
    switch (drive->state) {
        case NX_CIA402_SWITCH_ON_DISABLED:
            if (shutdown && feedback->ready) {
                drive->state = NX_CIA402_READY_TO_SWITCH_ON;
            }
            break;
        case NX_CIA402_READY_TO_SWITCH_ON:
            if ((switch_on || enable) && mode && feedback->ready) {
                action->enable_voltage = true;
                if (feedback->voltage_enabled) {
                    drive->state = NX_CIA402_SWITCHED_ON;
                }
            } else if (!shutdown && !feedback->voltage_enabled) {
                drive->state = NX_CIA402_SWITCH_ON_DISABLED;
            }
            break;
        case NX_CIA402_SWITCHED_ON:
            if ((switch_on || enable) && mode && feedback->ready) {
                action->enable_voltage = true;
                action->enable_operation = enable;
                if (enable && feedback->operation_enabled) {
                    drive->state = NX_CIA402_OPERATION_ENABLED;
                }
            } else if (!feedback->voltage_enabled &&
                       !feedback->operation_enabled) {
                drive->state = shutdown ? NX_CIA402_READY_TO_SWITCH_ON
                                        : NX_CIA402_SWITCH_ON_DISABLED;
            }
            break;
        case NX_CIA402_OPERATION_ENABLED:
        case NX_CIA402_QUICK_STOP_ACTIVE:
            if (quick_stop && mode) {
                action->enable_voltage = true;
                action->enable_operation = true;
                action->quick_stop = true;
                if (feedback->quick_stopping) {
                    drive->state = NX_CIA402_QUICK_STOP_ACTIVE;
                }
            } else if (drive->state == NX_CIA402_OPERATION_ENABLED && enable &&
                       mode && feedback->ready) {
                action->enable_voltage = true;
                action->enable_operation = true;
            } else if (drive->state == NX_CIA402_OPERATION_ENABLED &&
                       switch_on && mode) {
                action->enable_voltage = true;
                if (!feedback->operation_enabled) {
                    drive->state = NX_CIA402_SWITCHED_ON;
                }
            } else if (!feedback->voltage_enabled &&
                       !feedback->operation_enabled) {
                drive->state = shutdown ? NX_CIA402_READY_TO_SWITCH_ON
                                        : NX_CIA402_SWITCH_ON_DISABLED;
            }
            break;
        default:
            break;
    }
    action->halt = (control & 0x0100U) != 0;
    if (action->enable_operation && !action->halt && !action->quick_stop &&
        drive->state == NX_CIA402_OPERATION_ENABLED) {
        action->velocity_rad_s =
            (float)drive->target_velocity * drive->config.velocity_unit_rad_s;
    }
}

static nx_result_t update(nx_cia402_core_t* drive,
                          const nx_canopen_report_t* protocol,
                          const nx_cia402_feedback_t* feedback,
                          nx_cia402_action_t* action) {
    if (drive == NULL || protocol == NULL || feedback == NULL ||
        action == NULL || !drive->initialized ||
        overlaps(drive, sizeof(*drive), protocol, sizeof(*protocol)) ||
        overlaps(drive, sizeof(*drive), feedback, sizeof(*feedback)) ||
        overlaps(drive, sizeof(*drive), action, sizeof(*action)) ||
        overlaps(action, sizeof(*action), protocol, sizeof(*protocol)) ||
        overlaps(action, sizeof(*action), feedback, sizeof(*feedback))) {
        return NX_ERROR_INVALID;
    }
    nx_cia402_action_t next = {0};
    next.acceleration_rad_s2 =
        (float)drive->config.acceleration * drive->config.velocity_unit_rad_s;
    next.deceleration_rad_s2 =
        (float)drive->config.deceleration * drive->config.velocity_unit_rad_s;
    const bool live = protocol->nmt == NX_CANOPEN_OPERATIONAL &&
                      protocol->peer != NX_CANOPEN_PEER_TIMEOUT &&
                      protocol->error == NX_SUCCESS && !protocol->closing &&
                      protocol->reset == NX_CANOPEN_RESET_NONE;
    const bool valid = feedback_velocity(drive, feedback);
    const int64_t target = drive->target_velocity;
    const bool target_valid =
        target >= -(int64_t)drive->config.maximum_velocity &&
        target <= drive->config.maximum_velocity;
    const bool asks_power = (drive->controlword & 0x0087U) == 0x0007U;
    const bool asks_operation = (drive->controlword & 0x008fU) == 0x000fU;
    const bool observed_power_lost =
        ((drive->state == NX_CIA402_SWITCHED_ON ||
          drive->state == NX_CIA402_OPERATION_ENABLED) &&
         asks_power && !feedback->voltage_enabled) ||
        (drive->state == NX_CIA402_OPERATION_ENABLED && asks_operation &&
         !feedback->operation_enabled);
    if (!valid) {
        latch_fault(drive, 0x7300);
    } else if (feedback->fault) {
        latch_fault(drive, 0xff00);
    } else if (!target_valid) {
        latch_fault(drive, 0xff01);
    } else if ((drive->state != NX_CIA402_SWITCH_ON_DISABLED &&
                !feedback->ready) ||
               observed_power_lost) {
        latch_fault(drive, 0xff02);
    } else if ((!live && drive->state != NX_CIA402_SWITCH_ON_DISABLED &&
                drive->state != NX_CIA402_READY_TO_SWITCH_ON) ||
               protocol->error != NX_SUCCESS || protocol->closing ||
               protocol->reset != NX_CANOPEN_RESET_NONE) {
        latch_fault(drive, 0x8130);
    }
    if (!live || !valid || !target_valid) {
        drive->reset_pending = false;
    }
    const bool reset_edge = (drive->controlword & 0x0080U) != 0 &&
                            (drive->previous_controlword & 0x0080U) == 0;
    if (drive->fault_latched) {
        drive->state = feedback->voltage_enabled || feedback->operation_enabled
                           ? NX_CIA402_FAULT_REACTION_ACTIVE
                           : NX_CIA402_FAULT;
        if (reset_edge && live && valid && feedback->reset_permitted &&
            !feedback->voltage_enabled && !feedback->operation_enabled) {
            next.reset_fault = true;
            drive->reset_pending = true;
        } else if (drive->reset_pending && live && valid && target_valid &&
                   !feedback->fault && feedback->reset_permitted &&
                   !feedback->voltage_enabled && !feedback->operation_enabled) {
            drive->fault_latched = false;
            drive->reset_pending = false;
            drive->error_code = 0;
            drive->state = NX_CIA402_SWITCH_ON_DISABLED;
        }
    } else if (live) {
        normal_request(drive, feedback, &next);
    }
    if (next.quick_stop) {
        next.deceleration_rad_s2 =
            (float)drive->config.quick_stop_deceleration *
            drive->config.velocity_unit_rad_s;
    }
    drive->statusword = state_bits(drive->state);
    if (feedback->voltage_enabled) {
        drive->statusword |= 0x0010;
    }
    if (live) {
        drive->statusword |= 0x0200;
    }
    if (drive->state == NX_CIA402_OPERATION_ENABLED &&
        feedback->target_reached) {
        drive->statusword |= 0x0400;
    }
    if (feedback->limited) {
        drive->statusword |= 0x0800;
    }
    drive->previous_controlword = drive->controlword;
    *action = next;
    return NX_SUCCESS;
}

nx_result_t nx_cia402_update(nx_cia402_t* drive,
                             const nx_canopen_report_t* protocol,
                             const nx_cia402_feedback_t* feedback,
                             nx_cia402_action_t* action) {
    if (drive == NULL || protocol == NULL || feedback == NULL ||
        action == NULL ||
        overlaps(drive, sizeof(*drive), protocol, sizeof(*protocol)) ||
        overlaps(drive, sizeof(*drive), feedback, sizeof(*feedback)) ||
        overlaps(drive, sizeof(*drive), action, sizeof(*action))) {
        return NX_ERROR_INVALID;
    }
    return update(&drive->core, protocol, feedback, action);
}

nx_result_t nx_cia402_release(nx_cia402_t* drive) {
    if (drive == NULL) {
        return NX_ERROR_INVALID;
    }
    if (drive->core.node != NULL && drive->core.node->state != NULL) {
        return NX_ERROR_BUSY;
    }
    memset(drive, 0, sizeof(*drive));
    return NX_SUCCESS;
}

static bool position_config(const nx_cia402_position_config_t* config) {
    return config != NULL && valid_config(&config->axis) &&
           isfinite(config->position_unit_rad) &&
           config->position_unit_rad > 0 &&
           config->minimum_position < config->maximum_position &&
           config->profile_velocity > 0 &&
           config->profile_velocity <= config->axis.maximum_velocity &&
           config->position_window > 0 && config->epoch != 0 &&
           isfinite(config->velocity_window_rad_s) &&
           config->velocity_window_rad_s >= 0 &&
           fabs((double)config->position_unit_rad * config->minimum_position) <=
               FLT_MAX &&
           fabs((double)config->position_unit_rad * config->maximum_position) <=
               FLT_MAX;
}

nx_result_t
nx_cia402_position_initialize(nx_cia402_position_t* drive,
                              const nx_cia402_position_config_t* config) {
    if (drive == NULL || config == NULL ||
        overlaps(drive, sizeof(*drive), config, sizeof(*config)) ||
        !position_config(config)) {
        return NX_ERROR_INVALID;
    }
    if (drive->core.initialized) {
        return NX_ERROR_BUSY;
    }
    memset(drive, 0, sizeof(*drive));
    initialize_core(&drive->core, &config->axis, NX_CIA402_PROFILE_POSITION);
    drive->config = *config;
    nx_cia402_core_t* core = &drive->core;
    const nx_canopen_object_t objects[NX_CIA402_POSITION_OBJECT_COUNT] = {
        {0x603f, &core->error_code, 2, false, false, false},
        {0x6040, &core->controlword, 2, true, true, false},
        {0x6041, &core->statusword, 2, false, false, true},
        {0x6060, &core->requested_mode, 1, true, true, false},
        {0x6061, &core->actual_mode, 1, false, false, true},
        {0x6064, &drive->actual_position, 4, false, false, true},
        {0x6067, &drive->config.position_window, 4, false, false, false},
        {0x6068, &drive->config.position_window_ms, 2, false, false, false},
        {0x607a, &drive->target_position, 4, true, true, false},
        {0x607f, &core->config.maximum_velocity, 4, false, false, false},
        {0x6081, &drive->config.profile_velocity, 4, false, false, false},
        {0x6083, &core->config.acceleration, 4, false, false, false},
        {0x6084, &core->config.deceleration, 4, false, false, false},
        {0x6085, &core->config.quick_stop_deceleration, 4, false, false, false},
        {0x6502, &core->supported_modes, 4, false, false, false}};
    memcpy(drive->objects, objects, sizeof(objects));
    return NX_SUCCESS;
}

nx_result_t nx_cia402_position_node_init(nx_cia402_position_t* drive,
                                         nx_canopen_t* node,
                                         const nx_canopen_config_t* transport,
                                         void* workspace, size_t bytes) {
    if (drive == NULL || node == NULL || transport == NULL ||
        overlaps(drive, sizeof(*drive), node, sizeof(*node)) ||
        overlaps(drive, sizeof(*drive), transport, sizeof(*transport)) ||
        overlaps(drive, sizeof(*drive), workspace, bytes)) {
        return NX_ERROR_INVALID;
    }
    const nx_result_t result =
        bind_node(&drive->core, drive->objects, NX_CIA402_POSITION_OBJECT_COUNT,
                  node, transport, workspace, bytes);
    if (result == NX_SUCCESS) {
        drive->maximum_step_us = transport->maximum_step_us;
    }
    return result;
}

static bool position_feedback(nx_cia402_position_t* drive,
                              const nx_cia402_position_feedback_t* feedback) {
    if (!feedback->position_valid || !isfinite(feedback->position_rad) ||
        feedback->observed_at == NX_DEADLINE_NEVER || feedback->sequence == 0 ||
        (drive->last_sequence != 0 &&
         (drive->last_sequence == UINT32_MAX ||
          feedback->sequence != drive->last_sequence + 1 ||
          feedback->observed_at <= drive->last_observed ||
          feedback->observed_at - drive->last_observed >
              drive->maximum_step_us))) {
        return false;
    }
    const double raw =
        round((double)feedback->position_rad / drive->config.position_unit_rad);
    if (raw < INT32_MIN || raw > INT32_MAX) {
        return false;
    }
    drive->actual_position = (int32_t)raw;
    drive->last_sequence = feedback->sequence;
    drive->last_observed = feedback->observed_at;
    return true;
}

static void position_admission(nx_cia402_position_t* drive,
                               const nx_cia402_position_feedback_t* feedback,
                               bool new_edge) {
    const bool already_pending = drive->pending;
    if (new_edge && drive->core.state == NX_CIA402_OPERATION_ENABLED) {
        if ((drive->core.controlword & 0x0060U) != 0x0020U ||
            drive->target_position < drive->config.minimum_position ||
            drive->target_position > drive->config.maximum_position ||
            drive->pending || (drive->have_target && !drive->reached) ||
            drive->generation == UINT32_MAX) {
            latch_fault(&drive->core, 0xff03);
        } else {
            ++drive->generation;
            drive->retained_position = drive->target_position;
            drive->pending_since = feedback->observed_at;
            drive->pending = true;
            drive->acknowledged = false;
            drive->reached = false;
            drive->settling = false;
        }
    }
    if (already_pending && drive->pending) {
        const bool epoch =
            feedback->acknowledgement_epoch == drive->config.epoch;
        if ((epoch && feedback->rejected_generation == drive->generation) ||
            feedback->observed_at - drive->pending_since >=
                drive->maximum_step_us) {
            latch_fault(&drive->core, 0xff04);
        } else if (epoch &&
                   feedback->accepted_generation == drive->generation) {
            drive->pending = false;
            drive->have_target = true;
            drive->acknowledged = true;
        }
    }
}

static void position_settling(nx_cia402_position_t* drive,
                              const nx_cia402_position_feedback_t* feedback) {
    const double target =
        (double)drive->retained_position * drive->config.position_unit_rad;
    const double window =
        (double)drive->config.position_window * drive->config.position_unit_rad;
    const bool inside = drive->have_target && !drive->pending &&
                        drive->core.state == NX_CIA402_OPERATION_ENABLED &&
                        fabs(target - feedback->position_rad) <= window &&
                        fabsf(feedback->axis.velocity_rad_s) <=
                            drive->config.velocity_window_rad_s;
    if (!inside) {
        drive->settling = false;
        drive->reached = false;
    } else {
        if (!drive->settling) {
            drive->settling = true;
            drive->dwell_since = feedback->observed_at;
        }
        drive->reached = feedback->observed_at - drive->dwell_since >=
                         (nx_time_us_t)drive->config.position_window_ms * 1000U;
    }
}

nx_result_t
nx_cia402_position_update(nx_cia402_position_t* drive,
                          const nx_canopen_report_t* protocol,
                          const nx_cia402_position_feedback_t* feedback,
                          nx_cia402_position_action_t* action) {
    if (drive == NULL || protocol == NULL || feedback == NULL ||
        action == NULL || !drive->core.initialized ||
        drive->maximum_step_us == 0 ||
        overlaps(drive, sizeof(*drive), protocol, sizeof(*protocol)) ||
        overlaps(drive, sizeof(*drive), feedback, sizeof(*feedback)) ||
        overlaps(drive, sizeof(*drive), action, sizeof(*action)) ||
        overlaps(action, sizeof(*action), protocol, sizeof(*protocol)) ||
        overlaps(action, sizeof(*action), feedback, sizeof(*feedback))) {
        return NX_ERROR_INVALID;
    }
    const bool valid = position_feedback(drive, feedback);
    if (!valid) {
        latch_fault(&drive->core, 0x7300);
    }
    const bool setpoint = (drive->core.controlword & 0x0010U) != 0;
    if (valid && !drive->core.fault_latched) {
        position_admission(drive, feedback,
                           setpoint && !drive->previous_setpoint);
    }
    nx_cia402_position_action_t next = {0};
    nx_cia402_feedback_t axis = feedback->axis;
    axis.valid = axis.valid && valid;
    axis.target_reached = false;
    (void)update(&drive->core, protocol, &axis, &next.axis);
    if (drive->core.fault_latched ||
        drive->core.state != NX_CIA402_OPERATION_ENABLED) {
        drive->pending = false;
        drive->acknowledged = false;
        drive->have_target = false;
        drive->settling = false;
        drive->reached = false;
    } else {
        position_settling(drive, feedback);
        next.new_setpoint = drive->pending;
    }
    if (!setpoint) {
        drive->acknowledged = false;
    }
    if (drive->acknowledged) {
        drive->core.statusword |= 0x1000;
    }
    if (drive->reached) {
        drive->core.statusword |= 0x0400;
    }
    next.generation = drive->generation;
    next.epoch = drive->config.epoch;
    next.position_rad =
        (float)drive->retained_position * drive->config.position_unit_rad;
    next.maximum_velocity_rad_s = (float)drive->config.profile_velocity *
                                  drive->core.config.velocity_unit_rad_s;
    drive->previous_setpoint = setpoint;
    *action = next;
    return NX_SUCCESS;
}

nx_result_t nx_cia402_position_release(nx_cia402_position_t* drive) {
    if (drive == NULL) {
        return NX_ERROR_INVALID;
    }
    if (drive->core.node != NULL && drive->core.node->state != NULL) {
        return NX_ERROR_BUSY;
    }
    memset(drive, 0, sizeof(*drive));
    return NX_SUCCESS;
}
