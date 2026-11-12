/**
 * \file            hash.c
 * \brief           Digest/HMAC byte spans and checked immutable-face dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "crypto_validation.h"

nx_result_t nx_hash_operation_check(const nx_hash_request_t* request,
                                    const nx_hash_operation_t* operation,
                                    nx_time_us_t deadline) {
    if (!nx_crypto_span_valid(request, sizeof(*request),
                              _Alignof(nx_hash_request_t), false) ||
        !nx_crypto_span_valid(operation, sizeof(*operation),
                              _Alignof(nx_hash_operation_t), false) ||
        deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    const nx_hash_operation_t* o = operation;
    if ((unsigned)o->algorithm > NX_HASH_HMAC_MD5)
        return NX_ERROR_INVALID;
    bool keyed =
        o->algorithm == NX_HASH_HMAC_SHA1 || o->algorithm == NX_HASH_HMAC_MD5;
    size_t digest =
        o->algorithm == NX_HASH_SHA1 || o->algorithm == NX_HASH_HMAC_SHA1 ? 20U
                                                                          : 16U;
    if ((!keyed && (o->key != NULL || o->key_bytes != 0U)) ||
        o->digest_capacity < digest ||
        !nx_crypto_span_valid(o->input, o->bytes, 1U, true) ||
        !nx_crypto_span_valid(o->key, o->key_bytes, 1U, true) ||
        !nx_crypto_span_valid(o->digest, o->digest_capacity, 1U, false))
        return NX_ERROR_INVALID;
    if (nx_crypto_overlap(request, sizeof(*request), o->input, o->bytes) ||
        nx_crypto_overlap(request, sizeof(*request), o->key, o->key_bytes) ||
        nx_crypto_overlap(request, sizeof(*request), o->digest,
                          o->digest_capacity) ||
        nx_crypto_overlap(o->digest, o->digest_capacity, o->input, o->bytes) ||
        nx_crypto_overlap(o->digest, o->digest_capacity, o->key, o->key_bytes))
        return NX_ERROR_INVALID;
    return NX_SUCCESS;
}
nx_result_t nx_hash_prepare(nx_hash_request_t* request,
                            const nx_hash_operation_t* operation,
                            nx_time_us_t deadline) {
    if (!nx_crypto_span_valid(request, sizeof(*request),
                              _Alignof(nx_hash_request_t), false))
        return NX_ERROR_INVALID;
    nx_request_state_t state = nx_request_state(&request->base);
    if (state != NX_REQUEST_READY && state != NX_REQUEST_SETTLED)
        return NX_ERROR_STATE;
    nx_result_t result = nx_hash_operation_check(request, operation, deadline);
    if (result != NX_SUCCESS)
        return result;
    nx_hash_operation_t value = *operation;
    result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS)
        request->operation = value;
    return result;
}
static bool valid(const nx_hash_port_t* port) {
    return nx_crypto_span_valid(port, sizeof(*port), _Alignof(nx_hash_port_t),
                                false) &&
           nx_crypto_span_valid(port->ops, sizeof(*port->ops),
                                _Alignof(nx_hash_ops_t), false) &&
           port->context != NULL;
}
static bool controls(const nx_hash_port_t* port, const void* p, size_t n) {
    return nx_crypto_overlap(p, n, port, sizeof(*port)) ||
           nx_crypto_overlap(p, n, port->ops, sizeof(*port->ops));
}
nx_result_t nx_hash_port_query(const nx_hash_port_t* port,
                               nx_hash_algorithm_t algorithm,
                               nx_hash_constraints_t* result) {
    if (!valid(port) || (unsigned)algorithm > NX_HASH_HMAC_MD5 ||
        !nx_crypto_span_valid(result, sizeof(*result),
                              _Alignof(nx_hash_constraints_t), false) ||
        controls(port, result, sizeof(*result)))
        return NX_ERROR_INVALID;
    return port->ops->query == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->query(port->context, algorithm, result);
}
nx_result_t nx_hash_port_submit(const nx_hash_port_t* port,
                                nx_hash_request_t* request) {
    if (!valid(port) ||
        !nx_crypto_span_valid(request, sizeof(*request),
                              _Alignof(nx_hash_request_t), false) ||
        controls(port, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    if (nx_request_state(&request->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    if (nx_hash_operation_check(request, &request->operation,
                                request->base.deadline) != NX_SUCCESS ||
        controls(port, request->operation.digest,
                 request->operation.digest_capacity))
        return NX_ERROR_INVALID;
    return port->ops->submit == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->submit(port->context, request);
}
nx_result_t nx_hash_port_cancel(const nx_hash_port_t* port,
                                nx_hash_request_t* request) {
    if (!valid(port) ||
        !nx_crypto_span_valid(request, sizeof(*request),
                              _Alignof(nx_hash_request_t), false) ||
        controls(port, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    return port->ops->cancel == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->cancel(port->context, request);
}
nx_result_t nx_hash_port_service(const nx_hash_port_t* port) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->service(port->context);
}
nx_result_t nx_hash_port_stop(const nx_hash_port_t* port) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}
nx_result_t nx_hash_port_attach_wake(const nx_hash_port_t* port,
                                     const nx_irq_wake_t* wake,
                                     uint8_t ceiling) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->attach_wake == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->attach_wake(port->context, wake, ceiling);
}
