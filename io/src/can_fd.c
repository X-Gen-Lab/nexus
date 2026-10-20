/**
 * \file            can_fd.c
 * \brief           Optional CAN FD value preparation without runtime storage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "can_fd_impl.h"

/** \brief Actual payload lengths never round into a larger wire frame. */
bool nx_can_fd_frame_valid(const nx_can_fd_frame_t* frame) {
    if (frame == NULL ||
        (frame->kind != NX_CAN_DATA && frame->kind != NX_CAN_REMOTE) ||
        !((frame->format == NX_CAN_STANDARD && frame->id <= 0x7FFU) ||
          (frame->format == NX_CAN_EXTENDED && frame->id <= 0x1FFFFFFFU))) {
        return false;
    }
    if (frame->wire == NX_CAN_WIRE_CLASSIC) {
        return frame->payload_bytes <= 8U && !frame->brs;
    }
    if (frame->wire != NX_CAN_WIRE_FD || frame->kind != NX_CAN_DATA) {
        return false;
    }
    uint8_t bytes = frame->payload_bytes;
    return bytes <= 8U || bytes == 12U || bytes == 16U || bytes == 20U ||
           bytes == 24U || bytes == 32U || bytes == 48U || bytes == 64U;
}

/** \brief Copy only after whole-value validation and unborrowed preparation. */
nx_result_t nx_can_fd_tx_prepare(nx_can_fd_tx_request_t* request,
                                 const nx_can_fd_frame_t* frame,
                                 nx_time_us_t deadline) {
    if (request == NULL || !nx_can_fd_frame_valid(frame)) {
        return NX_ERROR_INVALID;
    }
    nx_can_fd_frame_t copy = *frame;
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->frame = copy;
    }
    return result;
}

/** \brief A typed lookup never substitutes an absent context or method. */
static bool face_valid(const nx_can_fd_port_t* port) {
    return port != NULL && port->ops != NULL && port->context != NULL;
}
/** \brief Immutable faces and tables cannot become request/output storage. */
static bool overlaps(const void* first, size_t first_size, const void* second,
                     size_t second_size) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return first_size > UINTPTR_MAX - a || second_size > UINTPTR_MAX - b ||
           (a < b + second_size && b < a + first_size);
}
/** \brief Dispatch preserves the exact face and readonly table identities. */
static bool face_disjoint(const nx_can_fd_port_t* port, const void* storage,
                          size_t bytes) {
    return storage != NULL && !overlaps(port, sizeof(*port), storage, bytes) &&
           !overlaps(port->ops, sizeof(*port->ops), storage, bytes);
}
/** \brief Dispatch once into the selected controller's admission method. */
nx_result_t nx_can_fd_port_submit(const nx_can_fd_port_t* port,
                                  nx_can_fd_tx_request_t* request) {
    if (!face_valid(port) || !face_disjoint(port, request, sizeof(*request))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->submit != NULL ? port->ops->submit(port->context, request)
                                     : NX_ERROR_UNSUPPORTED;
}
/** \brief A cancel request never revokes the existing provider borrow. */
nx_result_t nx_can_fd_port_cancel(const nx_can_fd_port_t* port,
                                  nx_can_fd_tx_request_t* request) {
    if (!face_valid(port) || !face_disjoint(port, request, sizeof(*request))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->cancel != NULL ? port->ops->cancel(port->context, request)
                                     : NX_ERROR_UNSUPPORTED;
}
/** \brief Service only the exact immutable typed face, with no hidden wait. */
nx_result_t nx_can_fd_port_service(const nx_can_fd_port_t* port) {
    if (!face_valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->service != NULL ? port->ops->service(port->context)
                                      : NX_ERROR_UNSUPPORTED;
}
/** \brief Output values are copied by the provider and never retained. */
nx_result_t nx_can_fd_port_read_events(const nx_can_fd_port_t* port,
                                       nx_can_fd_event_t* events,
                                       size_t capacity, size_t* count) {
    if (!face_valid(port) || capacity == 0U ||
        capacity > SIZE_MAX / sizeof(*events) ||
        !face_disjoint(port, events, capacity * sizeof(*events)) ||
        !face_disjoint(port, count, sizeof(*count)) ||
        overlaps(events, capacity * sizeof(*events), count, sizeof(*count))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->read_events != NULL
               ? port->ops->read_events(port->context, events, capacity, count)
               : NX_ERROR_UNSUPPORTED;
}
/** \brief Latched state remains separate from receive queue capacity. */
nx_result_t nx_can_fd_port_state(const nx_can_fd_port_t* port,
                                 nx_can_fd_state_t* state) {
    if (!face_valid(port) || !face_disjoint(port, state, sizeof(*state))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->state != NULL ? port->ops->state(port->context, state)
                                    : NX_ERROR_UNSUPPORTED;
}
/** \brief Stop success must come from the actual selected hardware owner. */
nx_result_t nx_can_fd_port_stop(const nx_can_fd_port_t* port) {
    if (!face_valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop != NULL ? port->ops->stop(port->context)
                                   : NX_ERROR_UNSUPPORTED;
}
/** \brief Recovery does not replay a request or change controller identity. */
nx_result_t nx_can_fd_port_recover(const nx_can_fd_port_t* port) {
    if (!face_valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->recover != NULL ? port->ops->recover(port->context)
                                      : NX_ERROR_UNSUPPORTED;
}
/** \brief The provider validates actual IRQ facts before borrowing a sink. */
nx_result_t nx_can_fd_port_attach_wake(const nx_can_fd_port_t* port,
                                       const nx_irq_wake_t* wake,
                                       uint8_t syscall_ceiling) {
    if (!face_valid(port) ||
        (wake != NULL && !face_disjoint(port, wake, sizeof(*wake)))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->attach_wake != NULL
               ? port->ops->attach_wake(port->context, wake, syscall_ceiling)
               : NX_ERROR_UNSUPPORTED;
}
