/**
 * \file            atecc608b.h
 * \brief           Read-only locked ATECC608B profile and opaque P256 signer
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_ATECC608B_H
#define NEXUS_COMPONENTS_ATECC608B_H
#include "nexus/io/i2c.h"
#include "nexus/io/p256_signer.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Public readback facts, not a secret or anti-counterfeit proof. */
typedef struct {
    uint8_t serial[9];
    uint8_t revision[4];
    uint8_t signing_slot;
} nx_atecc608b_identity_t;
/** \brief Device identity references caller private workspace while bound. */
typedef struct {
    void* state;
} nx_atecc608b_t;
/**
 * \brief Explicit settled wire, timing and key-slot binding.
 * \note Polling endpoint transactions release every MCU buffer before return.
 *       Wake callback creates the selected electrical pulse and tWHI wait;
 *       the component verifies the four-byte wake response itself. Delay waits
 *       the requested microseconds or returns an error within the same finite
 *       deadline. Both callbacks are task-only and retain no arguments.
 *       Maximum_polls bounds receive attempts across one public operation.
 *       One physical device has one live component owner, with one selected
 *       key. Slot 0..15 must already hold an approved locked private P256 key.
 *       Endpoint/ops, active scope, device and used workspace stay stable and
 *       disjoint. Opaque endpoint/clock/callback contexts also stay disjoint
 *       from mutable workspace, device and outputs; callers check their full
 *       extents because the component cannot inspect private context sizes.
 */
typedef struct {
    const nx_i2c_endpoint_t* endpoint;
    nx_clock_t clock;
    nx_result_t (*wake)(void* context, nx_time_us_t deadline);
    nx_result_t (*delay)(void* context, uint32_t us, nx_time_us_t deadline);
    void* context;
    uint16_t maximum_polls;
    uint8_t signing_slot;
} nx_atecc608b_config_t;
/**
 * \brief Consumer-owned active operation for vendor context-free delay hooks.
 * \note One execution domain explicitly publishes this scope before entering
 *       a component call and withdraws it after return. No concurrent operation
 *       or other CryptoAuthLib instance may replace it. This is not a library
 *       current-device global, thread-local registry or execution sandbox.
 */
typedef struct {
    nx_atecc608b_t* device;
    nx_time_us_t deadline;
} nx_atecc608b_operation_t;
/**
 * \brief Consumer supplies the exact current operation, NULL outside calls.
 * \return Stable caller scope naming this device and finite deadline.
 * \note Explicit static-binding function, like a volume-operation getter.
 *       Do not change or reclaim it until the entire component call returns.
 */
const nx_atecc608b_operation_t* nx_atecc608b_operation_get(void);
/** \brief Exact fixed-profile workspace size; no heap or global packet pool. */
size_t nx_atecc608b_workspace_bytes(void);
/** \brief Required private-workspace alignment. */
size_t nx_atecc608b_workspace_alignment(void);
/**
 * \brief Verify exact B revision, both lock bytes, serial and signing slot.
 * \param[in,out] device: Fresh zero identity, with no concurrent face users.
 * \param[in] config: Complete stable binding, copied during this call.
 * \param[in,out] workspace: Aligned caller storage, retained while bound.
 * \param[in] bytes: At least the exact workspace size.
 * \param[in] deadline: Finite whole-operation monotonic budget, matching scope.
 * \return SUCCESS, INVALID/STATE/PERMISSION/UNSUPPORTED or transport error.
 * \note Only Info and Config reads are issued. 00006003 is the reviewed B
 *       DevRev. Both zone lock bytes and the selected SlotLocked bit must be
 *       zero. The slot must be private, secret P256 with ExternalSign and
 *       public-info, and no selected auth/random requirement.
 *       Argument/time preflight failures retain nothing. After physical access
 *       starts, any failure retains this binding, unready, for explicit stop.
 *       No provisioning, key generation, Lock, Write, OTP or reset is exposed.
 */
nx_result_t nx_atecc608b_init(nx_atecc608b_t* device,
                              const nx_atecc608b_config_t* config,
                              void* workspace, size_t bytes,
                              nx_time_us_t deadline);
/**
 * \brief Return the existing ready readonly signer face, or NULL if unready.
 * \param[in] device: Stable bound device, serialized with calls and stop.
 * \return Existing face without clock, probing, allocation or initialization.
 * \note Join every face user before stop or workspace reuse. A returned face
 *       retains no new hardware ownership and exposes no key bytes or SDK type.
 */
const nx_p256_signer_t* nx_atecc608b_signer(const nx_atecc608b_t* device);
/**
 * \brief Copy cached successful initialization identity without hardware I/O.
 * \param[in] device: Bound ready same-executor device.
 * \param[out] out: Separate copied facts, unchanged on rejection.
 * \return SUCCESS, INVALID or STATE; no retained output.
 * \note Cached facts do not detect replacement, authenticate hardware or prove
 *       private-key possession. Actual public-key/sign operations read
 * hardware.
 */
nx_result_t nx_atecc608b_read_identity(const nx_atecc608b_t* device,
                                       nx_atecc608b_identity_t* out);
/**
 * \brief Close signing admission and request explicit device sleep.
 * \param[in,out] device: Same bound identity after all other users join.
 * \param[in] deadline: Fresh finite cleanup budget, matching active scope.
 * \return SUCCESS clears the binding; failure retains it for cleanup retry.
 * \note Synchronous I2C buffers are settled on every return. Failed sleep
 *       leaves device state uncertain and retains workspace/ports. No key
 *       material is erased, generated, exported or reprovisioned. A stopped
 *       component needs explicit reinitialization before signing again.
 */
nx_result_t nx_atecc608b_stop(nx_atecc608b_t* device, nx_time_us_t deadline);
#ifdef __cplusplus
}
#endif
#endif
