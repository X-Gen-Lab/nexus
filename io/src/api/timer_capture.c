/**
 * \file            timer_capture.c
 * \brief           Checked optional typed dispatch to exact static state
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/timer_capture.h"
/** \brief Protect the live face/table before passing any writable span. */
static bool output_valid(const nx_timer_capture_t* port, const void* output,
                         size_t bytes) {
    uintptr_t start = (uintptr_t)output;
    uintptr_t face = (uintptr_t)port;
    uintptr_t table = (uintptr_t)port->ops;
    return output != NULL && bytes <= UINTPTR_MAX - start &&
           !(start < face + sizeof(*port) && face < start + bytes) &&
           !(start < table + sizeof(*port->ops) && table < start + bytes);
}
/** \brief Dispatch only to the validated immutable face's exact context. */
nx_result_t nx_timer_capture_start(const nx_timer_capture_t* port,
                                   nx_timer_capture_event_t* events,
                                   size_t capacity) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (capacity > SIZE_MAX / sizeof(*events) ||
        !output_valid(port, events, capacity * sizeof(*events))) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->start == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->start(port->context, events, capacity);
}
/** \brief Dispatch only to the validated immutable face's exact context. */
nx_result_t nx_timer_capture_stop(const nx_timer_capture_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->stop == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->stop(port->context);
}
/** \brief Dispatch only to the validated immutable face's exact context. */
nx_result_t nx_timer_capture_read(const nx_timer_capture_t* port,
                                  nx_timer_capture_event_t* event) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!output_valid(port, event, sizeof(*event)) ||
        (uintptr_t)event % _Alignof(nx_timer_capture_event_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->read == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->read(port->context, event);
}
/** \brief Dispatch only to the validated immutable face's exact context. */
nx_result_t nx_timer_capture_state(const nx_timer_capture_t* port,
                                   nx_timer_capture_state_t* state) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!output_valid(port, state, sizeof(*state)) ||
        (uintptr_t)state % _Alignof(nx_timer_capture_state_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->state == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->state(port->context, state);
}
