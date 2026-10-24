/**
 * \file            pwm_group.c
 * \brief           Checked dispatch of coherent fixed-base PWM groups
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/pwm_group.h"

/** \brief Check exclusive-end arithmetic before any public descriptor read. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && bytes != 0U && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}
/** \brief All arguments have already passed exclusive-end span checks. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief Lookup never grants context ownership or initializes hardware. */
static bool valid(const nx_pwm_group_t* port) {
    return span(port, sizeof(*port), _Alignof(nx_pwm_group_t)) &&
           span(port->ops, sizeof(*port->ops), _Alignof(nx_pwm_group_ops_t)) &&
           port->context != NULL;
}
/** \brief Public admission proves all batch descriptors readable first. */
nx_result_t nx_pwm_group_commit(const nx_pwm_group_t* port,
                                const nx_pwm_group_frame_t* frame) {
    if (!valid(port) ||
        !span(frame, sizeof(*frame), _Alignof(nx_pwm_group_frame_t)) ||
        frame->channel_count == 0U ||
        frame->channel_count > SIZE_MAX / sizeof(*frame->duty_ticks) ||
        !span(frame->duty_ticks,
              frame->channel_count * sizeof(*frame->duty_ticks),
              _Alignof(uint32_t))) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->commit == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->commit(port->context, frame);
}
/** \brief Exact shared operations table receives only its own context. */
nx_result_t nx_pwm_group_start(const nx_pwm_group_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->start == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->start(port->context);
}
/** \brief A failed provider stop retains that provider's hardware lease. */
nx_result_t nx_pwm_group_stop(const nx_pwm_group_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}
/** \brief Recovery is explicit and cannot be replaced by factory lookup. */
nx_result_t nx_pwm_group_clear_fault(const nx_pwm_group_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->clear_fault == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->clear_fault(port->context);
}
/** \brief Snapshot cannot overwrite the immutable identity or shared table. */
nx_result_t nx_pwm_group_state(const nx_pwm_group_t* port,
                               nx_pwm_group_state_t* state) {
    if (!valid(port) ||
        !span(state, sizeof(*state), _Alignof(nx_pwm_group_state_t)) ||
        overlap(port, sizeof(*port), state, sizeof(*state)) ||
        overlap(port->ops, sizeof(*port->ops), state, sizeof(*state))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->state == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->state(port->context, state);
}
