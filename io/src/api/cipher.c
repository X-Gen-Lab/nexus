/**
 * \file            cipher.c
 * \brief           Standard cipher spans and checked immutable-face dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "crypto_validation.h"

nx_result_t nx_cipher_operation_check(const nx_cipher_request_t* request,
                                      const nx_cipher_operation_t* operation,
                                      nx_time_us_t deadline) {
    if (!nx_crypto_span_valid(request, sizeof(*request),
                              _Alignof(nx_cipher_request_t), false) ||
        !nx_crypto_span_valid(operation, sizeof(*operation),
                              _Alignof(nx_cipher_operation_t), false) ||
        deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    const nx_cipher_operation_t* o = operation;
    if ((unsigned)o->algorithm > NX_CIPHER_TDES_CBC ||
        (unsigned)o->direction > NX_CIPHER_DECRYPT)
        return NX_ERROR_INVALID;
    bool aes = o->algorithm <= NX_CIPHER_AES_CTR;
    bool des =
        o->algorithm == NX_CIPHER_DES_ECB || o->algorithm == NX_CIPHER_DES_CBC;
    bool ecb = o->algorithm == NX_CIPHER_AES_ECB ||
               o->algorithm == NX_CIPHER_DES_ECB ||
               o->algorithm == NX_CIPHER_TDES_ECB;
    size_t block = aes ? 16U : 8U;
    if ((aes && o->key_bytes != 16U && o->key_bytes != 24U &&
         o->key_bytes != 32U) ||
        (des && o->key_bytes != 8U) ||
        (!aes && !des && o->key_bytes != 16U && o->key_bytes != 24U) ||
        (ecb ? o->iv != NULL || o->iv_bytes != 0U : o->iv_bytes != block) ||
        (o->algorithm != NX_CIPHER_AES_CTR && o->bytes % block != 0U) ||
        !nx_crypto_span_valid(o->input, o->bytes, 1U, false) ||
        !nx_crypto_span_valid(o->output, o->bytes, 1U, false) ||
        !nx_crypto_span_valid(o->key, o->key_bytes, 1U, false) ||
        !nx_crypto_span_valid(o->iv, o->iv_bytes, 1U, ecb))
        return NX_ERROR_INVALID;
    if (nx_crypto_overlap(request, sizeof(*request), o->input, o->bytes) ||
        nx_crypto_overlap(request, sizeof(*request), o->output, o->bytes) ||
        nx_crypto_overlap(request, sizeof(*request), o->key, o->key_bytes) ||
        nx_crypto_overlap(request, sizeof(*request), o->iv, o->iv_bytes) ||
        nx_crypto_overlap(o->output, o->bytes, o->key, o->key_bytes) ||
        nx_crypto_overlap(o->output, o->bytes, o->iv, o->iv_bytes) ||
        (o->input != o->output &&
         nx_crypto_overlap(o->input, o->bytes, o->output, o->bytes)))
        return NX_ERROR_INVALID;
    return NX_SUCCESS;
}

nx_result_t nx_cipher_prepare(nx_cipher_request_t* request,
                              const nx_cipher_operation_t* operation,
                              nx_time_us_t deadline) {
    if (!nx_crypto_span_valid(request, sizeof(*request),
                              _Alignof(nx_cipher_request_t), false))
        return NX_ERROR_INVALID;
    nx_request_state_t state = nx_request_state(&request->base);
    if (state != NX_REQUEST_READY && state != NX_REQUEST_SETTLED)
        return NX_ERROR_STATE;
    nx_result_t result =
        nx_cipher_operation_check(request, operation, deadline);
    if (result != NX_SUCCESS)
        return result;
    /* operation may be request->operation; snapshot before resetting base. */
    nx_cipher_operation_t value = *operation;
    result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS)
        request->operation = value;
    return result;
}

static bool valid(const nx_cipher_port_t* port) {
    return nx_crypto_span_valid(port, sizeof(*port), _Alignof(nx_cipher_port_t),
                                false) &&
           nx_crypto_span_valid(port->ops, sizeof(*port->ops),
                                _Alignof(nx_cipher_ops_t), false) &&
           port->context != NULL;
}
/** \brief Mutable outputs never overwrite the public immutable identity. */
static bool controls(const nx_cipher_port_t* port, const void* p, size_t n) {
    return nx_crypto_overlap(p, n, port, sizeof(*port)) ||
           nx_crypto_overlap(p, n, port->ops, sizeof(*port->ops));
}
nx_result_t nx_cipher_port_query(const nx_cipher_port_t* port,
                                 nx_cipher_algorithm_t algorithm,
                                 nx_cipher_constraints_t* result) {
    if (!valid(port) || (unsigned)algorithm > NX_CIPHER_TDES_CBC ||
        !nx_crypto_span_valid(result, sizeof(*result),
                              _Alignof(nx_cipher_constraints_t), false) ||
        controls(port, result, sizeof(*result)))
        return NX_ERROR_INVALID;
    return port->ops->query == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->query(port->context, algorithm, result);
}
nx_result_t nx_cipher_port_submit(const nx_cipher_port_t* port,
                                  nx_cipher_request_t* request) {
    if (!valid(port) ||
        !nx_crypto_span_valid(request, sizeof(*request),
                              _Alignof(nx_cipher_request_t), false) ||
        controls(port, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    if (nx_request_state(&request->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    if (nx_cipher_operation_check(request, &request->operation,
                                  request->base.deadline) != NX_SUCCESS ||
        controls(port, request->operation.output, request->operation.bytes))
        return NX_ERROR_INVALID;
    return port->ops->submit == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->submit(port->context, request);
}
nx_result_t nx_cipher_port_cancel(const nx_cipher_port_t* port,
                                  nx_cipher_request_t* request) {
    if (!valid(port) ||
        !nx_crypto_span_valid(request, sizeof(*request),
                              _Alignof(nx_cipher_request_t), false) ||
        controls(port, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    return port->ops->cancel == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->cancel(port->context, request);
}
nx_result_t nx_cipher_port_service(const nx_cipher_port_t* port) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->service(port->context);
}
nx_result_t nx_cipher_port_stop(const nx_cipher_port_t* port) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}
nx_result_t nx_cipher_port_attach_wake(const nx_cipher_port_t* port,
                                       const nx_irq_wake_t* wake,
                                       uint8_t ceiling) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->attach_wake == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->attach_wake(port->context, wake, ceiling);
}
