/**
 * \file            crypto_validation.h
 * \brief           Shared pure byte-span admission for cipher and digest
 * \author          Nexus Team
 */
#ifndef NEXUS_CRYPTO_VALIDATION_H
#define NEXUS_CRYPTO_VALIDATION_H
#include "nexus/io/cipher.h"
#include "nexus/io/hash.h"

/** \brief Validate arithmetic before any overlap calculation or access. */
static inline bool nx_crypto_span_valid(const void* data, size_t bytes,
                                        size_t alignment, bool allow_empty) {
    if (bytes == 0U)
        return allow_empty;
    return data != NULL && alignment != 0U &&
           (uintptr_t)data % alignment == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)data;
}
/** \brief Compare already validated ranges; empty spans never overlap. */
static inline bool nx_crypto_overlap(const void* a, size_t an, const void* b,
                                     size_t bn) {
    return an != 0U && bn != 0U && (uintptr_t)a < (uintptr_t)b + bn &&
           (uintptr_t)b < (uintptr_t)a + an;
}
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Check standard cipher shape without touching state or payload.
 * \param[in] request: Entire descriptor extent, not read by this function.
 * \param[in] operation: Readable value tuple; no pointed-to bytes are read.
 * \param[in] deadline: Finite absolute deadline; no clock is queried.
 * \return SUCCESS or INVALID with no mutation or hardware effects.
 * \note Provider adds selected alignment/size limits and private alias checks.
 */
nx_result_t nx_cipher_operation_check(const nx_cipher_request_t* request,
                                      const nx_cipher_operation_t* operation,
                                      nx_time_us_t deadline);
/** \brief Check digest/HMAC shape without reading message or secret bytes.
 * \param[in] request: Entire descriptor extent, not read by this function.
 * \param[in] operation: Readable tuple; empty message/key need no storage.
 * \param[in] deadline: Finite absolute deadline; no clock is queried.
 * \return SUCCESS or INVALID, preserving every input and output object.
 * \note Provider adds selected transport limits and opaque owner isolation.
 */
nx_result_t nx_hash_operation_check(const nx_hash_request_t* request,
                                    const nx_hash_operation_t* operation,
                                    nx_time_us_t deadline);
#ifdef __cplusplus
}
#endif
#endif
