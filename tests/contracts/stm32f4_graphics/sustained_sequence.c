/**
 * \file            sustained_sequence.c
 * \brief           Explicit render, scanout and retirement of two static frames
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "sustained_sequence.h"

static bool span(const void* p, size_t bytes) {
    return p != NULL && bytes <= UINTPTR_MAX - (uintptr_t)p;
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
static void fail(nx_graphics_reference_t* s, nx_result_t result) {
    if (s->first_failure == NX_SUCCESS && result != NX_SUCCESS &&
        result != NX_ERROR_BUSY &&
        !(s->stopping && result == NX_ERROR_CANCELLED))
        s->first_failure = result;
}
nx_result_t nx_graphics_reference_initialize(
    nx_graphics_reference_t* s, const nx_image_port_t* image,
    const nx_display_port_t* display, const nx_image_destination_t buffers[2],
    unsigned frames) {
    if (s == NULL || image == NULL || display == NULL || buffers == NULL ||
        frames == 0U)
        return NX_ERROR_INVALID;
    if (s->initialized && !s->stopped)
        return NX_ERROR_STATE;
    nx_display_constraints_t limits;
    nx_image_constraints_t image_limits;
    nx_result_t result = nx_display_port_query(display, 0U, &limits);
    if (result != NX_SUCCESS)
        return result;
    result =
        nx_image_port_query(image, NX_IMAGE_FILL, NX_IMAGE_RGB565,
                            NX_IMAGE_RGB565, NX_IMAGE_RGB565, &image_limits);
    if (result != NX_SUCCESS)
        return result;
    if (limits.width == 0U || limits.height == 0U ||
        limits.width > SIZE_MAX / 2U ||
        limits.width > image_limits.maximum_width ||
        limits.height > image_limits.maximum_height ||
        (limits.formats & (1U << NX_IMAGE_RGB565)) == 0U)
        return NX_ERROR_UNSUPPORTED;
    size_t row_bytes = limits.width * 2U;
    for (unsigned i = 0U; i < 2U; ++i) {
        if (buffers[i].format != NX_IMAGE_RGB565 ||
            !span(buffers[i].data, buffers[i].capacity) ||
            buffers[i].stride_bytes < row_bytes ||
            limits.height - 1U >
                (SIZE_MAX - row_bytes) / buffers[i].stride_bytes ||
            buffers[i].capacity <
                (limits.height - 1U) * buffers[i].stride_bytes + row_bytes ||
            overlap(buffers[i].data, buffers[i].capacity, s, sizeof(*s)))
            return NX_ERROR_INVALID;
    }
    if (overlap(buffers[0].data, buffers[0].capacity, buffers[1].data,
                buffers[1].capacity))
        return NX_ERROR_INVALID;
    nx_image_destination_t selected[2] = {buffers[0], buffers[1]};
    /* No request has been accepted. Reinitialization is allowed only after
     * both earlier providers returned their original descriptors and bytes. */
    *s = (nx_graphics_reference_t){.image = image,
                                   .display = display,
                                   .width = limits.width,
                                   .height = limits.height,
                                   .target = frames,
                                   .initialized = true};
    s->buffers[0] = selected[0];
    s->buffers[1] = selected[1];
    nx_request_initialize(&s->render.base);
    nx_request_initialize(&s->scanout[0].base);
    nx_request_initialize(&s->scanout[1].base);
    return NX_SUCCESS;
}
/** \brief Only the final request predicate returns a provider's buffer loan. */
static void observe(nx_graphics_reference_t* s) {
    if (s->render_loan &&
        nx_request_state(&s->render.base) == NX_REQUEST_SETTLED) {
        nx_result_t result;
        size_t bytes;
        nx_result_t observed =
            nx_request_result(&s->render.base, &result, &bytes);
        if (observed == NX_SUCCESS) {
            s->render_loan = false;
            if (result == NX_SUCCESS && bytes == s->width * s->height * 2U) {
                s->ready[s->rendering] = true;
                ++s->rendered;
            } else {
                fail(s, result == NX_SUCCESS ? NX_ERROR_IO : result);
            }
        } else {
            fail(s, observed);
        }
    }
    for (unsigned i = 0U; i < 2U; ++i)
        if (s->display_loan[i] &&
            nx_request_state(&s->scanout[i].base) == NX_REQUEST_SETTLED) {
            nx_result_t result;
            size_t ignored;
            nx_result_t observed =
                nx_request_result(&s->scanout[i].base, &result, &ignored);
            if (observed == NX_SUCCESS) {
                s->display_loan[i] = false;
                ++s->retired;
                fail(s, result);
            } else {
                fail(s, observed);
            }
        }
}
/** \brief Diagnostics explain a fault; only request SETTLED grants reuse. */
static nx_result_t service_display(nx_graphics_reference_t* s) {
    nx_result_t result = nx_display_port_service(s->display);
    if (result != NX_SUCCESS && result != NX_ERROR_BUSY) {
        nx_display_snapshot_t snapshot;
        if (nx_display_port_snapshot(s->display, 0U, &snapshot) == NX_SUCCESS &&
            snapshot.first_failure != NX_SUCCESS)
            return snapshot.first_failure;
    }
    return result;
}
static nx_result_t present(nx_graphics_reference_t* s, unsigned i) {
    const nx_image_destination_t* b = &s->buffers[i];
    const nx_display_operation_t frame = {
        .frame = {(const uint8_t*)b->data, b->stride_bytes, s->width * 2U,
                  s->height, b->capacity},
        .format = NX_IMAGE_RGB565,
        .layer = 0U};
    nx_result_t result =
        nx_display_prepare(&s->scanout[i], &frame, NX_DEADLINE_NEVER);
    if (result != NX_SUCCESS)
        return result;
    result = nx_display_port_submit(s->display, &s->scanout[i]);
    if (result == NX_SUCCESS) {
        s->ready[i] = false;
        s->display_loan[i] = true;
        ++s->presented;
        result = service_display(s);
    }
    return result;
}
nx_result_t nx_graphics_reference_step(nx_graphics_reference_t* s,
                                       nx_time_us_t deadline) {
    if (s == NULL || !s->initialized || s->stopping || s->stopped)
        return NX_ERROR_STATE;
    if (s->render_loan)
        fail(s, nx_image_port_service(s->image));
    if (s->display_loan[0] || s->display_loan[1])
        fail(s, service_display(s));
    observe(s);
    if (s->first_failure != NX_SUCCESS)
        return s->first_failure;
    for (unsigned i = 0U; i < 2U; ++i)
        if (s->ready[i] && !s->display_loan[i]) {
            fail(s, present(s, i));
            if (s->first_failure != NX_SUCCESS)
                return s->first_failure;
        }
    if (!s->render_loan && s->rendered < s->target)
        for (unsigned i = 0U; i < 2U; ++i)
            if (!s->display_loan[i] && !s->ready[i]) {
                nx_image_operation_t fill = {.kind = NX_IMAGE_FILL,
                                             .destination = s->buffers[i],
                                             .width = s->width,
                                             .height = s->height,
                                             .fill_value = 0x6d5bU};
                nx_result_t result =
                    nx_image_prepare(&s->render, &fill, deadline);
                if (result == NX_SUCCESS)
                    result = nx_image_port_submit(s->image, &s->render);
                if (result == NX_SUCCESS) {
                    s->rendering = (uint8_t)i;
                    s->render_loan = true;
                } else {
                    fail(s, result);
                }
                break;
            }
    if (s->first_failure != NX_SUCCESS)
        return s->first_failure;
    /* A completed handover prefix still leaves its final scanout borrowed.
     * The caller explicitly stops both controllers before Platform shutdown. */
    return s->presented == s->target && s->retired + 1U >= s->target
               ? NX_SUCCESS
               : NX_ERROR_BUSY;
}
nx_result_t nx_graphics_reference_stop(nx_graphics_reference_t* s) {
    if (s == NULL || !s->initialized)
        return NX_ERROR_STATE;
    if (s->stopped)
        return NX_SUCCESS;
    s->stopping = true;
    nx_result_t image = nx_image_port_stop(s->image);
    nx_result_t display = nx_display_port_stop(s->display);
    fail(s, image);
    fail(s, display);
    observe(s);
    if (image != NX_SUCCESS || display != NX_SUCCESS || s->render_loan ||
        s->display_loan[0] || s->display_loan[1])
        return s->first_failure != NX_SUCCESS ? s->first_failure
                                              : NX_ERROR_BUSY;
    s->ready[0] = s->ready[1] = false;
    s->stopped = true;
    return NX_SUCCESS;
}
