/**
 * \file            image.c
 * \brief           Checked image-memory tuples and selected owner dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/image.h"
#include "graphics_checks.h"

/** \brief Validate only the public face and shared readonly method table. */
static bool valid(const nx_image_port_t* p) {
    return nx_graphics_span(p, sizeof(*p), _Alignof(nx_image_port_t)) &&
           nx_graphics_span(p->ops, sizeof(*p->ops),
                            _Alignof(nx_image_ops_t)) &&
           p->context != NULL;
}
/** \brief Known readonly spans never serve as mutable or retained payload. */
static bool controls(const nx_image_port_t* p, const void* data, size_t bytes) {
    return nx_graphics_overlap(data, bytes, p, sizeof(*p)) ||
           nx_graphics_overlap(data, bytes, p->ops, sizeof(*p->ops));
}
/** \brief Inactive source fields must not hide an unintended borrowed span. */
static bool empty(const nx_image_source_t* source) {
    return source->data == NULL && source->capacity == 0U &&
           source->stride_bytes == 0U && source->format == NX_IMAGE_RGB565;
}
/** \brief Read only a fully bounded source geometry, never pixel bytes. */
static nx_result_t source_valid(const nx_image_request_t* request,
                                const nx_image_operation_t* op,
                                const nx_image_source_t* source) {
    nx_result_t result = nx_graphics_extent(
        op->width, op->height, nx_graphics_pixel_bytes(source->format),
        source->stride_bytes, source->capacity);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_graphics_span(source->data, source->capacity, 1U) ||
        nx_graphics_overlap(source->data, source->capacity, request,
                            sizeof(*request)) ||
        nx_graphics_overlap(source->data, source->capacity,
                            op->destination.data, op->destination.capacity))
        return NX_ERROR_INVALID;
    return NX_SUCCESS;
}
/** \brief Check complete capacities, arithmetic and operation-specific use. */
static nx_result_t operation(const nx_image_request_t* request,
                             const nx_image_operation_t* op) {
    if (op->kind != NX_IMAGE_FILL && op->kind != NX_IMAGE_COPY &&
        op->kind != NX_IMAGE_CONVERT && op->kind != NX_IMAGE_BLEND)
        return NX_ERROR_INVALID;
    if ((op->foreground_alpha_mode != NX_IMAGE_ALPHA_MULTIPLY &&
         op->foreground_alpha_mode != NX_IMAGE_ALPHA_REPLACE) ||
        (op->background_alpha_mode != NX_IMAGE_ALPHA_MULTIPLY &&
         op->background_alpha_mode != NX_IMAGE_ALPHA_REPLACE) ||
        (op->kind != NX_IMAGE_BLEND &&
         (op->foreground_alpha_mode != NX_IMAGE_ALPHA_MULTIPLY ||
          op->background_alpha_mode != NX_IMAGE_ALPHA_MULTIPLY)))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_graphics_extent(
        op->width, op->height, nx_graphics_pixel_bytes(op->destination.format),
        op->destination.stride_bytes, op->destination.capacity);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_graphics_span(op->destination.data, op->destination.capacity, 1U) ||
        nx_graphics_overlap(op->destination.data, op->destination.capacity,
                            request, sizeof(*request)) ||
        (op->kind != NX_IMAGE_BLEND &&
         (op->foreground_alpha != 0U || op->background_alpha != 0U)))
        return NX_ERROR_INVALID;
    if (op->kind == NX_IMAGE_FILL)
        return empty(&op->foreground) && empty(&op->background) &&
                       (op->destination.format != NX_IMAGE_RGB565 ||
                        op->fill_value <= UINT16_MAX)
                   ? NX_SUCCESS
                   : NX_ERROR_INVALID;
    if (op->fill_value != 0U ||
        (op->kind != NX_IMAGE_BLEND && !empty(&op->background)) ||
        (op->kind == NX_IMAGE_COPY &&
         op->foreground.format != op->destination.format))
        return NX_ERROR_INVALID;
    result = source_valid(request, op, &op->foreground);
    return result == NX_SUCCESS && op->kind == NX_IMAGE_BLEND
               ? source_valid(request, op, &op->background)
               : result;
}
/** \brief Commit metadata only after all generic checks pass. */
nx_result_t nx_image_prepare(nx_image_request_t* request,
                             const nx_image_operation_t* op,
                             nx_time_us_t deadline) {
    if (!nx_graphics_span(request, sizeof(*request),
                          _Alignof(nx_image_request_t)) ||
        !nx_graphics_span(op, sizeof(*op), _Alignof(nx_image_operation_t)))
        return NX_ERROR_INVALID;
    nx_request_state_t state = nx_request_state(&request->base);
    if (state != NX_REQUEST_READY && state != NX_REQUEST_SETTLED)
        return NX_ERROR_STATE;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    nx_image_operation_t selected = *op;
    nx_result_t result = operation(request, &selected);
    if (result != NX_SUCCESS)
        return result;
    result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS)
        request->operation = selected;
    return result;
}
/** \brief Query cannot mutate known public readonly storage. */
nx_result_t nx_image_port_query(const nx_image_port_t* p, nx_image_kind_t kind,
                                nx_image_format_t foreground,
                                nx_image_format_t background,
                                nx_image_format_t destination,
                                nx_image_constraints_t* result) {
    if (!valid(p) ||
        !nx_graphics_span(result, sizeof(*result),
                          _Alignof(nx_image_constraints_t)) ||
        controls(p, result, sizeof(*result)))
        return NX_ERROR_INVALID;
    return p->ops->query == NULL
               ? NX_ERROR_UNSUPPORTED
               : p->ops->query(p->context, kind, foreground, background,
                               destination, result);
}
/** \brief Keep the public wrapper free of acquired resources or fallback. */
nx_result_t nx_image_port_submit(const nx_image_port_t* p,
                                 nx_image_request_t* request) {
    if (!valid(p) ||
        !nx_graphics_span(request, sizeof(*request),
                          _Alignof(nx_image_request_t)) ||
        controls(p, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    if (nx_request_state(&request->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    nx_result_t result = operation(request, &request->operation);
    if (result != NX_SUCCESS)
        return result;
    const nx_image_operation_t* op = &request->operation;
    if (controls(p, op->destination.data, op->destination.capacity) ||
        (op->kind != NX_IMAGE_FILL &&
         controls(p, op->foreground.data, op->foreground.capacity)) ||
        (op->kind == NX_IMAGE_BLEND &&
         controls(p, op->background.data, op->background.capacity)))
        return NX_ERROR_INVALID;
    return p->ops->submit == NULL ? NX_ERROR_UNSUPPORTED
                                  : p->ops->submit(p->context, request);
}
/** \brief One explicit executor drives service without hidden work. */
nx_result_t nx_image_port_service(const nx_image_port_t* p) {
    if (!valid(p))
        return NX_ERROR_INVALID;
    return p->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                   : p->ops->service(p->context);
}
/** \brief Cancel does not inspect still-borrowed operation fields. */
nx_result_t nx_image_port_cancel(const nx_image_port_t* p,
                                 nx_image_request_t* request) {
    if (!valid(p) ||
        !nx_graphics_span(request, sizeof(*request),
                          _Alignof(nx_image_request_t)) ||
        controls(p, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    return p->ops->cancel == NULL ? NX_ERROR_UNSUPPORTED
                                  : p->ops->cancel(p->context, request);
}
/** \brief Only the concrete owner can release the hardware responsibility. */
nx_result_t nx_image_port_stop(const nx_image_port_t* p) {
    if (!valid(p))
        return NX_ERROR_INVALID;
    return p->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                : p->ops->stop(p->context);
}
/** \brief Validate known target spans before provider-specific IRQ admission.
 */
nx_result_t nx_image_port_attach_wake(const nx_image_port_t* p,
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
