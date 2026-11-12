/**
 * \file            hash.h
 * \brief           Single-shot hardware digest/HMAC and retained byte spans
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_HASH_H
#define NEXUS_IO_HASH_H
#include "nexus/core/request.h"
#include "nexus/io/fwd.h"
#include "nexus/io/wake.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Exact standard operation; hardware availability remains explicit. */
typedef enum {
    NX_HASH_SHA1,
    NX_HASH_MD5,
    NX_HASH_HMAC_SHA1,
    NX_HASH_HMAC_MD5
} nx_hash_algorithm_t;
/** \brief Readonly algorithm/transport constraints, no hidden readable padding.
 * \note Alignment applies only to the named start address. A message/key with
 *       a nonmultiple-of-four length never implies extra readable bytes.
 */
typedef struct {
    size_t input_alignment;
    size_t key_alignment;
    size_t digest_alignment;
    size_t maximum_bytes;
    size_t maximum_key_bytes;
    uint8_t digest_bytes;
    bool empty_message;
    bool empty_key;
} nx_hash_constraints_t;
/** \brief Value tuple; referenced spans remain explicit caller storage.
 * \note An empty input may use NULL. Plain hash requires NULL key/zero key
 *       bytes; HMAC may use an empty key if the provider explicitly supports
 *       it. digest_capacity is at least 20 bytes for SHA1 or 16 for MD5. Only
 * the algorithm's digest bytes are written; the remainder is untouched.
 */
typedef struct {
    nx_hash_algorithm_t algorithm;
    const void* input;
    size_t bytes;
    const void* key;
    size_t key_bytes;
    void* digest;
    size_t digest_capacity;
} nx_hash_operation_t;
/** \brief Caller-owned one-shot request and byte-span lifetime.
 * \note ACCEPTED retains descriptor, message, key and digest until observed
 *       SETTLED. A failed hardware/context/secret cleanup retains the borrow
 *       in QUARANTINED. Caller key is never erased by this provider.
 *       Digest is valid only on SUCCESS; failure may alter its contents.
 *       transferred is the digest byte count on success and zero on error.
 *       Every terminal path withdraws sources/IRQ references and clears
 *       provider scratch, key/context/FIFO residue and HAL references, or
 *       proves the documented exclusive HASH reset. Refused cleanup keeps the
 *       borrow QUARANTINED. Shared RNG source/IRQ ownership is preserved.
 */
typedef struct {
    nx_request_t base;
    nx_hash_operation_t operation;
} nx_hash_request_t;
/** \brief Shared immutable operations; no queue, worker, heap or key store. */
typedef struct {
    nx_result_t (*query)(void*, nx_hash_algorithm_t, nx_hash_constraints_t*);
    nx_result_t (*submit)(void*, nx_hash_request_t*);
    nx_result_t (*cancel)(void*, nx_hash_request_t*);
    nx_result_t (*service)(void*);
    nx_result_t (*stop)(void*);
    nx_result_t (*attach_wake)(void*, const nx_irq_wake_t*, uint8_t);
} nx_hash_ops_t;
/** \brief Stable two-pointer face; lookup does not initialize hardware. */
struct nx_hash_port {
    const nx_hash_ops_t* ops;
    void* context;
};
/** \brief Query exact selected constraints with no hardware or borrow effect.
 * \param[in] port: Stable selected face and private owner storage.
 * \param[in] algorithm: SHA1/MD5 or explicitly keyed variant.
 * \param[out] constraints: Independent output, changed only on success.
 * \return SUCCESS, UNSUPPORTED, INVALID or provider error.
 * \note Readonly; no key policy, TLS binding or certification is implied.
 */
nx_result_t nx_hash_port_query(const nx_hash_port_t* port,
                               nx_hash_algorithm_t algorithm,
                               nx_hash_constraints_t* constraints);
/** \brief Prepare a one-shot tuple without reading any message/key byte.
 * \param[in,out] request: Initialized READY or consumed SETTLED storage.
 * \param[in] operation: Value copied; its object is not retained.
 * \param[in] deadline: Finite absolute Core monotonic deadline.
 * \return SUCCESS or INVALID/STATE, with no tuple mutation on rejection.
 * \note Every referenced byte span (message/key/digest) is disjoint from the
 *       entire mutable request. Digest is also disjoint from message/key; the
 *       operation argument itself is a copied value, never a retained span.
 *       Provider-specific limits reject before borrowing or hardware effects.
 *       No source padding or full-message bounce buffer is required by this
 *       interface.
 */
nx_result_t nx_hash_prepare(nx_hash_request_t* request,
                            const nx_hash_operation_t* operation,
                            nx_time_us_t deadline);
/** \brief Admit exactly one operation on an initialized serialized owner.
 * \param[in] port: Privileged unmasked task executor.
 * \param[in,out] request: READY descriptor and immutable input/key spans.
 * \return SUCCESS means ACCEPTED; rejection retains no reference/effect.
 * \note Keep every span live through cancellation and verified cleanup.
 */
nx_result_t nx_hash_port_submit(const nx_hash_port_t* port,
                                nx_hash_request_t* request);
/** \brief Cancel without revoking the active message/key/digest borrow.
 * \param[in] port: Same serialized executor as submit.
 * \param[in,out] request: Active descriptor or observed SETTLED identity.
 * \return SUCCESS requests cancellation; STATE means another request.
 * \note First observed terminal cause wins; cleanup can outlive its deadline.
 */
nx_result_t nx_hash_port_cancel(const nx_hash_port_t* port,
                                nx_hash_request_t* request);
/** \brief Observe hardware/deadline and make bounded cleanup progress.
 * \param[in] port: Same unmasked owner, called throughout stop/drain.
 * \return SUCCESS after progress; BUSY/IO may retain QUARANTINED storage.
 * \note No busy wait inside the HASH/RNG IRQ and no implicit worker.
 *       The bounded FIFO provider requires explicit service while HMAC is
 *       active, including between its inner/key phases. A wake is a hint;
 *       waiting only for a final digest IRQ cannot advance those phases.
 */
nx_result_t nx_hash_port_service(const nx_hash_port_t* port);
/** \brief Seal admission and retire hardware, key/context and IRQ references.
 * \param[in] port: Serialized owner with producers/observers quiesced.
 * \return SUCCESS only after withdrawal; BUSY/IO retains responsibility.
 * \note Callback completion alone cannot establish secret/context cleanup.
 */
nx_result_t nx_hash_port_stop(const nx_hash_port_t* port);
/** \brief Attach one optional IRQ hint under the existing ceiling contract.
 * \param[in] port: Live owner with no active request.
 * \param[in] wake: Stable explicit binding or NULL to detach.
 * \param[in] syscall_ceiling: Existing backend IRQ admission bound.
 * \return SUCCESS or INVALID/CONTEXT/STATE/BUSY/UNSUPPORTED.
 * \note Retain wake storage until source and publishers have withdrawn.
 */
nx_result_t nx_hash_port_attach_wake(const nx_hash_port_t* port,
                                     const nx_irq_wake_t* wake,
                                     uint8_t syscall_ceiling);
#ifdef __cplusplus
}
#endif
#endif
