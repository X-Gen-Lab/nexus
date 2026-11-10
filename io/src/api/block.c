/**
 * \file            block.c
 * \brief           Checked synchronous block dispatch through readonly methods
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/block.h"

nx_result_t nx_block_port_media_removed(const nx_block_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL)
        return NX_ERROR_INVALID;
    return port->ops->media_removed != NULL
               ? port->ops->media_removed(port->context)
               : NX_ERROR_UNSUPPORTED;
}

/** \brief Dispatch queries through the exact immutable instance face. */
const nx_block_geometry_t* nx_block_port_geometry(const nx_block_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL ||
        port->ops->geometry == NULL)
        return NULL;
    return port->ops->geometry(port->context);
}

/** \brief Missing faces cannot assert initialized media availability. */
nx_block_state_t nx_block_port_state(const nx_block_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL ||
        port->ops->state == NULL)
        return NX_BLOCK_UNAVAILABLE;
    return port->ops->state(port->context);
}

/** \brief Activation is explicit, never a factory lookup side effect. */
nx_result_t nx_block_port_activate(const nx_block_port_t* port,
                                   nx_time_us_t deadline) {
    if (port == NULL || port->ops == NULL || port->context == NULL)
        return NX_ERROR_INVALID;
    if (port->ops->activate == NULL)
        return NX_ERROR_UNSUPPORTED;
    return port->ops->activate(port->context, deadline);
}

/** \brief Preserve the provider's confirmed prefix and failure status. */
nx_block_io_result_t nx_block_port_read(const nx_block_port_t* port,
                                        uint64_t first_block, void* data,
                                        size_t data_bytes, uint32_t blocks,
                                        nx_time_us_t deadline) {
    if (port == NULL || port->ops == NULL || port->context == NULL)
        return (nx_block_io_result_t){NX_ERROR_INVALID, 0U};
    if (port->ops->read == NULL)
        return (nx_block_io_result_t){NX_ERROR_UNSUPPORTED, 0U};
    return port->ops->read(port->context, first_block, data, data_bytes, blocks,
                           deadline);
}

/** \brief A failure never converts an unconfirmed write into a full prefix. */
nx_block_io_result_t nx_block_port_write(const nx_block_port_t* port,
                                         uint64_t first_block, const void* data,
                                         size_t data_bytes, uint32_t blocks,
                                         nx_time_us_t deadline) {
    if (port == NULL || port->ops == NULL || port->context == NULL)
        return (nx_block_io_result_t){NX_ERROR_INVALID, 0U};
    if (port->ops->write == NULL)
        return (nx_block_io_result_t){NX_ERROR_UNSUPPORTED, 0U};
    return port->ops->write(port->context, first_block, data, data_bytes,
                            blocks, deadline);
}

/** \brief Synchronize without discovering or replacing media. */
nx_result_t nx_block_port_sync(const nx_block_port_t* port,
                               nx_time_us_t deadline) {
    if (port == NULL || port->ops == NULL || port->context == NULL)
        return NX_ERROR_INVALID;
    if (port->ops->sync == NULL)
        return NX_ERROR_UNSUPPORTED;
    return port->ops->sync(port->context, deadline);
}

/** \brief Refused withdrawal leaves the exact provider owner intact. */
nx_result_t nx_block_port_stop(const nx_block_port_t* port,
                               nx_time_us_t deadline) {
    if (port == NULL || port->ops == NULL || port->context == NULL)
        return NX_ERROR_INVALID;
    if (port->ops->stop == NULL)
        return NX_ERROR_UNSUPPORTED;
    return port->ops->stop(port->context, deadline);
}
