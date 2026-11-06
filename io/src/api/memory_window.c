/**
 * \file            memory_window.c
 * \brief           Checked exact CPU memory-window loan dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/memory_window.h"

/** \brief Validate the complete loan storage before any provider mutation. */
static bool output_valid(const nx_memory_window_port_t* port,
                         const void* output, size_t bytes, size_t alignment) {
    uintptr_t begin = (uintptr_t)output;
    uintptr_t face = (uintptr_t)port;
    uintptr_t ops = port == NULL ? 0U : (uintptr_t)port->ops;
    if (port == NULL || port->ops == NULL || port->context == NULL ||
        output == NULL || begin % alignment != 0U ||
        begin > UINTPTR_MAX - bytes || face > UINTPTR_MAX - sizeof(*port) ||
        ops > UINTPTR_MAX - sizeof(*port->ops))
        return false;
    return !(begin < face + sizeof(*port) && face < begin + bytes) &&
           !(begin < ops + sizeof(*port->ops) && ops < begin + bytes);
}

nx_result_t nx_memory_window_port_acquire(const nx_memory_window_port_t* port,
                                          nx_memory_window_lease_t* lease) {
    if (!output_valid(port, lease, sizeof(*lease),
                      _Alignof(nx_memory_window_lease_t)))
        return NX_ERROR_INVALID;
    if (port->ops->acquire == NULL)
        return NX_ERROR_UNSUPPORTED;
    return port->ops->acquire(port->context, lease);
}

nx_result_t nx_memory_window_port_release(const nx_memory_window_port_t* port,
                                          nx_memory_window_lease_t* lease,
                                          uint64_t epoch) {
    if (!output_valid(port, lease, sizeof(*lease),
                      _Alignof(nx_memory_window_lease_t)) ||
        epoch == 0U)
        return NX_ERROR_INVALID;
    if (port->ops->release == NULL)
        return NX_ERROR_UNSUPPORTED;
    return port->ops->release(port->context, lease, epoch);
}

nx_result_t
nx_memory_window_port_geometry(const nx_memory_window_port_t* port,
                               nx_memory_window_geometry_t* geometry) {
    if (!output_valid(port, geometry, sizeof(*geometry),
                      _Alignof(nx_memory_window_geometry_t)))
        return NX_ERROR_INVALID;
    if (port->ops->geometry == NULL)
        return NX_ERROR_UNSUPPORTED;
    return port->ops->geometry(port->context, geometry);
}
