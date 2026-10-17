/**
 * \file            ethernet_frames.c
 * \brief           Exact enhanced-frame dispatch and scatter-gather admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "../private/eth_frames_validation.h"

nx_result_t nx_eth_frames_tx_prepare(nx_eth_frames_tx_request_t* request,
                                     const nx_eth_fragment_t* fragments,
                                     size_t count, bool timestamp,
                                     nx_time_us_t deadline) {
    if (!nx_eth_frames_span(request, sizeof(*request)) ||
        !nx_eth_frames_valid(fragments, count, NULL, NULL) ||
        nx_eth_frames_overlap(request, sizeof(*request), fragments,
                              count * sizeof(*fragments)))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < count; ++i)
        if (nx_eth_frames_overlap(request, sizeof(*request), fragments[i].data,
                                  fragments[i].length))
            return NX_ERROR_INVALID;
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->fragments = fragments;
        request->fragment_count = count;
        request->timestamp_requested = timestamp;
        request->stamp = (nx_io_timestamp_t){0};
        request->capture_flags = 0U;
    }
    return result;
}
static bool face_valid(const nx_eth_frames_port_t* port) {
    return nx_eth_frames_span(port, sizeof(*port)) &&
           nx_eth_frames_span(port->ops, sizeof(*port->ops)) &&
           port->context != NULL;
}
static bool face_disjoint(const nx_eth_frames_port_t* port, const void* data,
                          size_t length) {
    return face_valid(port) && nx_eth_frames_span(data, length) &&
           !nx_eth_frames_overlap(port, sizeof(*port), data, length) &&
           !nx_eth_frames_overlap(port->ops, sizeof(*port->ops), data, length);
}
#define DISPATCH(port, method, ...)                                            \
    (!face_valid(port)                                                         \
         ? NX_ERROR_INVALID                                                    \
         : ((port)->ops->method == NULL                                        \
                ? NX_ERROR_UNSUPPORTED                                         \
                : (port)->ops->method((port)->context, __VA_ARGS__)))
#define DISPATCH_ZERO(port, method)                                            \
    (!face_valid(port) ? NX_ERROR_INVALID                                      \
                       : ((port)->ops->method == NULL                          \
                              ? NX_ERROR_UNSUPPORTED                           \
                              : (port)->ops->method((port)->context)))
nx_result_t nx_eth_frames_port_submit(const nx_eth_frames_port_t* port,
                                      nx_eth_frames_tx_request_t* request) {
    return !face_disjoint(port, request, sizeof(*request))
               ? NX_ERROR_INVALID
               : DISPATCH(port, submit, request);
}
nx_result_t nx_eth_frames_port_cancel(const nx_eth_frames_port_t* port,
                                      nx_eth_frames_tx_request_t* request) {
    return request == NULL ? NX_ERROR_INVALID : DISPATCH(port, cancel, request);
}
nx_result_t nx_eth_frames_port_service(const nx_eth_frames_port_t* port) {
    return DISPATCH_ZERO(port, service);
}
nx_result_t nx_eth_frames_port_acquire(const nx_eth_frames_port_t* port,
                                       nx_eth_frames_rx_loan_t* loan,
                                       nx_eth_fragment_t* fragments,
                                       size_t capacity) {
    if (capacity == 0U || capacity > SIZE_MAX / sizeof(*fragments) ||
        !face_disjoint(port, loan, sizeof(*loan)) ||
        !face_disjoint(port, fragments, capacity * sizeof(*fragments)) ||
        nx_eth_frames_overlap(loan, sizeof(*loan), fragments,
                              capacity * sizeof(*fragments)))
        return NX_ERROR_INVALID;
    return DISPATCH(port, acquire, loan, fragments, capacity);
}
nx_result_t nx_eth_frames_port_release(const nx_eth_frames_port_t* port,
                                       const nx_eth_frames_rx_loan_t* loan) {
    return loan == NULL ? NX_ERROR_INVALID : DISPATCH(port, release, loan);
}
nx_result_t nx_eth_frames_port_state(const nx_eth_frames_port_t* port,
                                     nx_eth_frames_state_t* state) {
    return !face_disjoint(port, state, sizeof(*state))
               ? NX_ERROR_INVALID
               : DISPATCH(port, state, state);
}
nx_result_t nx_eth_frames_port_clock(const nx_eth_frames_port_t* port,
                                     nx_io_timestamp_t* stamp) {
    return !face_disjoint(port, stamp, sizeof(*stamp))
               ? NX_ERROR_INVALID
               : DISPATCH(port, clock, stamp);
}
nx_result_t nx_eth_frames_port_apply_link(const nx_eth_frames_port_t* port,
                                          const nx_eth_link_t* link) {
    return link == NULL ? NX_ERROR_INVALID : DISPATCH(port, apply_link, link);
}
nx_result_t nx_eth_frames_port_stop(const nx_eth_frames_port_t* port) {
    return DISPATCH_ZERO(port, stop);
}
nx_result_t nx_eth_frames_port_recover(const nx_eth_frames_port_t* port,
                                       nx_time_us_t deadline) {
    return DISPATCH(port, recover, deadline);
}
nx_result_t nx_eth_frames_port_attach_wake(const nx_eth_frames_port_t* port,
                                           const nx_irq_wake_t* wake,
                                           uint8_t ceiling) {
    return DISPATCH(port, attach_wake, wake, ceiling);
}
