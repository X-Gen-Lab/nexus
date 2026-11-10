/**
 * \file            block_async.c
 * \brief           Checked optional block transport dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/block_async.h"

static bool valid(const nx_block_async_port_t* port) {
    return port != NULL && port->ops != NULL && port->context != NULL;
}

nx_result_t nx_block_async_submit(const nx_block_async_port_t* port,
                                  nx_block_request_t* request) {
    if (!valid(port) || request == NULL)
        return NX_ERROR_INVALID;
    return port->ops->submit != NULL ? port->ops->submit(port->context, request)
                                     : NX_ERROR_UNSUPPORTED;
}

nx_result_t nx_block_async_cancel(const nx_block_async_port_t* port,
                                  nx_block_request_t* request) {
    if (!valid(port) || request == NULL)
        return NX_ERROR_INVALID;
    return port->ops->cancel != NULL ? port->ops->cancel(port->context, request)
                                     : NX_ERROR_UNSUPPORTED;
}

void nx_block_async_service(const nx_block_async_port_t* port) {
    if (valid(port) && port->ops->service != NULL)
        port->ops->service(port->context);
}

nx_result_t nx_block_async_media_removed(const nx_block_async_port_t* port) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->media_removed != NULL
               ? port->ops->media_removed(port->context)
               : NX_ERROR_UNSUPPORTED;
}
