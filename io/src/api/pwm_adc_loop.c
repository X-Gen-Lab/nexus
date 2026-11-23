/**
 * \file            pwm_adc_loop.c
 * \brief           Checked static IRQ control-session preparation and dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/pwm_adc_loop.h"

/** \brief Check typed storage and representable exclusive ends first. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && bytes != 0U && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}

/** \brief Inputs have already passed representable-end validation. */
static bool overlap(const void* first, size_t first_bytes, const void* second,
                    size_t second_bytes) {
    return (uintptr_t)first < (uintptr_t)second + second_bytes &&
           (uintptr_t)second < (uintptr_t)first + first_bytes;
}

/** \brief A face is existing identity, never a constructor or acquisition. */
static bool valid(const nx_pwm_adc_loop_t* port) {
    return span(port, sizeof(*port), _Alignof(nx_pwm_adc_loop_t)) &&
           span(port->ops, sizeof(*port->ops),
                _Alignof(nx_pwm_adc_loop_ops_t)) &&
           port->context != NULL;
}

nx_result_t nx_pwm_adc_loop_prepare(nx_pwm_adc_loop_session_t* session,
                                    uint16_t* samples, size_t sample_count,
                                    uint32_t* duty_ticks, size_t duty_count,
                                    nx_pwm_adc_loop_compute_t compute,
                                    void* context, nx_time_us_t deadline) {
    if (!span(session, sizeof(*session), _Alignof(nx_pwm_adc_loop_session_t))) {
        return NX_ERROR_INVALID;
    }
    nx_request_state_t state = nx_request_state(&session->base);
    if (state != NX_REQUEST_READY && state != NX_REQUEST_SETTLED) {
        return NX_ERROR_STATE;
    }
    if (deadline == NX_DEADLINE_NEVER || sample_count == 0U ||
        sample_count > SIZE_MAX / sizeof(*samples) || duty_count == 0U ||
        duty_count > SIZE_MAX / sizeof(*duty_ticks) || compute == NULL ||
        !span(context, 1U, 1U) ||
        !span(samples, sample_count * sizeof(*samples), _Alignof(uint16_t)) ||
        !span(duty_ticks, duty_count * sizeof(*duty_ticks),
              _Alignof(uint32_t))) {
        return NX_ERROR_INVALID;
    }
    if (overlap(session, sizeof(*session), samples,
                sample_count * sizeof(*samples)) ||
        overlap(session, sizeof(*session), duty_ticks,
                duty_count * sizeof(*duty_ticks)) ||
        overlap(samples, sample_count * sizeof(*samples), duty_ticks,
                duty_count * sizeof(*duty_ticks)) ||
        overlap(context, 1U, session, sizeof(*session)) ||
        overlap(context, 1U, samples, sample_count * sizeof(*samples)) ||
        overlap(context, 1U, duty_ticks, duty_count * sizeof(*duty_ticks))) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_request_prepare(&session->base, deadline);
    if (result == NX_SUCCESS) {
        session->samples = samples;
        session->sample_count = sample_count;
        session->duty_ticks = duty_ticks;
        session->duty_count = duty_count;
        session->compute = compute;
        session->context = context;
    }
    return result;
}

nx_result_t nx_pwm_adc_loop_start(const nx_pwm_adc_loop_t* port,
                                  nx_pwm_adc_loop_session_t* session) {
    if (!valid(port) ||
        !span(session, sizeof(*session), _Alignof(nx_pwm_adc_loop_session_t)) ||
        overlap(session, sizeof(*session), port, sizeof(*port)) ||
        overlap(session, sizeof(*session), port->ops, sizeof(*port->ops))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->start == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->start(port->context, session);
}

nx_result_t nx_pwm_adc_loop_service(const nx_pwm_adc_loop_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->service(port->context);
}

nx_result_t nx_pwm_adc_loop_stop(const nx_pwm_adc_loop_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}

nx_result_t nx_pwm_adc_loop_state(const nx_pwm_adc_loop_t* port,
                                  nx_pwm_adc_loop_state_t* state) {
    if (!valid(port) ||
        !span(state, sizeof(*state), _Alignof(nx_pwm_adc_loop_state_t)) ||
        overlap(state, sizeof(*state), port, sizeof(*port)) ||
        overlap(state, sizeof(*state), port->ops, sizeof(*port->ops))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->state == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->state(port->context, state);
}
