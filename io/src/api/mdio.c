/**
 * \file            mdio.c
 * \brief           Narrow Clause22 exact-context dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/mdio.h"

nx_result_t nx_mdio_read(const nx_mdio_port_t* port, uint8_t address,
                         uint8_t reg, uint16_t* value, nx_time_us_t deadline) {
    if (port == NULL || port->ops == NULL || value == NULL || address > 31U ||
        reg > 31U || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    return port->ops->read == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->read(port->context, address, reg, value, deadline);
}
nx_result_t nx_mdio_write(const nx_mdio_port_t* port, uint8_t address,
                          uint8_t reg, uint16_t value, nx_time_us_t deadline) {
    if (port == NULL || port->ops == NULL || address > 31U || reg > 31U ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    return port->ops->write == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->write(port->context, address, reg, value, deadline);
}
