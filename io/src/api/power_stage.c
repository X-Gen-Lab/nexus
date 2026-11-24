/**
 * \file            power_stage.c
 * \brief           Checked caller-owned electric-drive session dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/power_stage.h"

static bool span(const void* p, size_t n, size_t alignment) {
    uintptr_t address = (uintptr_t)p;
    return p != NULL && n != 0U && address % alignment == 0U &&
           n <= UINTPTR_MAX - address;
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
static bool valid(const nx_power_stage_t* port) {
    return span(port, sizeof(*port), _Alignof(nx_power_stage_t)) &&
           span(port->ops, sizeof(*port->ops),
                _Alignof(nx_power_stage_ops_t)) &&
           port->context != NULL;
}

nx_result_t nx_power_stage_prepare(nx_power_stage_session_t* session,
                                   const nx_power_stage_plan_t* initial,
                                   nx_power_stage_compute_t compute,
                                   void* context, size_t context_bytes,
                                   nx_time_us_t startup_deadline) {
    if (!span(session, sizeof(*session), _Alignof(nx_power_stage_session_t)) ||
        !span(initial, sizeof(*initial), _Alignof(nx_power_stage_plan_t)) ||
        !span(context, context_bytes, 1U) || compute == NULL ||
        startup_deadline == NX_DEADLINE_NEVER ||
        overlap(session, sizeof(*session), initial, sizeof(*initial)) ||
        overlap(session, sizeof(*session), context, context_bytes) ||
        overlap(initial, sizeof(*initial), context, context_bytes))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_request_prepare(&session->base, NX_DEADLINE_NEVER);
    if (result == NX_SUCCESS) {
        session->startup_deadline = startup_deadline;
        session->initial = *initial;
        session->compute = compute;
        session->context = context;
        session->context_bytes = context_bytes;
    }
    return result;
}
nx_result_t nx_power_stage_start(const nx_power_stage_t* port,
                                 nx_power_stage_session_t* session) {
    if (!valid(port) ||
        !span(session, sizeof(*session), _Alignof(nx_power_stage_session_t)) ||
        overlap(session, sizeof(*session), port, sizeof(*port)) ||
        overlap(session, sizeof(*session), port->ops, sizeof(*port->ops)) ||
        !span(session->context, session->context_bytes, 1U) ||
        overlap(session->context, session->context_bytes, port,
                sizeof(*port)) ||
        overlap(session->context, session->context_bytes, port->ops,
                sizeof(*port->ops)))
        return NX_ERROR_INVALID;
    return port->ops->start == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->start(port->context, session);
}
nx_result_t nx_power_stage_service(const nx_power_stage_t* port) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->service(port->context);
}
nx_result_t nx_power_stage_stop(const nx_power_stage_t* port) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}
nx_result_t nx_power_stage_state(const nx_power_stage_t* port,
                                 nx_power_stage_state_t* state) {
    if (!valid(port) ||
        !span(state, sizeof(*state), _Alignof(nx_power_stage_state_t)) ||
        overlap(state, sizeof(*state), port, sizeof(*port)) ||
        overlap(state, sizeof(*state), port->ops, sizeof(*port->ops)))
        return NX_ERROR_INVALID;
    return port->ops->state == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->state(port->context, state);
}
