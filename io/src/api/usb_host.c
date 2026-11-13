/**
 * \file            usb_host.c
 * \brief           USB host packet preparation and immutable face dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/usb_host.h"
#include "dispatch_checks.h"

/** \brief Prepare metadata without accessing or borrowing packet bytes. */
nx_result_t nx_usb_host_prepare(nx_usb_host_request_t* request, uint8_t* bytes,
                                size_t capacity, size_t length, bool setup,
                                bool data1, nx_time_us_t deadline) {
    if (request == NULL || capacity < length ||
        (capacity != 0U && bytes == NULL) || (setup && length != 8U)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t status = nx_request_prepare(&request->base, deadline);
    if (status != NX_SUCCESS) {
        return status;
    }
    request->bytes = bytes;
    request->capacity = capacity;
    request->length = length;
    request->setup = setup;
    request->data1 = data1;
    return NX_SUCCESS;
}

/** \brief Reject a malformed controller identity before dispatch. */
static bool valid(const nx_usb_host_port_t* port) {
    return port != NULL && port->ops != NULL && port->context != NULL;
}

/** \brief Open the selected logical pipe without hardware discovery. */
nx_result_t nx_usb_host_open(const nx_usb_host_port_t* port,
                             nx_usb_host_pipe_id_t pipe_id,
                             const nx_usb_host_pipe_t* pipe) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->open == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->open(port->context, pipe_id, pipe);
}

/** \brief An unsuccessful close retains the same pipe responsibility. */
nx_result_t nx_usb_host_close(const nx_usb_host_port_t* port,
                              nx_usb_host_pipe_id_t pipe_id) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->close == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->close(port->context, pipe_id);
}

/** \brief Expose geometry only; output cannot overwrite the interface itself.
 */
nx_result_t nx_usb_host_storage_requirements(
    const nx_usb_host_port_t* port, nx_usb_host_pipe_id_t pipe_id,
    size_t length, nx_usb_host_storage_requirements_t* requirements) {
    if (!nx_io_dispatch_span(port, sizeof(*port),
                             _Alignof(nx_usb_host_port_t)) ||
        !valid(port) ||
        !nx_io_dispatch_span(port->ops, sizeof(*port->ops),
                             _Alignof(nx_usb_host_ops_t)) ||
        !nx_io_dispatch_span(requirements, sizeof(*requirements),
                             _Alignof(nx_usb_host_storage_requirements_t)) ||
        nx_io_dispatch_overlap(requirements, sizeof(*requirements), port,
                               sizeof(*port)) ||
        nx_io_dispatch_overlap(requirements, sizeof(*requirements), port->ops,
                               sizeof(*port->ops)))
        return NX_ERROR_INVALID;
    return port->ops->requirements == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->requirements(port->context, pipe_id, length,
                                         requirements);
}

/** \brief The provider owns actual admission and DMA memory validation. */
nx_result_t nx_usb_host_submit(const nx_usb_host_port_t* port,
                               nx_usb_host_pipe_id_t pipe_id,
                               nx_usb_host_request_t* request) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->submit == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->submit(port->context, pipe_id, request);
}

/** \brief Cancellation never makes an accepted request unborrowed. */
nx_result_t nx_usb_host_cancel(const nx_usb_host_port_t* port,
                               nx_usb_host_pipe_id_t pipe_id,
                               nx_usb_host_request_t* request) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->cancel == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->cancel(port->context, pipe_id, request);
}

/** \brief Service runs only in the caller's explicitly selected executor. */
nx_result_t nx_usb_host_service(const nx_usb_host_port_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->service(port->context);
}

/** \brief Snapshot output stays under caller ownership. */
nx_result_t nx_usb_host_state(const nx_usb_host_port_t* port,
                              nx_usb_host_state_t* state) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->state == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->state(port->context, state);
}

/** \brief Port reset retains the provider's deadline and drain checks. */
nx_result_t nx_usb_host_reset(const nx_usb_host_port_t* port,
                              nx_time_us_t deadline) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->reset == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->reset(port->context, deadline);
}

/** \brief The provider proves suspend/resume timing in the Core time domain. */
nx_result_t nx_usb_host_suspend(const nx_usb_host_port_t* port, bool suspended,
                                nx_time_us_t deadline) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->suspend == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->suspend(port->context, suspended, deadline);
}

/** \brief Power withdrawal remains with the explicit external role owner. */
nx_result_t nx_usb_host_stop(const nx_usb_host_port_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}
