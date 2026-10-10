/**
 * \file            spi.c
 *
 * \brief           Checked typed SPI polling and finite-request dispatch
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/spi.h"

/** \brief Optional DMA hint binding is validated by all actual publisher IRQs.
 */
nx_result_t nx_spi_port_attach_wake(const nx_spi_port_t* port,
                                    const nx_irq_wake_t* wake,
                                    uint8_t syscall_ceiling) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->attach_wake != NULL
               ? port->ops->attach_wake(port->context, wake, syscall_ceiling)
               : NX_ERROR_UNSUPPORTED;
}

/** \brief An adapter must never borrow work for a different execution bus. */
bool nx_spi_endpoint_on_port(const nx_spi_endpoint_t* endpoint,
                             const nx_spi_port_t* port) {
    return endpoint != NULL && endpoint->ops != NULL &&
           endpoint->context != NULL && endpoint->ops->on_port != NULL &&
           port != NULL && port->ops != NULL && port->context != NULL &&
           endpoint->ops->on_port(endpoint->context, port);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_spi_port_recover(const nx_spi_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->recover == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->recover(port->context);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_spi_endpoint_transfer(const nx_spi_endpoint_t* port,
                                     const uint8_t* tx, uint8_t* rx,
                                     size_t length, nx_time_us_t deadline,
                                     size_t* transferred) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->transfer == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->transfer(port->context, tx, rx, length, deadline,
                               transferred);
}

/** \brief Request preparation never admits or changes hardware. */
nx_result_t nx_spi_request_prepare(nx_spi_request_t* request, const uint8_t* tx,
                                   uint8_t* rx, size_t length,
                                   nx_time_us_t deadline) {
    if (request == NULL || length == 0U || (tx == NULL && rx == NULL)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->tx = tx;
        request->rx = rx;
        request->length = length;
    }
    return result;
}

/** \brief Optional finite-mode method dispatch preserves the exact bus context.
 */
nx_result_t nx_spi_endpoint_submit(const nx_spi_endpoint_t* port,
                                   nx_spi_request_t* request) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->submit == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->submit(port->context, request);
}

/** \brief Optional finite-mode method dispatch preserves the exact bus context.
 */
nx_result_t nx_spi_endpoint_start_admitted(const nx_spi_endpoint_t* port,
                                           nx_spi_request_t* request) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->start_admitted == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->start_admitted(port->context, request);
}

/** \brief Optional finite-mode method dispatch preserves the exact bus context.
 */
nx_result_t nx_spi_port_cancel(const nx_spi_port_t* port,
                               nx_spi_request_t* request) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->cancel == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->cancel(port->context, request);
}

/** \brief Optional finite-mode method dispatch preserves the exact bus context.
 */
nx_result_t nx_spi_port_stop(const nx_spi_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->stop == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->stop(port->context);
}

/** \brief Optional finite-mode method dispatch preserves the exact bus context.
 */
void nx_spi_port_service(const nx_spi_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return;
    }
    if (port->ops->service == NULL) {
        return;
    }
    port->ops->service(port->context);
}
