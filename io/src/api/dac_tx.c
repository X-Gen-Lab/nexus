/**
 * \file            dac_tx.c
 * \brief           Checked encoded DAC preparation and exact TX dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/dac_tx.h"

/** \brief Reject wrapping and misaligned metadata before its first access. */
static bool span(const void* pointer, size_t size, size_t alignment) {
    return pointer != NULL && (uintptr_t)pointer % alignment == 0U &&
           size <= UINTPTR_MAX - (uintptr_t)pointer;
}
/** \brief Both input extents have already passed nonwrapping admission. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief A face selects only shared methods and its stable instance state. */
static bool valid(const nx_dac_tx_port_t* port) {
    return span(port, sizeof(*port), _Alignof(nx_dac_tx_port_t)) &&
           span(port->ops, sizeof(*port->ops), _Alignof(nx_dac_tx_ops_t)) &&
           port->context != NULL;
}
/** \brief A provider must never mutate its own face or shared operation table.
 */
static bool descriptor(const nx_dac_tx_port_t* port,
                       const nx_dac_tx_request_t* request) {
    return span(request, sizeof(*request), _Alignof(nx_dac_tx_request_t)) &&
           !overlap(request, sizeof(*request), port, sizeof(*port)) &&
           !overlap(request, sizeof(*request), port->ops, sizeof(*port->ops));
}
/** \brief Static preparation cannot admit hardware or consume frame bytes. */
nx_result_t nx_dac_tx_prepare(nx_dac_tx_request_t* request, const void* data,
                              size_t length, uint32_t trigger_hz,
                              nx_time_us_t deadline) {
    if (!span(request, sizeof(*request), _Alignof(nx_dac_tx_request_t)) ||
        !span(data, length, 1U) || length == 0U || trigger_hz == 0U ||
        overlap(request, sizeof(*request), data, length)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->data = data;
        request->length = length;
        request->trigger_hz = trigger_hz;
    }
    return result;
}
/** \brief The selected provider alone establishes the retained frame loan. */
nx_result_t nx_dac_tx_port_submit(const nx_dac_tx_port_t* port,
                                  nx_dac_tx_request_t* request) {
    if (!valid(port) || !descriptor(port, request)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->submit == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->submit(port->context, request);
}
/** \brief Cancellation requests withdrawal without manufacturing settlement. */
nx_result_t nx_dac_tx_port_cancel(const nx_dac_tx_port_t* port,
                                  nx_dac_tx_request_t* request) {
    if (!valid(port) || !descriptor(port, request)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->cancel == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->cancel(port->context, request);
}
/** \brief The caller remains the only executor of provider progress. */
void nx_dac_tx_port_service(const nx_dac_tx_port_t* port) {
    if (valid(port) && port->ops->service != NULL) {
        port->ops->service(port->context);
    }
}
/** \brief Only the provider can validate and borrow its actual IRQ target. */
nx_result_t nx_dac_tx_port_attach_wake(const nx_dac_tx_port_t* port,
                                       const nx_irq_wake_t* wake,
                                       uint8_t syscall_ceiling) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->attach_wake == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->attach_wake(port->context, wake, syscall_ceiling);
}
/** \brief Preserve the provider's live deadline without inventing progress. */
nx_time_us_t nx_dac_tx_port_next_service(const nx_dac_tx_port_t* port) {
    return !valid(port) || port->ops->next_service == NULL
               ? NX_DEADLINE_NEVER
               : port->ops->next_service(port->context);
}
