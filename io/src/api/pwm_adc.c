/**
 * \file            pwm_adc.c
 * \brief           Checked finite joint PWM / ADC request dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/pwm_adc.h"

/** \brief Validate exclusive ends before descriptor access or alias checks. */
static bool span(const void* p, size_t bytes, size_t alignment) {
    return p != NULL && bytes != 0U && (uintptr_t)p % alignment == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)p;
}
/** \brief Both checked spans have representable exclusive ends. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief Lookup is immutable identity, never ownership acquisition. */
static bool valid(const nx_pwm_adc_t* p) {
    return span(p, sizeof(*p), _Alignof(nx_pwm_adc_t)) &&
           span(p->ops, sizeof(*p->ops), _Alignof(nx_pwm_adc_ops_t)) &&
           p->context != NULL;
}
nx_result_t nx_pwm_adc_prepare(nx_pwm_adc_request_t* request, uint16_t* samples,
                               size_t count, const nx_pwm_group_frame_t* frame,
                               nx_time_us_t deadline) {
    if (!span(request, sizeof(*request), _Alignof(nx_pwm_adc_request_t))) {
        return NX_ERROR_INVALID;
    }
    nx_request_state_t state = nx_request_state(&request->base);
    if (state != NX_REQUEST_READY && state != NX_REQUEST_SETTLED) {
        return NX_ERROR_STATE;
    }
    if (deadline == NX_DEADLINE_NEVER || count == 0U ||
        count > SIZE_MAX / sizeof(*samples) ||
        !span(samples, count * sizeof(*samples), _Alignof(uint16_t)) ||
        !span(frame, sizeof(*frame), _Alignof(nx_pwm_group_frame_t)) ||
        overlap(request, sizeof(*request), frame, sizeof(*frame))) {
        return NX_ERROR_INVALID;
    }
    nx_pwm_group_frame_t copied = *frame;
    if (copied.channel_count == 0U ||
        copied.channel_count > SIZE_MAX / sizeof(*copied.duty_ticks) ||
        copied.trigger_ticks == 0U ||
        !span(copied.duty_ticks,
              copied.channel_count * sizeof(*copied.duty_ticks),
              _Alignof(uint32_t))) {
        return NX_ERROR_INVALID;
    }
    const void* pointers[4] = {request, samples, frame, copied.duty_ticks};
    size_t sizes[4] = {sizeof(*request), count * sizeof(*samples),
                       sizeof(*frame),
                       copied.channel_count * sizeof(*copied.duty_ticks)};
    for (size_t i = 0U; i != 4U; ++i) {
        for (size_t j = i + 1U; j != 4U; ++j) {
            if (overlap(pointers[i], sizes[i], pointers[j], sizes[j])) {
                return NX_ERROR_INVALID;
            }
        }
    }
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->samples = samples;
        request->sample_count = count;
        request->frame = copied;
    }
    return result;
}
nx_result_t nx_pwm_adc_submit(const nx_pwm_adc_t* p,
                              nx_pwm_adc_request_t* request) {
    if (!valid(p) ||
        !span(request, sizeof(*request), _Alignof(nx_pwm_adc_request_t)) ||
        overlap(request, sizeof(*request), p, sizeof(*p)) ||
        overlap(request, sizeof(*request), p->ops, sizeof(*p->ops))) {
        return NX_ERROR_INVALID;
    }
    return p->ops->submit == NULL ? NX_ERROR_UNSUPPORTED
                                  : p->ops->submit(p->context, request);
}
nx_result_t nx_pwm_adc_cancel(const nx_pwm_adc_t* p,
                              nx_pwm_adc_request_t* request) {
    if (!valid(p) ||
        !span(request, sizeof(*request), _Alignof(nx_pwm_adc_request_t))) {
        return NX_ERROR_INVALID;
    }
    return p->ops->cancel == NULL ? NX_ERROR_UNSUPPORTED
                                  : p->ops->cancel(p->context, request);
}
nx_result_t nx_pwm_adc_service(const nx_pwm_adc_t* p) {
    if (!valid(p)) {
        return NX_ERROR_INVALID;
    }
    return p->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                   : p->ops->service(p->context);
}
nx_result_t nx_pwm_adc_stop(const nx_pwm_adc_t* p) {
    if (!valid(p)) {
        return NX_ERROR_INVALID;
    }
    return p->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                : p->ops->stop(p->context);
}
nx_result_t nx_pwm_adc_state(const nx_pwm_adc_t* p, nx_pwm_adc_state_t* state) {
    if (!valid(p) ||
        !span(state, sizeof(*state), _Alignof(nx_pwm_adc_state_t)) ||
        overlap(state, sizeof(*state), p, sizeof(*p)) ||
        overlap(state, sizeof(*state), p->ops, sizeof(*p->ops))) {
        return NX_ERROR_INVALID;
    }
    return p->ops->state == NULL ? NX_ERROR_UNSUPPORTED
                                 : p->ops->state(p->context, state);
}
