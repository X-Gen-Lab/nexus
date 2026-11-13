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

/** \brief Reject invalid geometry and aliases before changing caller storage.
 */
static nx_result_t prepare(nx_block_request_t* request, uint64_t first_block,
                           const void* data, size_t data_bytes, uint32_t blocks,
                           nx_time_us_t deadline) {
    uintptr_t control = (uintptr_t)request;
    uintptr_t payload = (uintptr_t)data;
    if (request == NULL || control % _Alignof(nx_block_request_t) != 0U ||
        sizeof(*request) > UINTPTR_MAX - control || data == NULL ||
        data_bytes == 0U || data_bytes > UINTPTR_MAX - payload ||
        blocks == 0U || first_block > UINT64_MAX - (uint64_t)(blocks - 1U) ||
        deadline == NX_DEADLINE_NEVER ||
        (control < payload + data_bytes &&
         payload < control + sizeof(*request)))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->first_block = first_block;
        request->data_bytes = data_bytes;
        request->blocks = blocks;
    }
    return result;
}

nx_result_t nx_block_read_prepare(nx_block_request_t* request,
                                  uint64_t first_block, void* data,
                                  size_t data_bytes, uint32_t blocks,
                                  nx_time_us_t deadline) {
    nx_result_t result =
        prepare(request, first_block, data, data_bytes, blocks, deadline);
    if (result == NX_SUCCESS) {
        request->buffer.rx = data;
        request->write = false;
    }
    return result;
}

nx_result_t nx_block_write_prepare(nx_block_request_t* request,
                                   uint64_t first_block, const void* data,
                                   size_t data_bytes, uint32_t blocks,
                                   nx_time_us_t deadline) {
    nx_result_t result =
        prepare(request, first_block, data, data_bytes, blocks, deadline);
    if (result == NX_SUCCESS) {
        request->buffer.tx = data;
        request->write = true;
    }
    return result;
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
