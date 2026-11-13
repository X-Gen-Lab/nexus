/**
 * \file            nand.c
 * \brief           Checked finite raw NAND dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/nand.h"
static bool span(const void* p, size_t bytes, size_t alignment) {
    return p != NULL && bytes != 0U && (uintptr_t)p % alignment == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)p;
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
static bool valid(const nx_nand_port_t* p) {
    return span(p, sizeof(*p), _Alignof(nx_nand_port_t)) &&
           span(p->ops, sizeof(*p->ops), _Alignof(nx_nand_ops_t)) &&
           p->context != NULL;
}
static bool output(const nx_nand_port_t* p, const void* data, size_t bytes,
                   size_t align) {
    return span(data, bytes, align) && !overlap(p, sizeof(*p), data, bytes) &&
           !overlap(p->ops, sizeof(*p->ops), data, bytes);
}
const nx_nand_geometry_t* nx_nand_geometry(const nx_nand_port_t* p) {
    return valid(p) && p->ops->geometry != NULL ? p->ops->geometry(p->context)
                                                : NULL;
}
nx_result_t nx_nand_prepare(nx_nand_request_t* r, nx_time_us_t deadline) {
    if (!span(r, sizeof(*r), _Alignof(nx_nand_request_t)) ||
        deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    nx_request_state_t state = nx_request_state(&r->base);
    if (state != NX_REQUEST_READY && state != NX_REQUEST_SETTLED)
        return NX_ERROR_STATE;
    nx_result_t result = nx_request_prepare(&r->base, deadline);
    if (result == NX_SUCCESS) {
        r->operation = NX_NAND_READ_PAGE;
        r->page = 0U;
        r->data = NULL;
        r->bytes = 0U;
        r->oob = NULL;
        r->oob_bytes = 0U;
        r->ecc = NULL;
        r->ecc_count = 0U;
    }
    return result;
}
nx_result_t nx_nand_id(const nx_nand_port_t* p, uint8_t* bytes, size_t count) {
    if (!valid(p) || count == 0U || count > 8U || !output(p, bytes, count, 1U))
        return NX_ERROR_INVALID;
    return p->ops->id == NULL ? NX_ERROR_UNSUPPORTED
                              : p->ops->id(p->context, bytes, count);
}
nx_result_t nx_nand_status(const nx_nand_port_t* p, nx_nand_status_t* status) {
    if (!valid(p) ||
        !output(p, status, sizeof(*status), _Alignof(nx_nand_status_t)))
        return NX_ERROR_INVALID;
    return p->ops->status == NULL ? NX_ERROR_UNSUPPORTED
                                  : p->ops->status(p->context, status);
}
/** \brief The provider describes its actual code profile without conversion. */
nx_result_t nx_nand_ecc_format(const nx_nand_port_t* p,
                               nx_nand_ecc_format_t* format) {
    if (!valid(p) ||
        !output(p, format, sizeof(*format), _Alignof(nx_nand_ecc_format_t)))
        return NX_ERROR_INVALID;
    return p->ops->ecc_format == NULL ? NX_ERROR_UNSUPPORTED
                                      : p->ops->ecc_format(p->context, format);
}
nx_result_t nx_nand_submit(const nx_nand_port_t* p,
                           nx_nand_request_t* request) {
    if (!valid(p) ||
        !output(p, request, sizeof(*request), _Alignof(nx_nand_request_t)))
        return NX_ERROR_INVALID;
    return p->ops->submit == NULL ? NX_ERROR_UNSUPPORTED
                                  : p->ops->submit(p->context, request);
}
nx_result_t nx_nand_cancel(const nx_nand_port_t* p,
                           nx_nand_request_t* request) {
    if (!valid(p) ||
        !output(p, request, sizeof(*request), _Alignof(nx_nand_request_t)))
        return NX_ERROR_INVALID;
    return p->ops->cancel == NULL ? NX_ERROR_UNSUPPORTED
                                  : p->ops->cancel(p->context, request);
}
nx_result_t nx_nand_service(const nx_nand_port_t* p) {
    if (!valid(p))
        return NX_ERROR_INVALID;
    return p->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                   : p->ops->service(p->context);
}
nx_result_t nx_nand_stop(const nx_nand_port_t* p) {
    if (!valid(p))
        return NX_ERROR_INVALID;
    return p->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                : p->ops->stop(p->context);
}
