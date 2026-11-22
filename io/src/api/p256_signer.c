/**
 * \file            p256_signer.c
 * \brief           Checked fixed-size P256 signer face dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/p256_signer.h"

static bool span(const void* pointer, size_t bytes, size_t alignment) {
    return pointer != NULL && (uintptr_t)pointer % alignment == 0 &&
           bytes <= UINTPTR_MAX - (uintptr_t)pointer;
}

static bool overlaps(const void* a, size_t a_bytes, const void* b,
                     size_t b_bytes) {
    const uintptr_t left = (uintptr_t)a;
    const uintptr_t right = (uintptr_t)b;
    return left <= right ? right - left < a_bytes : left - right < b_bytes;
}

static bool face(const nx_p256_signer_t* signer) {
    return span(signer, sizeof(*signer), _Alignof(nx_p256_signer_t)) &&
           signer->context != NULL &&
           span(signer->ops, sizeof(*signer->ops),
                _Alignof(nx_p256_signer_ops_t));
}

static bool payload(const nx_p256_signer_t* signer, const void* data,
                    size_t bytes) {
    return span(data, bytes, 1) &&
           !overlaps(signer, sizeof(*signer), data, bytes) &&
           !overlaps(signer->ops, sizeof(*signer->ops), data, bytes) &&
           data != signer->context;
}

nx_result_t nx_p256_signer_public_key(const nx_p256_signer_t* signer,
                                      nx_p256_public_key_t* out,
                                      nx_time_us_t deadline) {
    if (!face(signer) || !payload(signer, out, sizeof(*out)) ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    return signer->ops->public_key
               ? signer->ops->public_key(signer->context, out, deadline)
               : NX_ERROR_UNSUPPORTED;
}

nx_result_t nx_p256_signer_sign_digest(const nx_p256_signer_t* signer,
                                       const nx_p256_digest_t* digest,
                                       nx_p256_signature_t* out,
                                       nx_time_us_t deadline) {
    if (!face(signer) || !payload(signer, digest, sizeof(*digest)) ||
        !payload(signer, out, sizeof(*out)) ||
        overlaps(digest, sizeof(*digest), out, sizeof(*out)) ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    return signer->ops->sign_digest
               ? signer->ops->sign_digest(signer->context, digest, out,
                                          deadline)
               : NX_ERROR_UNSUPPORTED;
}
