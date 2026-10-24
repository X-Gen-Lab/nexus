/**
 * \file            lptim.c
 * \brief           Checked finite-shot preparation and typed dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/lptim.h"
#include "rtc_lptim_internal.h"
/** \brief Establish the whole immutable face/table before dispatch. */
static bool face_valid(const nx_lptim_port_t* port) {
    return nx_time_span(port, sizeof(*port), _Alignof(nx_lptim_port_t)) &&
           port->context != NULL &&
           nx_time_span(port->ops, sizeof(*port->ops),
                        _Alignof(nx_lptim_ops_t));
}
/** \brief Reject whole/partial aliasing before passing writable storage. */
static bool argument_valid(const nx_lptim_port_t* port, const void* argument,
                           size_t size, size_t alignment) {
    return nx_time_span(argument, size, alignment) &&
           !nx_time_overlap(argument, size, port, sizeof(*port)) &&
           !nx_time_overlap(argument, size, port->ops, sizeof(*port->ops));
}
/** \brief Validate and snapshot typed input before base preparation writes. */
nx_result_t nx_lptim_request_prepare(nx_lptim_request_t* request,
                                     const nx_lptim_domain_t* domain,
                                     uint32_t ticks, nx_time_us_t deadline) {
    if (!nx_time_span(request, sizeof(*request),
                      _Alignof(nx_lptim_request_t)) ||
        !nx_time_span(domain, sizeof(*domain), _Alignof(nx_lptim_domain_t)) ||
        nx_time_overlap(request, sizeof(*request), domain, sizeof(*domain)) ||
        ticks == 0U || ticks > 65536U) {
        return NX_ERROR_INVALID;
    }
    nx_lptim_domain_t copied_domain = *domain;
    if (copied_domain.identity == NULL || copied_domain.generation == 0U) {
        return NX_ERROR_INVALID;
    }
    nx_result_t status = nx_request_prepare(&request->base, deadline);
    if (status != NX_SUCCESS) {
        return status;
    }
    request->domain = copied_domain;
    request->ticks = ticks;
    return NX_SUCCESS;
}
/** \brief Dispatch without treating the modulo counter as absolute time. */
nx_result_t nx_lptim_port_sample(const nx_lptim_port_t* port,
                                 nx_lptim_sample_t* sample) {
    if (!face_valid(port) || !argument_valid(port, sample, sizeof(*sample),
                                             _Alignof(nx_lptim_sample_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->sample == NULL ? NX_ERROR_UNSUPPORTED
                                     : port->ops->sample(port->context, sample);
}
/** \brief Provider admission retains sole responsibility for all effects. */
nx_result_t nx_lptim_port_submit(const nx_lptim_port_t* port,
                                 nx_lptim_request_t* request) {
    if (!face_valid(port) || !argument_valid(port, request, sizeof(*request),
                                             _Alignof(nx_lptim_request_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->submit == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->submit(port->context, request);
}
/** \brief Foreign identity is numeric until the provider proves a match. */
nx_result_t nx_lptim_port_cancel(const nx_lptim_port_t* port,
                                 nx_lptim_request_t* request) {
    if (!face_valid(port) || !argument_valid(port, request, sizeof(*request),
                                             _Alignof(nx_lptim_request_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->cancel == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->cancel(port->context, request);
}
/** \brief Preserve the typed provider's progress and drain return value. */
nx_result_t nx_lptim_port_service(const nx_lptim_port_t* port) {
    if (!face_valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->service(port->context);
}
/** \brief Stop never converts CANCELLED/TIMEOUT into storage revocation. */
nx_result_t nx_lptim_port_stop(const nx_lptim_port_t* port) {
    if (!face_valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}
