/**
 * \file            flash_async.c
 * \brief           Checked optional finite Flash pulse dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/flash_async.h"
/** \brief Validate representable descriptor/face spans before dispatch. */
static bool span(const void* p, size_t n, size_t align) {
    return p != NULL && n != 0U && (uintptr_t)p % align == 0U &&
           n <= UINTPTR_MAX - (uintptr_t)p;
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
static bool valid(const nx_flash_async_port_t* p) {
    return span(p, sizeof(*p), _Alignof(nx_flash_async_port_t)) &&
           span(p->ops, sizeof(*p->ops), _Alignof(nx_flash_async_ops_t)) &&
           p->context != NULL;
}
nx_result_t nx_flash_async_prepare(nx_flash_async_request_t* r,
                                   nx_flash_async_operation_t op,
                                   uint32_t offset, const void* data,
                                   size_t length, nx_time_us_t deadline) {
    if (!span(r, sizeof(*r), _Alignof(nx_flash_async_request_t)))
        return NX_ERROR_INVALID;
    nx_request_state_t state = nx_request_state(&r->base);
    if (state != NX_REQUEST_READY && state != NX_REQUEST_SETTLED)
        return NX_ERROR_STATE;
    if ((op != NX_FLASH_ASYNC_PROGRAM && op != NX_FLASH_ASYNC_ERASE) ||
        length == 0U || length > UINT32_MAX - offset ||
        deadline == NX_DEADLINE_NEVER ||
        (op == NX_FLASH_ASYNC_ERASE ? data != NULL
                                    : (!span(data, length, 1U) ||
                                       overlap(data, length, r, sizeof(*r)))))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_request_prepare(&r->base, deadline);
    if (result == NX_SUCCESS) {
        r->operation = op;
        r->offset = offset;
        r->data = data;
        r->length = length;
    }
    return result;
}
nx_result_t nx_flash_async_submit(const nx_flash_async_port_t* port,
                                  nx_flash_async_request_t* request) {
    if (!valid(port) ||
        !span(request, sizeof(*request), _Alignof(nx_flash_async_request_t)) ||
        overlap(request, sizeof(*request), port, sizeof(*port)) ||
        overlap(request, sizeof(*request), port->ops, sizeof(*port->ops)))
        return NX_ERROR_INVALID;
    return port->ops->submit == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->submit(port->context, request);
}
nx_result_t nx_flash_async_cancel(const nx_flash_async_port_t* port,
                                  nx_flash_async_request_t* request) {
    if (!valid(port) ||
        !span(request, sizeof(*request), _Alignof(nx_flash_async_request_t)) ||
        overlap(request, sizeof(*request), port, sizeof(*port)) ||
        overlap(request, sizeof(*request), port->ops, sizeof(*port->ops)))
        return NX_ERROR_INVALID;
    return port->ops->cancel == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->cancel(port->context, request);
}
nx_result_t nx_flash_async_service(const nx_flash_async_port_t* port) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->service(port->context);
}
nx_result_t nx_flash_async_stop(const nx_flash_async_port_t* port) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}
