/**
 * \file            flash.c
 * \brief           Checked typed dispatch through shared read-only provider
 * methods \author          Nexus Team \version         1.0.0 \date 2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/flash.h"

/** \brief Dispatch using this face's exact instance state. */
const nx_flash_geometry_t* nx_flash_port_geometry(const nx_flash_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NULL;
    }
    if (port->ops->geometry == NULL) {
        return NULL;
    }
    return port->ops->geometry(port->context);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_flash_port_read(const nx_flash_port_t* port, uint32_t offset,
                               void* data, size_t length) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->read == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->read(port->context, offset, data, length);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_flash_port_program(const nx_flash_port_t* port, uint32_t offset,
                                  const void* data, size_t length,
                                  nx_time_us_t deadline) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->program == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->program(port->context, offset, data, length, deadline);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_flash_port_erase(const nx_flash_port_t* port, uint32_t offset,
                                size_t length, nx_time_us_t deadline) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->erase == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->erase(port->context, offset, length, deadline);
}
