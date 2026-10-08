/**
 * @file update.h
 * @brief SEC002: bounded, durable update policy; no bootloader or flash writer.
 *
 * All calls are synchronous, task-context only, and externally serialized.
 * The service owns no heap memory. Port callbacks must finish using borrowed
 * buffers before returning. A boot choice means verified eligibility only;
 * the product bootloader must implement secure transfer and protected slots.
 */
#ifndef NEXUS_UPDATE_H
#define NEXUS_UPDATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NX_UPDATE_MANIFEST_SCHEMA 1u
#define NX_UPDATE_SIGNED_SIZE 72u
#define NX_UPDATE_MANIFEST_SIZE 136u
#define NX_UPDATE_RECORD_SIZE 312u
#define NX_UPDATE_MAX_ATTEMPTS 32u

typedef enum {
    NX_UPDATE_OK = 0,
    NX_UPDATE_EINVAL,
    NX_UPDATE_ENOTFOUND,
    NX_UPDATE_ESTORAGE,
    NX_UPDATE_ECORRUPT,
    NX_UPDATE_EAUTH,
    NX_UPDATE_EBOARD,
    NX_UPDATE_EDIGEST,
    NX_UPDATE_EDOWNGRADE,
    NX_UPDATE_ESTATE,
    NX_UPDATE_ECOUNTER,
    NX_UPDATE_EXHAUSTED
} nx_update_status_t;

typedef enum {
    NX_UPDATE_IDLE = 0,
    NX_UPDATE_STAGED,
    NX_UPDATE_TRIAL,
    NX_UPDATE_CONFIRMED,
    NX_UPDATE_ROLLBACK
} nx_update_phase_t;

typedef struct {
    uint32_t schema;
    uint32_t board_id;
    uint32_t board_revision;
    uint32_t key_id;
    uint32_t image_size;
    uint64_t firmware_version;
    uint64_t security_version;
    uint8_t digest[32];
    uint8_t signature[64];
} nx_update_manifest_t;

typedef struct {
    nx_update_phase_t phase;
    uint32_t attempts;
    uint32_t attempt_limit;
    bool has_active;
    bool has_candidate;
    bool confirm_pending;
    uint32_t active_slot;
    uint32_t candidate_slot;
    nx_update_status_t rollback_reason;
    nx_update_manifest_t active;
    nx_update_manifest_t candidate;
} nx_update_state_t;

typedef struct {
    void* user;
    uint32_t board_id;
    uint32_t board_revision;
    uint32_t trial_limit;
    uint32_t maximum_image_size;
    /** Exact record, verified durable storage; ENOTFOUND means truly blank. */
    nx_update_status_t (*load)(void*, uint8_t*, size_t);
    /** Atomic replace: a failure may have committed old OR new complete record.
     * Service invalidates itself on failure; reopen storage and reinitialize. */
    nx_update_status_t (*save)(void*, const uint8_t*, size_t);
    /** Hash precisely image_size bytes of the immutable, bounded slot. */
    nx_update_status_t (*image_digest)(void*, uint32_t slot,
                                       uint32_t image_size, uint8_t digest[32]);
    /** Resolve key_id only through the product's pinned/revoked trust store. */
    nx_update_status_t (*verify_signature)(void*, uint32_t key_id,
                                           const uint8_t*, size_t,
                                           const uint8_t signature[64]);
    /** Hardware-protected, durable monotonic security-version floor. */
    nx_update_status_t (*counter_read)(void*, uint64_t*);
    /** Atomic compare-and-advance; retry reads the counter after any failure. */
    nx_update_status_t (*counter_advance)(void*, uint64_t expected,
                                         uint64_t target);
} nx_update_port_t;

typedef struct {
    nx_update_port_t port;
    nx_update_state_t state;
    bool initialized;
} nx_update_t;

typedef struct {
    uint32_t slot;
    nx_update_phase_t phase;
    nx_update_status_t rollback_reason;
    nx_update_manifest_t manifest;
} nx_update_choice_t;

/** Explicit little-endian signing/persistence codec; never sign C structs. */
nx_update_status_t nx_update_manifest_encode(const nx_update_manifest_t*,
                                             uint8_t output[NX_UPDATE_MANIFEST_SIZE]);
nx_update_status_t nx_update_manifest_decode(const uint8_t*, size_t,
                                             nx_update_manifest_t*);

/** Loads policy state only; performs no boot, Flash erase or image installation.
 * Required after a save error to resolve an ambiguous durable commit. */
nx_update_status_t nx_update_init(nx_update_t*, const nx_update_port_t*);
/** Factory baseline must equal the provisioned counter, and is fully verified. */
nx_update_status_t nx_update_provision(nx_update_t*, uint32_t slot,
                                      const nx_update_manifest_t*);
/** Image must already exist in a distinct immutable slot; verifies before save. */
nx_update_status_t nx_update_stage(nx_update_t*, uint32_t slot,
                                  const nx_update_manifest_t*);
/**
 * Revalidates a slot and saves its trial attempt BEFORE returning eligibility.
 * Trial exhaustion or candidate authentication failure falls back only to a
 * verified last-confirmed identity. Output is untouched on any error.
 */
nx_update_status_t nx_update_select(nx_update_t*, nx_update_choice_t*);
/**
 * Product reports its health policy satisfied for the actual selected slot and
 * digest. Durable intent precedes counter advance; restart resumes that intent.
 * This does not measure health or boot success. No counter is ever decremented.
 */
nx_update_status_t nx_update_confirm(nx_update_t*, uint32_t running_slot,
                                    const uint8_t running_digest[32]);
/** Explicit rollback; rejects rollback once counter advancement has committed. */
nx_update_status_t nx_update_rollback(nx_update_t*);

#ifdef __cplusplus
}
#endif
#endif
