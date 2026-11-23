/**
 * \file            p256_signer.h
 * \brief           Finite SHA256 digest signing with a non-exported P256 key
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_P256_SIGNER_H
#define NEXUS_IO_P256_SIGNER_H
#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Exactly 32 SHA256 digest bytes, supplied and hashed by the caller. */
typedef struct {
    uint8_t bytes[32];
} nx_p256_digest_t;
/** \brief P256 uncompressed X then Y, each 32-byte big-endian, without 04 tag.
 */
typedef struct {
    uint8_t xy[64];
} nx_p256_public_key_t;
/** \brief ECDSA P256 R then S, each 32-byte big-endian; no ASN.1/DER framing.
 */
typedef struct {
    uint8_t rs[64];
} nx_p256_signature_t;
/** \brief Shared readonly methods; each implements the public call contract. */
typedef struct {
    nx_result_t (*public_key)(void* context, nx_p256_public_key_t* out,
                              nx_time_us_t deadline);
    nx_result_t (*sign_digest)(void* context, const nx_p256_digest_t* digest,
                               nx_p256_signature_t* out, nx_time_us_t deadline);
} nx_p256_signer_ops_t;
/**
 * \brief Existing initialized signer identity, without private key material.
 * \note Face/ops/context stay live through calls and joined provider stop.
 *       One task executor serializes operations. No allocation or key
 * discovery.
 */
typedef struct nx_p256_signer {
    const nx_p256_signer_ops_t* ops;
    void* context;
} nx_p256_signer_t;
/**
 * \brief Read the public key for the already selected signing key.
 * \param[in] signer: Stable readonly face and initialized private provider.
 * \param[out] out: Distinct writable complete key, unchanged on failure.
 * \param[in] deadline: Finite absolute provider-monotonic deadline.
 * \return SUCCESS, INVALID/UNSUPPORTED/STATE or actual provider error.
 * \note Task-only synchronous settled call: no retained output after return.
 *       Hardware identity/provisioning and authenticity need external policy.
 *       Output cannot overlap face/ops or private provider control storage.
 */
nx_result_t nx_p256_signer_public_key(const nx_p256_signer_t* signer,
                                      nx_p256_public_key_t* out,
                                      nx_time_us_t deadline);
/**
 * \brief Sign a caller-supplied digest without exposing private key bytes.
 * \param[in] signer: Stable same-executor selected face and provider state.
 * \param[in] digest: Complete immutable SHA256 digest, borrowed during call.
 * \param[out] out: Disjoint complete raw R/S signature, unchanged on failure.
 * \param[in] deadline: Finite whole-operation absolute provider deadline.
 * \return SUCCESS, INVALID/UNSUPPORTED/STATE or actual provider error.
 * \note Synchronous settled buffers, including failure. No implicit hash, DER,
 *       low-S normalization, certificate, authorization or private-key export.
 *       Input/output are disjoint from each other and private control storage.
 *       A deadline cannot preempt an already running device computation.
 */
nx_result_t nx_p256_signer_sign_digest(const nx_p256_signer_t* signer,
                                       const nx_p256_digest_t* digest,
                                       nx_p256_signature_t* out,
                                       nx_time_us_t deadline);
#ifdef __cplusplus
}
#endif
#endif
