/**
 * \file            uart.c
 *
 * \brief           Provider-independent caller-owned TX preparation.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/uart.h"

/** \brief Optional explicit hint target, never an implicit owner worker. */
nx_result_t nx_uart_port_attach_wake(const nx_uart_port_t* port,
                                     const nx_irq_wake_t* wake,
                                     uint8_t syscall_ceiling) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->attach_wake != NULL
               ? port->ops->attach_wake(port->context, wake, syscall_ceiling)
               : NX_ERROR_UNSUPPORTED;
}

/** \brief Prepare only unborrowed metadata and retain no payload reference. */
nx_result_t nx_uart_tx_prepare(nx_uart_tx_request_t* request,
                               const uint8_t* data, size_t length,
                               nx_time_us_t deadline) {
    if (request == NULL || data == NULL || length == 0) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    request->data = data;
    request->length = length;
    return NX_SUCCESS;
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_uart_port_submit(const nx_uart_port_t* port,
                                nx_uart_tx_request_t* request) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->submit == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->submit(port->context, request);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_uart_port_start_admitted(const nx_uart_port_t* port,
                                        nx_uart_tx_request_t* request) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->start_admitted == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->start_admitted(port->context, request);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_uart_port_cancel(const nx_uart_port_t* port,
                                nx_uart_tx_request_t* request) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->cancel == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->cancel(port->context, request);
}

/** \brief Dispatch using this face's exact instance state. */
void nx_uart_port_service(const nx_uart_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return;
    }
    if (port->ops->service == NULL) {
        return;
    }
    port->ops->service(port->context);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_uart_port_read_events(const nx_uart_port_t* port,
                                     nx_uart_rx_event_t* events,
                                     size_t capacity, size_t* count) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->read_events == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->read_events(port->context, events, capacity, count);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_uart_port_read_bytes(const nx_uart_port_t* port, uint8_t* bytes,
                                    size_t capacity, size_t* count) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->read_bytes == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->read_bytes(port->context, bytes, capacity, count);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_uart_port_stop(const nx_uart_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->stop == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->stop(port->context);
}

/** \brief Optional block-mode dispatch never implies an allocation or worker.
 */
nx_result_t nx_uart_port_rx_start(const nx_uart_port_t* port,
                                  nx_stream_t* stream) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->rx_start == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->rx_start(port->context, stream);
}

/** \brief Optional block-mode dispatch never implies an allocation or worker.
 */
nx_result_t nx_uart_port_rx_stop(const nx_uart_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->rx_stop == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->rx_stop(port->context);
}
