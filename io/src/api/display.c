/**
 * \file            display.c
 * \brief           Checked scanout frame preparation and immutable dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/display.h"
#include "graphics_checks.h"

/** \brief Validate the immutable public face before following its table. */
static bool valid(const nx_display_port_t* p) {
    return nx_graphics_span(p, sizeof(*p), _Alignof(nx_display_port_t)) &&
           nx_graphics_span(p->ops, sizeof(*p->ops),
                            _Alignof(nx_display_ops_t)) &&
           p->context != NULL;
}
/** \brief Keep writable/retained controls outside both known readonly spans. */
static bool controls(const nx_display_port_t* p, const void* data,
                     size_t bytes) {
    return nx_graphics_overlap(data, bytes, p, sizeof(*p)) ||
           nx_graphics_overlap(data, bytes, p->ops, sizeof(*p->ops));
}
/** \brief Check the complete byte view without copying a pixel. */
static nx_result_t operation(const nx_display_request_t* request,
                             const nx_display_operation_t* op) {
    size_t pixel = nx_graphics_pixel_bytes(op->format);
    if (pixel == 0U || op->frame.row_bytes == 0U ||
        op->frame.row_bytes % pixel != 0U)
        return NX_ERROR_INVALID;
    nx_result_t result =
        nx_graphics_extent(op->frame.row_bytes / pixel, op->frame.rows, pixel,
                           op->frame.stride, op->frame.span_bytes);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_graphics_span(op->frame.data, op->frame.span_bytes, 1U) ||
        nx_graphics_overlap(op->frame.data, op->frame.span_bytes, request,
                            sizeof(*request)))
        return NX_ERROR_INVALID;
    return NX_SUCCESS;
}
/** \brief Copy only after every generic invariant has passed. */
nx_result_t nx_display_prepare(nx_display_request_t* request,
                               const nx_display_operation_t* op,
                               nx_time_us_t deadline) {
    if (!nx_graphics_span(request, sizeof(*request),
                          _Alignof(nx_display_request_t)) ||
        !nx_graphics_span(op, sizeof(*op), _Alignof(nx_display_operation_t)))
        return NX_ERROR_INVALID;
    nx_request_state_t state = nx_request_state(&request->base);
    if (state != NX_REQUEST_READY && state != NX_REQUEST_SETTLED)
        return NX_ERROR_STATE;
    nx_display_operation_t selected = *op;
    nx_result_t result = operation(request, &selected);
    if (result != NX_SUCCESS)
        return result;
    result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS)
        request->operation = selected;
    return result;
}
/** \brief Query output cannot overwrite the face or shared methods. */
nx_result_t nx_display_port_query(const nx_display_port_t* p, uint8_t layer,
                                  nx_display_constraints_t* result) {
    if (!valid(p) ||
        !nx_graphics_span(result, sizeof(*result),
                          _Alignof(nx_display_constraints_t)) ||
        controls(p, result, sizeof(*result)))
        return NX_ERROR_INVALID;
    return p->ops->query == NULL ? NX_ERROR_UNSUPPORTED
                                 : p->ops->query(p->context, layer, result);
}
/** \brief Public admission adds no owner and cannot repair a bad tuple. */
nx_result_t nx_display_port_submit(const nx_display_port_t* p,
                                   nx_display_request_t* request) {
    if (!valid(p) ||
        !nx_graphics_span(request, sizeof(*request),
                          _Alignof(nx_display_request_t)) ||
        controls(p, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    if (nx_request_state(&request->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    nx_result_t result = operation(request, &request->operation);
    if (result != NX_SUCCESS)
        return result;
    if (controls(p, request->operation.frame.data,
                 request->operation.frame.span_bytes))
        return NX_ERROR_INVALID;
    return p->ops->submit == NULL ? NX_ERROR_UNSUPPORTED
                                  : p->ops->submit(p->context, request);
}
/** \brief Progress belongs only to the selected explicit executor. */
nx_result_t nx_display_port_service(const nx_display_port_t* p) {
    if (!valid(p))
        return NX_ERROR_INVALID;
    return p->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                   : p->ops->service(p->context);
}
/** \brief Cancellation does not inspect borrowed pixel metadata. */
nx_result_t nx_display_port_cancel(const nx_display_port_t* p,
                                   nx_display_request_t* request) {
    if (!valid(p) ||
        !nx_graphics_span(request, sizeof(*request),
                          _Alignof(nx_display_request_t)) ||
        controls(p, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    return p->ops->cancel == NULL ? NX_ERROR_UNSUPPORTED
                                  : p->ops->cancel(p->context, request);
}
/** \brief Read observations without granting another memory lifetime. */
nx_result_t nx_display_port_snapshot(const nx_display_port_t* p, uint8_t layer,
                                     nx_display_snapshot_t* result) {
    if (!valid(p) ||
        !nx_graphics_span(result, sizeof(*result),
                          _Alignof(nx_display_snapshot_t)) ||
        controls(p, result, sizeof(*result)))
        return NX_ERROR_INVALID;
    return p->ops->snapshot == NULL
               ? NX_ERROR_UNSUPPORTED
               : p->ops->snapshot(p->context, layer, result);
}
/** \brief Only the concrete owner can prove final source withdrawal. */
nx_result_t nx_display_port_stop(const nx_display_port_t* p) {
    if (!valid(p))
        return NX_ERROR_INVALID;
    return p->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                : p->ops->stop(p->context);
}
/** \brief The provider validates full opaque storage and actual IRQ policy. */
nx_result_t nx_display_port_attach_wake(const nx_display_port_t* p,
                                        const nx_irq_wake_t* wake,
                                        uint8_t ceiling) {
    if (!valid(p) ||
        (wake != NULL &&
         (!nx_graphics_span(wake, sizeof(*wake), _Alignof(nx_irq_wake_t)) ||
          controls(p, wake, sizeof(*wake)))))
        return NX_ERROR_INVALID;
    return p->ops->attach_wake == NULL
               ? NX_ERROR_UNSUPPORTED
               : p->ops->attach_wake(p->context, wake, ceiling);
}
