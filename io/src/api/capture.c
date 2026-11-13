/**
 * \file            capture.c
 * \brief           Checked typed capture operations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/capture.h"
#include "dispatch_checks.h"

/** \brief Lookup validates identity and never initializes private storage. */
static bool valid(const nx_capture_port_t* port) {
    return nx_io_dispatch_span(port, sizeof(*port),
                               _Alignof(nx_capture_port_t)) &&
           nx_io_dispatch_span(port->ops, sizeof(*port->ops),
                               _Alignof(nx_capture_ops_t)) &&
           port->context != NULL;
}
/** \brief Preserve original argument identity for the provider's private
 * checks. */
static bool independent(const nx_capture_port_t* port, const void* pointer,
                        size_t bytes, size_t alignment) {
    return nx_io_dispatch_span(pointer, bytes, alignment) &&
           !nx_io_dispatch_overlap(pointer, bytes, port, sizeof(*port)) &&
           !nx_io_dispatch_overlap(pointer, bytes, port->ops,
                                   sizeof(*port->ops));
}

/** \brief Admit one snapshot using the selected source and storage contract. */
nx_result_t nx_capture_port_snapshot(const nx_capture_port_t* port,
                                     nx_capture_request_t* request) {
    if (!valid(port) || !independent(port, request, sizeof(*request),
                                     _Alignof(nx_capture_request_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->snapshot == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->snapshot(port->context, request);
}

/** \brief Start an opaque block session using caller-owned stream storage. */
nx_result_t nx_capture_port_start_blocks(const nx_capture_port_t* port,
                                         nx_stream_t* destination) {
    if (!valid(port) || !independent(port, destination, sizeof(*destination),
                                     _Alignof(nx_stream_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->start_blocks == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->start_blocks(port->context, destination);
}

/** \brief Advance bounded source progress and withdrawal without a hidden
 * worker. */
nx_result_t nx_capture_port_service(const nx_capture_port_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->service(port->context);
}

/** \brief Request snapshot withdrawal without revoking its producer loans. */
nx_result_t nx_capture_port_cancel(const nx_capture_port_t* port,
                                   nx_capture_request_t* request) {
    if (!valid(port) || !independent(port, request, sizeof(*request),
                                     _Alignof(nx_capture_request_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->cancel == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->cancel(port->context, request);
}

/** \brief Join the producer; published consumer blocks keep independent loans.
 */
nx_result_t nx_capture_port_stop(const nx_capture_port_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}

/** \brief Attempt exclusive recovery; successful recovery requires Platform
 * restart. */
nx_result_t nx_capture_port_recover(const nx_capture_port_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->recover == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->recover(port->context);
}

/** \brief Admit finite complete snapshots with the selected inter-frame gaps.
 */
nx_result_t nx_capture_port_start_frames(const nx_capture_port_t* port,
                                         nx_capture_frames_request_t* request) {
    if (!valid(port) || !independent(port, request, sizeof(*request),
                                     _Alignof(nx_capture_frames_request_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->start_frames == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->start_frames(port->context, request);
}
