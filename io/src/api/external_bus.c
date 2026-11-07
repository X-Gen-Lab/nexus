/**
 * \file            external_bus.c
 * \brief           Checked shared external-controller lifecycle dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/external_bus.h"

nx_external_bus_state_t
nx_external_bus_port_state(const nx_external_bus_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL ||
        port->ops->state == NULL)
        return NX_EXTERNAL_BUS_UNAVAILABLE;
    return port->ops->state(port->context);
}

nx_result_t nx_external_bus_port_stop(const nx_external_bus_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL)
        return NX_ERROR_INVALID;
    if (port->ops->stop == NULL)
        return NX_ERROR_UNSUPPORTED;
    return port->ops->stop(port->context);
}
