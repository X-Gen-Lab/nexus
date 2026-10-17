/**
 * \file            adc_group.c
 * \brief           Checked finite whole-frame ADC request dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/adc_group.h"

/** \brief Validate representable exclusive ends before inspecting metadata. */
static bool span(const void* p, size_t bytes, size_t alignment) {
    return p != NULL && bytes != 0U && (uintptr_t)p % alignment == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)p;
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
static bool valid(const nx_adc_group_port_t* p) {
    return span(p, sizeof(*p), _Alignof(nx_adc_group_port_t)) &&
           span(p->ops, sizeof(*p->ops), _Alignof(nx_adc_group_ops_t)) &&
           p->context != NULL;
}
static bool control(const nx_adc_group_port_t* p, const void* data,
                    size_t bytes) {
    return overlap(data, bytes, p, sizeof(*p)) ||
           overlap(data, bytes, p->ops, sizeof(*p->ops));
}
nx_result_t nx_adc_group_prepare(nx_adc_group_request_t* request, void* frames,
                                 size_t bytes, size_t count,
                                 nx_time_us_t deadline) {
    if (!span(request, sizeof(*request), _Alignof(nx_adc_group_request_t))) {
        return NX_ERROR_INVALID;
    }
    nx_request_state_t state = nx_request_state(&request->base);
    if (state != NX_REQUEST_READY && state != NX_REQUEST_SETTLED) {
        return NX_ERROR_STATE;
    }
    if (count == 0U || deadline == NX_DEADLINE_NEVER ||
        !span(frames, bytes, 1U) ||
        overlap(frames, bytes, request, sizeof(*request))) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->frames = frames;
        request->bytes = bytes;
        request->frame_count = count;
    }
    return result;
}
nx_result_t nx_adc_group_submit(const nx_adc_group_port_t* p,
                                nx_adc_group_request_t* request) {
    if (!valid(p) ||
        !span(request, sizeof(*request), _Alignof(nx_adc_group_request_t)) ||
        control(p, request, sizeof(*request))) {
        return NX_ERROR_INVALID;
    }
    return p->ops->submit == NULL ? NX_ERROR_UNSUPPORTED
                                  : p->ops->submit(p->context, request);
}
nx_result_t nx_adc_group_cancel(const nx_adc_group_port_t* p,
                                nx_adc_group_request_t* request) {
    if (!valid(p) ||
        !span(request, sizeof(*request), _Alignof(nx_adc_group_request_t)) ||
        control(p, request, sizeof(*request))) {
        return NX_ERROR_INVALID;
    }
    return p->ops->cancel == NULL ? NX_ERROR_UNSUPPORTED
                                  : p->ops->cancel(p->context, request);
}
nx_result_t nx_adc_group_service(const nx_adc_group_port_t* p) {
    if (!valid(p)) {
        return NX_ERROR_INVALID;
    }
    return p->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                   : p->ops->service(p->context);
}
nx_result_t nx_adc_group_stop(const nx_adc_group_port_t* p) {
    if (!valid(p)) {
        return NX_ERROR_INVALID;
    }
    return p->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                : p->ops->stop(p->context);
}
nx_result_t nx_adc_group_info(const nx_adc_group_port_t* p,
                              nx_adc_group_info_t* info) {
    if (!valid(p) ||
        !span(info, sizeof(*info), _Alignof(nx_adc_group_info_t)) ||
        control(p, info, sizeof(*info))) {
        return NX_ERROR_INVALID;
    }
    return p->ops->info == NULL ? NX_ERROR_UNSUPPORTED
                                : p->ops->info(p->context, info);
}
