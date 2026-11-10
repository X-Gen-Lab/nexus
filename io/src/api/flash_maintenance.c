/**
 * \file            flash_maintenance.c
 * \brief           Checked dispatch for explicitly selected Flash maintenance
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/flash_maintenance.h"

static bool valid(const nx_flash_maintenance_port_t* port) {
    return port != NULL && port->ops != NULL && port->context != NULL;
}

nx_result_t
nx_flash_maintenance_snapshot(const nx_flash_maintenance_port_t* port,
                              nx_flash_maintenance_snapshot_t* snapshot) {
    if (!valid(port) || snapshot == NULL)
        return NX_ERROR_INVALID;
    return port->ops->snapshot != NULL
               ? port->ops->snapshot(port->context, snapshot)
               : NX_ERROR_UNSUPPORTED;
}

nx_result_t
nx_flash_maintenance_otp_read(const nx_flash_maintenance_port_t* port,
                              uint32_t offset, void* data, size_t length) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->otp_read != NULL
               ? port->ops->otp_read(port->context, offset, data, length)
               : NX_ERROR_UNSUPPORTED;
}

nx_result_t nx_flash_maintenance_otp_program(
    const nx_flash_maintenance_port_t* port, uint32_t offset, const void* data,
    size_t length, uint32_t acknowledged_effects, nx_time_us_t deadline) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->otp_program != NULL
               ? port->ops->otp_program(port->context, offset, data, length,
                                        acknowledged_effects, deadline)
               : NX_ERROR_UNSUPPORTED;
}

nx_result_t
nx_flash_maintenance_otp_lock(const nx_flash_maintenance_port_t* port,
                              uint32_t block, uint32_t acknowledged_effects,
                              nx_time_us_t deadline) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->otp_lock != NULL
               ? port->ops->otp_lock(port->context, block, acknowledged_effects,
                                     deadline)
               : NX_ERROR_UNSUPPORTED;
}

nx_result_t nx_flash_maintenance_set_options(
    const nx_flash_maintenance_port_t* port, const nx_flash_options_t* desired,
    uint32_t acknowledged_effects, nx_time_us_t deadline) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->set_options != NULL
               ? port->ops->set_options(port->context, desired,
                                        acknowledged_effects, deadline)
               : NX_ERROR_UNSUPPORTED;
}
