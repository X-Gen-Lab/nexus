/**
 * \file            cipher.h
 * \brief           Single-shot hardware cipher requests and explicit spans
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_CIPHER_H
#define NEXUS_IO_CIPHER_H
#include "nexus/core/request.h"
#include "nexus/io/fwd.h"
#include "nexus/io/wake.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Standard algorithms; a provider explicitly reports its constraints.
 */
typedef enum {
    NX_CIPHER_AES_ECB,
    NX_CIPHER_AES_CBC,
    NX_CIPHER_AES_CTR,
    NX_CIPHER_DES_ECB,
    NX_CIPHER_DES_CBC,
    NX_CIPHER_TDES_ECB,
    NX_CIPHER_TDES_CBC
} nx_cipher_algorithm_t;
/** \brief Direction does not change key/IV encoding or buffer ownership. */
typedef enum { NX_CIPHER_ENCRYPT, NX_CIPHER_DECRYPT } nx_cipher_direction_t;
#define NX_CIPHER_KEY_8  UINT32_C(1)
#define NX_CIPHER_KEY_16 UINT32_C(2)
#define NX_CIPHER_KEY_24 UINT32_C(4)
#define NX_CIPHER_KEY_32 UINT32_C(8)
/** \brief Readonly transport constraints for one standard algorithm.
 * \note A CTR provider can report length_multiple=1 or a whole-block value;
 *       CTR itself does not imply a sixteen-byte request requirement.
 */
typedef struct {
    size_t alignment;
    size_t length_multiple;
    size_t maximum_bytes;
    uint32_t key_sizes;
    uint8_t iv_bytes;
    bool exact_in_place;
} nx_cipher_constraints_t;
/** \brief Value copied at preparation; referenced storage is never copied.
 * \note key/iv contain standard algorithm byte order, not vendor words.
 *       bytes is positive; output provides exactly bytes writable bytes.
 *       ECB has no IV; CBC/CTR take a caller-selected IV/counter. No IV
 *       generation, padding, authentication or secret-store policy is implied.
 */
typedef struct {
    nx_cipher_algorithm_t algorithm;
    nx_cipher_direction_t direction;
    const void* input;
    void* output;
    size_t bytes;
    const void* key;
    size_t key_bytes;
    const void* iv;
    size_t iv_bytes;
} nx_cipher_operation_t;
/** \brief Caller-owned request; initialize base before the first prepare.
 * \note ACCEPTED retains this descriptor and input/output/key/IV through
 *       acquire-observed SETTLED. Failed drain/secret cleanup retains them in
 *       QUARANTINED. Original caller key/IV are never erased by this provider.
 *       transferred counts bytes actually written in complete output blocks;
 *       a future byte-tail provider counts only its actual tail bytes. Accepted
 *       failure can leave a partial prefix. This is not authenticated output.
 *       Every terminal path first withdraws peripheral/IRQ references and
 *       clears provider scratch, hardware key/IV/context/FIFO residue and HAL
 *       pointer references, or proves the documented exclusive reset. BUSY or
 *       refused cleanup keeps the borrow QUARANTINED; not only stop cleans it.
 */
typedef struct {
    nx_request_t base;
    nx_cipher_operation_t operation;
} nx_cipher_request_t;
/** \brief Immutable shared methods; one task serializes each controller. */
typedef struct {
    nx_result_t (*query)(void*, nx_cipher_algorithm_t,
                         nx_cipher_constraints_t*);
    nx_result_t (*submit)(void*, nx_cipher_request_t*);
    nx_result_t (*cancel)(void*, nx_cipher_request_t*);
    nx_result_t (*service)(void*);
    nx_result_t (*stop)(void*);
    nx_result_t (*attach_wake)(void*, const nx_irq_wake_t*, uint8_t);
} nx_cipher_ops_t;
/** \brief Stable two-pointer face; factory lookup never initializes hardware.
 */
