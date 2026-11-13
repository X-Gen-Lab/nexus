/**
 * \file            timer_sequence.c
 * \brief           Checked typed timer sequence operations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/timer_sequence.h"
#include "dispatch_checks.h"

/** \brief Lookup validates identity and never initializes private storage. */
static bool valid(const nx_timer_sequence_port_t* port) {
    return nx_io_dispatch_span(port, sizeof(*port),
                               _Alignof(nx_timer_sequence_port_t)) &&
           nx_io_dispatch_span(port->ops, sizeof(*port->ops),
                               _Alignof(nx_timer_sequence_ops_t)) &&
           port->context != NULL;
}
/** \brief Preserve original argument identity for the provider's private
 * checks. */
static bool independent(const nx_timer_sequence_port_t* port,
                        const void* pointer, size_t bytes, size_t alignment) {
    return nx_io_dispatch_span(pointer, bytes, alignment) &&
           !nx_io_dispatch_overlap(pointer, bytes, port, sizeof(*port)) &&
           !nx_io_dispatch_overlap(pointer, bytes, port->ops,
                                   sizeof(*port->ops));
}

/** \brief Admit an already encoded native plan without copying its source. */
nx_result_t
nx_timer_sequence_port_submit(const nx_timer_sequence_port_t* port,
                              nx_timer_sequence_request_t* request) {
    if (!valid(port) || !independent(port, request, sizeof(*request),
                                     _Alignof(nx_timer_sequence_request_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->submit == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->submit(port->context, request);
}

/** \brief Request withdrawal without revoking the source or request loan. */
nx_result_t
nx_timer_sequence_port_cancel(const nx_timer_sequence_port_t* port,
                              nx_timer_sequence_request_t* request) {
    if (!valid(port) || !independent(port, request, sizeof(*request),
                                     _Alignof(nx_timer_sequence_request_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->cancel == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->cancel(port->context, request);
}

/** \brief Advance bounded tail and withdrawal work; query the request for
 * settlement. */
nx_result_t
nx_timer_sequence_port_service(const nx_timer_sequence_port_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->service(port->context);
}

/** \brief Withdraw the source; failed drain preserves every outstanding loan.
 */
nx_result_t nx_timer_sequence_port_stop(const nx_timer_sequence_port_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}

/** \brief Copy native plan constraints and current owner observations. */
nx_result_t nx_timer_sequence_port_state(const nx_timer_sequence_port_t* port,
                                         nx_timer_sequence_state_t* state) {
    if (!valid(port) || !independent(port, state, sizeof(*state),
                                     _Alignof(nx_timer_sequence_state_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->state == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->state(port->context, state);
}

/** \brief Preserve the caller's original spans for provider-specific checks. */
nx_result_t nx_timer_sequence_port_encode(const nx_timer_sequence_port_t* port,
                                          const nx_timer_sequence_step_t* steps,
                                          size_t count, void* storage,
                                          size_t capacity, size_t* bytes) {
    if (!valid(port) || count == 0U || count > SIZE_MAX / sizeof(*steps) ||
        !independent(port, steps, count * sizeof(*steps),
                     _Alignof(nx_timer_sequence_step_t)) ||
        !independent(port, storage, capacity, 1U) ||
        !independent(port, bytes, sizeof(*bytes), _Alignof(size_t)) ||
        nx_io_dispatch_overlap(storage, capacity, steps,
                               count * sizeof(*steps)) ||
        nx_io_dispatch_overlap(bytes, sizeof(*bytes), steps,
                               count * sizeof(*steps)) ||
        nx_io_dispatch_overlap(bytes, sizeof(*bytes), storage, capacity)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->encode == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->encode(port->context, steps, count, storage,
                                   capacity, bytes);
}
