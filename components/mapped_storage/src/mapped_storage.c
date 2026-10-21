/**
 * \file            mapped_storage.c
 * \brief           Checked mapped-storage dispatch and unborrowed preparation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/mapped_storage.h"

/** \brief Prove representable input and request spans before overlap tests. */
static bool separate(const void* first, size_t first_size, const void* second,
                     size_t second_size) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return first != NULL && second != NULL && first_size <= UINTPTR_MAX - a &&
           second_size <= UINTPTR_MAX - b &&
           (a + first_size <= b || b + second_size <= a);
}

/** \brief Prepare only unborrowed metadata; never read the input payload. */
nx_result_t nx_mapped_storage_prepare(nx_mapped_storage_request_t* request,
                                      nx_mapped_storage_operation_t operation,
                                      size_t offset, const void* data,
                                      size_t length, nx_time_us_t deadline) {
    if (request == NULL || length == 0 || length > SIZE_MAX - offset ||
        (operation != NX_MAPPED_STORAGE_PROGRAM &&
         operation != NX_MAPPED_STORAGE_ERASE) ||
        (operation == NX_MAPPED_STORAGE_ERASE && data != NULL) ||
        (operation == NX_MAPPED_STORAGE_PROGRAM &&
         !separate(data, length, request, sizeof(*request)))) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->operation = operation;
        request->offset = offset;
        request->data = data;
        request->length = length;
    }
    return result;
}

/** \brief Reject invalid faces before any provider access. */
static bool valid(const nx_mapped_storage_port_t* port) {
    return port != NULL && port->ops != NULL && port->context != NULL;
}

/** \brief Dispatch immutable geometry using exact instance state. */
const nx_mapped_storage_geometry_t*
nx_mapped_storage_geometry(const nx_mapped_storage_port_t* port) {
    return valid(port) && port->ops->geometry != NULL
               ? port->ops->geometry(port->context)
               : NULL;
}

/** \brief Dispatch mapping admission. */
nx_result_t nx_mapped_storage_acquire(const nx_mapped_storage_port_t* port,
                                      nx_mapped_storage_reader_t reader,
                                      size_t offset, size_t length,
                                      nx_mapped_storage_loan_t* loan) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->acquire == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->acquire(port->context, reader, offset, length,
                                    loan);
}

/** \brief Dispatch exact loan release. */
nx_result_t nx_mapped_storage_release(const nx_mapped_storage_port_t* port,
                                      const nx_mapped_storage_loan_t* loan) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->release == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->release(port->context, loan);
}

/** \brief Dispatch exclusive request admission. */
nx_result_t nx_mapped_storage_submit(const nx_mapped_storage_port_t* port,
                                     nx_mapped_storage_request_t* request,
                                     nx_mapped_storage_ticket_t* ticket) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->submit == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->submit(port->context, request, ticket);
}

/** \brief Dispatch stable-identity cancellation. */
nx_result_t nx_mapped_storage_cancel(const nx_mapped_storage_port_t* port,
                                     nx_mapped_storage_ticket_t ticket) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->cancel == NULL ? NX_ERROR_UNSUPPORTED
                                     : port->ops->cancel(port->context, ticket);
}

/** \brief Dispatch a caller-scheduled phase boundary. */
void nx_mapped_storage_service(const nx_mapped_storage_port_t* port) {
    if (valid(port) && port->ops->service != NULL) {
        port->ops->service(port->context);
    }
}

/** \brief Dispatch admission closure without revocation. */
nx_result_t nx_mapped_storage_stop(const nx_mapped_storage_port_t* port) {
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}