struct nx_cipher_port {
    const nx_cipher_ops_t* ops;
    void* context;
};
/** \brief Query a selected algorithm without hardware effects or borrowing.
 * \param[in] port: Stable selected face and live private owner storage.
 * \param[in] algorithm: Standard algorithm selector.
 * \param[out] constraints: Independent output, changed only on success.
 * \return SUCCESS, UNSUPPORTED, INVALID or provider error.
 * \note Readonly, no allocation or implicit start. Query does not establish
 *       physical qualification or approve product cryptographic policy.
 */
nx_result_t nx_cipher_port_query(const nx_cipher_port_t* port,
                                 nx_cipher_algorithm_t algorithm,
                                 nx_cipher_constraints_t* constraints);
/** \brief Copy a valid value tuple into unborrowed request storage.
 * \param[in,out] request: Initialized READY or consumed SETTLED descriptor.
 * \param[in] operation: Value copied here; it need not remain live afterward.
 * \param[in] deadline: Finite absolute Core monotonic deadline.
 * \return SUCCESS or INVALID/STATE; rejection leaves the tuple unchanged.
 * \note Provider-specific alignment/granularity is checked at submit. Generic
 *       CTR preparation does not require whole blocks. Partial input/output
 *       overlap is invalid; exact in-place is subject to the provider query.
 *       Every referenced byte span (input/output/key/IV) is disjoint from the
 *       entire mutable request. Output is also disjoint from key/IV. The
 *       operation argument itself is a copied value, never a retained span.
 *       All spans are disjoint from opaque provider state/config, proved by
 *       selected admission.
 */
nx_result_t nx_cipher_prepare(nx_cipher_request_t* request,
                              const nx_cipher_operation_t* operation,
                              nx_time_us_t deadline);
/** \brief Admit one operation; rejection has no retained or hardware effect.
 * \param[in] port: Initialized privileged task owner, with IRQs unmasked.
 * \param[in,out] request: READY tuple and stable referenced spans.
 * \return SUCCESS means ACCEPTED, including later settled start failure.
 * \note No queue, worker, allocation or concurrent control is supplied.
 */
nx_result_t nx_cipher_port_submit(const nx_cipher_port_t* port,
                                  nx_cipher_request_t* request);
/** \brief Request cancellation without revoking any borrowed span.
 * \param[in] port: Same serialized task owner as submit.
 * \param[in,out] request: Exact active descriptor or observed SETTLED identity.
 * \return SUCCESS requests cancellation; STATE means another request.
 * \note First terminal cause wins; hardware and secret cleanup still follow.
 */
nx_result_t nx_cipher_port_cancel(const nx_cipher_port_t* port,
                                  nx_cipher_request_t* request);
/** \brief Make bounded deadline/drain/cleanup progress without waiting.
 * \param[in] port: Same unmasked task owner, called through stop/recovery.
 * \return SUCCESS after progress; BUSY/IO can retain quarantined storage.
 * \note HAL completion is a fact; it does not alone publish SETTLED.
 */
nx_result_t nx_cipher_port_service(const nx_cipher_port_t* port);
/** \brief Close admission and drain the active operation and secret state.
 * \param[in] port: Serialized owner; producers/observers must quiesce.
 * \return SUCCESS after complete withdrawal; BUSY/IO retains responsibility.
 * \note No task or notification destruction. Late IRQ cannot access retired
 *       request/key/config storage after successful stop.
 */
nx_result_t nx_cipher_port_stop(const nx_cipher_port_t* port);
/** \brief Bind an optional IRQ hint, independent of request state predicates.
 * \param[in] port: Live serialized task owner, no active request.
 * \param[in] wake: Stable explicit hint binding, or NULL to detach.
 * \param[in] syscall_ceiling: Existing backend IRQ ceiling contract.
 * \return SUCCESS or INVALID/CONTEXT/STATE/BUSY/UNSUPPORTED.
 * \note Wake storage remains live through source/publisher withdrawal.
 */
nx_result_t nx_cipher_port_attach_wake(const nx_cipher_port_t* port,
                                       const nx_irq_wake_t* wake,
                                       uint8_t syscall_ceiling);
#ifdef __cplusplus
}
#endif
#endif
