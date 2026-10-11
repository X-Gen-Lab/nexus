/**
 * \file            i2c.c
 * \brief           Checked typed dispatch and finite message admission bounds
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
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

/** \brief Address intervals are checked before any overflow-prone addition. */
static bool overlap(uintptr_t first, size_t first_size, uintptr_t second,
                    size_t second_size) {
    return first < second + second_size && second < first + first_size;
}

/** \brief Reject mutable/control aliasing before acquiring hardware or storage.
 */
static nx_result_t validate_fields(const nx_i2c_request_t* request,
                                   const nx_i2c_message_t* messages,
                                   size_t count) {
    if (request == NULL ||
        (uintptr_t)request % _Alignof(nx_i2c_request_t) != 0U ||
        messages == NULL || count == 0U ||
        (uintptr_t)messages % _Alignof(nx_i2c_message_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (count > NX_I2C_MAX_MESSAGES) {
        return NX_ERROR_UNSUPPORTED;
    }
    uintptr_t control = (uintptr_t)request;
    uintptr_t descriptors = (uintptr_t)messages;
    size_t descriptor_size = count * sizeof(*messages);
    if (sizeof(*request) > UINTPTR_MAX - control ||
        descriptor_size > UINTPTR_MAX - descriptors ||
        overlap(control, sizeof(*request), descriptors, descriptor_size)) {
        return NX_ERROR_INVALID;
    }
    for (size_t i = 0U; i < count; ++i) {
        const nx_i2c_message_t* item = &messages[i];
        uintptr_t data = (uintptr_t)item->data;
        if (item->data == NULL || item->length == 0U ||
            item->length > UINTPTR_MAX - data) {
            return NX_ERROR_INVALID;
        }
        if (item->length > NX_I2C_MAX_MESSAGE_BYTES) {
            return NX_ERROR_UNSUPPORTED;
        }
        if (overlap(data, item->length, control, sizeof(*request)) ||
            overlap(data, item->length, descriptors, descriptor_size)) {
            return NX_ERROR_INVALID;
        }
        for (size_t j = 0U; j < i; ++j) {
            const nx_i2c_message_t* prior = &messages[j];
            if (item->read != prior->read &&
                overlap(data, item->length, (uintptr_t)prior->data,
                        prior->length)) {
                return NX_ERROR_INVALID;
            }
        }
    }
    return NX_SUCCESS;
}

/** \brief Validate the eventual control address without a temporary request. */
nx_result_t nx_i2c_request_validate(const nx_i2c_request_t* request) {
    if (request == NULL ||
        (uintptr_t)request % _Alignof(nx_i2c_request_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    return validate_fields(request, request->messages, request->count);
}

/** \brief Preparation cannot retain payload or change a hardware source. */
nx_result_t nx_i2c_request_prepare(nx_i2c_request_t* request,
                                   nx_i2c_message_t* messages, size_t count,
                                   nx_time_us_t deadline) {
    if (request == NULL ||
        (uintptr_t)request % _Alignof(nx_i2c_request_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = validate_fields(request, messages, count);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->messages = messages;
        request->count = count;
    }
    return result;
}

/** \brief Optional finite modes never silently execute a polling transaction.
 */
nx_result_t nx_i2c_endpoint_submit(const nx_i2c_endpoint_t* port,
                                   nx_i2c_request_t* request) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->submit != NULL ? port->ops->submit(port->context, request)
                                     : NX_ERROR_UNSUPPORTED;
}

/** \brief Transfer an existing adapter loan without duplicate admission. */
nx_result_t nx_i2c_endpoint_start_admitted(const nx_i2c_endpoint_t* port,
                                           nx_i2c_request_t* request) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->start_admitted != NULL
               ? port->ops->start_admitted(port->context, request)
               : NX_ERROR_UNSUPPORTED;
}

/** \brief Exact controller identity precedes adapter admission. */
bool nx_i2c_endpoint_on_port(const nx_i2c_endpoint_t* endpoint,
                             const nx_i2c_port_t* port) {
    return endpoint != NULL && endpoint->ops != NULL &&
           endpoint->context != NULL && port != NULL && port->ops != NULL &&
           port->context != NULL && endpoint->ops->on_port != NULL &&
           endpoint->ops->on_port(endpoint->context, port);
}

/** \brief Cancellation requests drain rather than revoking any loan. */
nx_result_t nx_i2c_port_cancel(const nx_i2c_port_t* port,
                               nx_i2c_request_t* request) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->cancel != NULL ? port->ops->cancel(port->context, request)
                                     : NX_ERROR_UNSUPPORTED;
}

/** \brief Bounded service retains the provider's authoritative request state.
 */
void nx_i2c_port_service(const nx_i2c_port_t* port) {
    if (port != NULL && port->ops != NULL && port->context != NULL &&
        port->ops->service != NULL) {
        port->ops->service(port->context);
    }
}

/** \brief Successful stop proves complete source release. */
nx_result_t nx_i2c_port_stop(const nx_i2c_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop != NULL ? port->ops->stop(port->context)
                                   : NX_ERROR_UNSUPPORTED;
}

/** \brief Actual publisher policy is validated before retaining a wake sink. */
nx_result_t nx_i2c_port_attach_wake(const nx_i2c_port_t* port,
                                    const nx_irq_wake_t* wake,
                                    uint8_t syscall_ceiling) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->attach_wake != NULL
               ? port->ops->attach_wake(port->context, wake, syscall_ceiling)
               : NX_ERROR_UNSUPPORTED;
}
