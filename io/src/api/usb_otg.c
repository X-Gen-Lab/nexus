/**
 * \file            usb_otg.c
 * \brief           Managed USB role face dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/usb_otg.h"

/** \brief Factory identities are immutable and do not acquire an owner. */
static bool valid(const nx_usb_otg_port_t* port) {
    return port != NULL && port->ops != NULL && port->context != NULL;
}

/** \brief Generation admission and all effects remain with the provider. */
nx_result_t nx_usb_otg_request(const nx_usb_otg_port_t* port,
                               uint64_t generation, nx_usb_otg_role_t role,
                               nx_time_us_t deadline) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->request == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->request(port->context, generation, role, deadline);
}

/** \brief The caller explicitly advances the one serialized executor. */
nx_result_t nx_usb_otg_service(const nx_usb_otg_port_t* port) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->service(port->context);
}

/** \brief Provider validation protects caller output and immutable metadata. */
nx_result_t nx_usb_otg_state(const nx_usb_otg_port_t* port,
                             nx_usb_otg_state_t* state) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->state == NULL ? NX_ERROR_UNSUPPORTED
                                    : port->ops->state(port->context, state);
}

/** \brief An inactive, busy or stale generation has no current Device face. */
const nx_usb_device_port_t* nx_usb_otg_device(const nx_usb_otg_port_t* port,
                                              uint64_t generation) {
    return !valid(port) || port->ops->device == NULL
               ? NULL
               : port->ops->device(port->context, generation);
}

/** \brief An inactive, busy or stale generation has no current Host face. */
const nx_usb_host_port_t* nx_usb_otg_host(const nx_usb_otg_port_t* port,
                                          uint64_t generation) {
    return !valid(port) || port->ops->host == NULL
               ? NULL
               : port->ops->host(port->context, generation);
}
