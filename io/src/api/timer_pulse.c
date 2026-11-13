/**
 * \file            timer_pulse.c
 * \brief           Checked typed timer pulse operations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/timer_pulse.h"
#include "dispatch_checks.h"

/** \brief Lookup validates identity and never initializes private storage. */
static bool valid(const nx_timer_pulse_port_t* port) {
    return nx_io_dispatch_span(port, sizeof(*port),
                               _Alignof(nx_timer_pulse_port_t)) &&
           nx_io_dispatch_span(port->ops, sizeof(*port->ops),
                               _Alignof(nx_timer_pulse_ops_t)) &&
           port->context != NULL;
}
/** \brief Preserve original argument identity for the provider's private
 * checks. */
static bool independent(const nx_timer_pulse_port_t* port, const void* pointer,
                        size_t bytes, size_t alignment) {
    return nx_io_dispatch_span(pointer, bytes, alignment) &&
           !nx_io_dispatch_overlap(pointer, bytes, port, sizeof(*port)) &&
           !nx_io_dispatch_overlap(pointer, bytes, port->ops,
                                   sizeof(*port->ops));
}

/** \brief Admit one finite pulse without changing the selected waveform shape.
 */
nx_result_t nx_timer_pulse_port_submit(const nx_timer_pulse_port_t* port,
                                       nx_timer_pulse_request_t* request) {
    if (!valid(port) || !independent(port, request, sizeof(*request),
                                     _Alignof(nx_timer_pulse_request_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->submit == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->submit(port->context, request);
}

/** \brief Request pulse withdrawal while retaining the original request loan.
 */
nx_result_t nx_timer_pulse_port_cancel(const nx_timer_pulse_port_t* port,
                                       nx_timer_pulse_request_t* request) {
    if (!valid(port) || !independent(port, request, sizeof(*request),
                                     _Alignof(nx_timer_pulse_request_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->cancel == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->cancel(port->context, request);
}

/** \brief Advance bounded completion or withdrawal; SUCCESS alone is not
 * settlement. */
nx_result_t nx_timer_pulse_port_service(const nx_timer_pulse_port_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->service(port->context);
}

/** \brief Join the finite pulse before reporting that its source has stopped.
 */
nx_result_t nx_timer_pulse_port_stop(const nx_timer_pulse_port_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}

/** \brief Copy selected units and current source observations. */
nx_result_t nx_timer_pulse_port_state(const nx_timer_pulse_port_t* port,
                                      nx_timer_pulse_state_t* state) {
    if (!valid(port) || !independent(port, state, sizeof(*state),
                                     _Alignof(nx_timer_pulse_state_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->state == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->state(port->context, state);
}
