/**
 * \file            usb_device.c
 * \brief           Immutable USB packet interface and unborrowed preparation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/usb_device.h"

/** \brief Preparation never touches packet memory or changes a borrowed loan.
 */
static nx_result_t prepare(nx_usb_request_t* request, const uint8_t* in,
                           uint8_t* out, size_t length, nx_usb_stage_t stage,
                           uint64_t epoch, nx_time_us_t deadline) {
    if (request == NULL || (length != 0U && in == NULL && out == NULL) ||
        (stage != NX_USB_DATA && stage != NX_USB_STATUS) ||
        (stage == NX_USB_STATUS && length != 0U)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    request->in = in;
    request->out = out;
    request->length = length;
    request->control_epoch = epoch;
    request->stage = stage;
    return NX_SUCCESS;
}
nx_result_t nx_usb_in_prepare(nx_usb_request_t* request, const uint8_t* bytes,
                              size_t length, nx_usb_stage_t stage,
                              uint64_t epoch, nx_time_us_t deadline) {
    return prepare(request, bytes, NULL, length, stage, epoch, deadline);
}
nx_result_t nx_usb_out_prepare(nx_usb_request_t* request, uint8_t* bytes,
                               size_t capacity, nx_usb_stage_t stage,
                               uint64_t epoch, nx_time_us_t deadline) {
    return prepare(request, NULL, bytes, capacity, stage, epoch, deadline);
}

/** \brief Dispatch the exact immutable face, without discovery or allocation.
 */
nx_result_t nx_usb_endpoint_submit(const nx_usb_endpoint_port_t* port,
                                   nx_usb_request_t* request) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->submit == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->submit(port->context, request);
}

/** \brief Dispatch the exact immutable face, without discovery or allocation.
 */
nx_result_t nx_usb_endpoint_cancel(const nx_usb_endpoint_port_t* port,
                                   nx_usb_request_t* request) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->cancel == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->cancel(port->context, request);
}

/** \brief Dispatch the exact immutable face, without discovery or allocation.
 */
nx_result_t nx_usb_endpoint_stall(const nx_usb_endpoint_port_t* port,
                                  bool stalled) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stall == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->stall(port->context, stalled);
}

/** \brief Dispatch the exact immutable face, without discovery or allocation.
 */
nx_result_t nx_usb_device_service(const nx_usb_device_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->service(port->context);
}

/** \brief Dispatch the exact immutable face, without discovery or allocation.
 */
nx_result_t nx_usb_device_read_events(const nx_usb_device_port_t* port,
                                      nx_usb_event_t* events, size_t capacity,
                                      size_t* count) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->read_events == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->read_events(port->context, events, capacity, count);
}

/** \brief Dispatch the exact immutable face, without discovery or allocation.
 */
nx_result_t nx_usb_device_state(const nx_usb_device_port_t* port,
                                nx_usb_device_state_t* state) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->state == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->state(port->context, state);
}

/** \brief Dispatch the exact immutable face, without discovery or allocation.
 */
nx_result_t nx_usb_device_address(const nx_usb_device_port_t* port,
                                  uint64_t epoch, uint8_t address) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->address == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->address(port->context, epoch, address);
}

/** \brief Dispatch the exact immutable face, without discovery or allocation.
 */
nx_result_t nx_usb_device_attach_wake(const nx_usb_device_port_t* port,
                                      const nx_irq_wake_t* wake,
                                      uint8_t syscall_ceiling) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->attach_wake == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->attach_wake(port->context, wake, syscall_ceiling);
}

/** \brief Dispatch the exact immutable face, without discovery or allocation.
 */
nx_result_t nx_usb_device_stop(const nx_usb_device_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}
