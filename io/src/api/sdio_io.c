/**
 * \file            sdio_io.c
 * \brief           Checked dispatch for typed SDIO function operations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/sdio_io.h"

static bool valid(const nx_sdio_io_port_t* port) {
    return port != NULL && port->ops != NULL && port->context != NULL;
}

nx_result_t nx_sdio_io_activate(const nx_sdio_io_port_t* port,
                                nx_time_us_t deadline) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->activate != NULL
               ? port->ops->activate(port->context, deadline)
               : NX_ERROR_UNSUPPORTED;
}

nx_result_t nx_sdio_io_stop(const nx_sdio_io_port_t* port,
                            nx_time_us_t deadline) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->stop != NULL ? port->ops->stop(port->context, deadline)
                                   : NX_ERROR_UNSUPPORTED;
}

nx_result_t nx_sdio_io_limits(const nx_sdio_io_port_t* port,
                              nx_sdio_io_limits_t* limits) {
    if (!valid(port) || limits == NULL)
        return NX_ERROR_INVALID;
    return port->ops->limits != NULL ? port->ops->limits(port->context, limits)
                                     : NX_ERROR_UNSUPPORTED;
}

nx_result_t nx_sdio_io_inspect(const nx_sdio_io_port_t* port,
                               nx_sdio_io_info_t* info, nx_time_us_t deadline) {
    if (!valid(port) || info == NULL)
        return NX_ERROR_INVALID;
    return port->ops->inspect != NULL
               ? port->ops->inspect(port->context, info, deadline)
               : NX_ERROR_UNSUPPORTED;
}

nx_result_t nx_sdio_io_enable(const nx_sdio_io_port_t* port, uint8_t function,
                              bool enabled, nx_time_us_t deadline) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->enable != NULL
               ? port->ops->enable(port->context, function, enabled, deadline)
               : NX_ERROR_UNSUPPORTED;
}

nx_result_t nx_sdio_io_block_size(const nx_sdio_io_port_t* port,
                                  uint8_t function, uint16_t bytes,
                                  nx_time_us_t deadline) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->block_size != NULL
               ? port->ops->block_size(port->context, function, bytes, deadline)
               : NX_ERROR_UNSUPPORTED;
}

nx_result_t nx_sdio_io_read_byte(const nx_sdio_io_port_t* port,
                                 uint8_t function, uint32_t address,
                                 uint8_t* value, nx_time_us_t deadline) {
    if (!valid(port) || value == NULL)
        return NX_ERROR_INVALID;
    return port->ops->read_byte != NULL
               ? port->ops->read_byte(port->context, function, address, value,
                                      deadline)
               : NX_ERROR_UNSUPPORTED;
}

nx_result_t nx_sdio_io_write_byte(const nx_sdio_io_port_t* port,
                                  uint8_t function, uint32_t address,
                                  uint8_t value, nx_time_us_t deadline) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->write_byte != NULL
               ? port->ops->write_byte(port->context, function, address, value,
                                       deadline)
               : NX_ERROR_UNSUPPORTED;
}

nx_result_t nx_sdio_io_interrupts(const nx_sdio_io_port_t* port,
                                  uint8_t functions, nx_time_us_t deadline) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->interrupts != NULL
               ? port->ops->interrupts(port->context, functions, deadline)
               : NX_ERROR_UNSUPPORTED;
}

nx_result_t nx_sdio_io_pending(const nx_sdio_io_port_t* port,
                               uint8_t* functions, nx_time_us_t deadline) {
    if (!valid(port) || functions == NULL)
        return NX_ERROR_INVALID;
    return port->ops->pending != NULL
               ? port->ops->pending(port->context, functions, deadline)
               : NX_ERROR_UNSUPPORTED;
}

nx_result_t nx_sdio_io_submit(const nx_sdio_io_port_t* port,
                              nx_sdio_io_request_t* request) {
    if (!valid(port) || request == NULL)
        return NX_ERROR_INVALID;
    return port->ops->submit != NULL ? port->ops->submit(port->context, request)
                                     : NX_ERROR_UNSUPPORTED;
}

nx_result_t nx_sdio_io_cancel(const nx_sdio_io_port_t* port,
                              nx_sdio_io_request_t* request) {
    if (!valid(port) || request == NULL)
        return NX_ERROR_INVALID;
    return port->ops->cancel != NULL ? port->ops->cancel(port->context, request)
                                     : NX_ERROR_UNSUPPORTED;
}

void nx_sdio_io_service(const nx_sdio_io_port_t* port) {
    if (valid(port) && port->ops->service != NULL)
        port->ops->service(port->context);
}

nx_result_t nx_sdio_io_sync(const nx_sdio_io_port_t* port,
                            nx_time_us_t deadline) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->sync != NULL ? port->ops->sync(port->context, deadline)
                                   : NX_ERROR_UNSUPPORTED;
}
