/**
 * \file            i2c.c
 * \brief           Checked typed dispatch through shared read-only provider
 * methods \author          Nexus Team \version         1.0.0 \date 2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/i2c.h"

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_i2c_port_recover(const nx_i2c_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->recover == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->recover(port->context);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_i2c_endpoint_transaction(const nx_i2c_endpoint_t* port,
                                        nx_i2c_message_t* messages,
                                        size_t count, nx_time_us_t deadline,
                                        size_t* transferred) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->transaction == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->transaction(port->context, messages, count, deadline,
                                  transferred);
}
