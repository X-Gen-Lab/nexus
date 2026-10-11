/**
 * \file            can.c
 * \brief           Classic CAN immutable-face dispatch and value preparation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "can_controller.h"

/** \brief Validate the complete classic frame without retaining its address. */
bool nx_can_frame_valid(const nx_can_frame_t* frame) {
    return frame != NULL && frame->dlc <= 8U &&
           (frame->kind == NX_CAN_DATA || frame->kind == NX_CAN_REMOTE) &&
           ((frame->format == NX_CAN_STANDARD && frame->id <= 0x7FFU) ||
            (frame->format == NX_CAN_EXTENDED && frame->id <= 0x1FFFFFFFU));
}

/** \brief Copy only after validating and preparing unborrowed storage. */
nx_result_t nx_can_tx_prepare(nx_can_tx_request_t* request,
                              const nx_can_frame_t* frame,
                              nx_time_us_t deadline) {
    if (request == NULL || !nx_can_frame_valid(frame)) {
        return NX_ERROR_INVALID;
    }
    nx_can_frame_t copy = *frame;
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->frame = copy;
        if (copy.kind == NX_CAN_REMOTE) {
            for (unsigned i = 0; i < 8U; ++i) {
                request->frame.data[i] = 0U;
            }
        }
    }
    return result;
}

/** \brief Dispatch the exact immutable face and optional method. */
nx_result_t nx_can_port_submit(const nx_can_port_t* port,
                               nx_can_tx_request_t* request) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->submit == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->submit(port->context, request);
}

/** \brief Dispatch the exact immutable face and optional method. */
nx_result_t nx_can_port_cancel(const nx_can_port_t* port,
                               nx_can_tx_request_t* request) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->cancel == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->cancel(port->context, request);
}

/** \brief Dispatch the exact immutable face and optional method. */
nx_result_t nx_can_port_service(const nx_can_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->service(port->context);
}

/** \brief Dispatch the exact immutable face and optional method. */
nx_result_t nx_can_port_read_events(const nx_can_port_t* port,
                                    nx_can_event_t* events, size_t capacity,
                                    size_t* count) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->read_events == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->read_events(port->context, events, capacity, count);
}

/** \brief Dispatch the exact immutable face and optional method. */
nx_result_t nx_can_port_state(const nx_can_port_t* port,
                              nx_can_state_t* state) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->state == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->state(port->context, state);
}

/** \brief Dispatch the exact immutable face and optional method. */
nx_result_t nx_can_port_stop(const nx_can_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}

/** \brief Dispatch the exact immutable face and optional method. */
nx_result_t nx_can_port_recover(const nx_can_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->recover == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->recover(port->context);
}

/** \brief Dispatch the exact immutable face and optional method. */
nx_result_t nx_can_port_attach_wake(const nx_can_port_t* port,
                                    const nx_irq_wake_t* wake,
                                    uint8_t syscall_ceiling) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->attach_wake == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->attach_wake(port->context, wake, syscall_ceiling);
}
