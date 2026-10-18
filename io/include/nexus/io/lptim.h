/**
 * \file            lptim.h
 * \brief           Finite 16-bit counter shots in a distinct tick domain
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_LPTIM_H
#define NEXUS_IO_LPTIM_H
#include "nexus/core/request.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Owner and nonwrapping shot incarnation, never an RTC/Core token.
 * \note Identity is compared without dereference. Tokens/observers must be
 *       retired before the owner storage lifetime ends or its address is
 * reused.
 */
typedef struct {
    const void* identity;
    uint64_t generation;
} nx_lptim_domain_t;
/** \brief Declared units and modulo counter, not an absolute time estimate.
 * \note A complete 65536-tick shot wraps counter to zero. Counter zero alone
 *       cannot establish completion, idle, missed wraps or low-power
 * continuity. tick_hz is a fixed declared rate, not physical oscillator
 * accuracy.
 */
typedef struct {
    nx_lptim_domain_t domain;
    uint32_t tick_hz;
    uint16_t counter;
    bool running;
} nx_lptim_sample_t;
/** \brief One caller-owned finite shot; transferred is final observed ticks.
 * \note Initialize base first. After ACCEPTED keep the entire request live and
 *       caller immutable until acquire-observed SETTLED and external joins.
 *       A latched CANCELLED/TIMEOUT fixes the cause, not the final progress:
 *       held withdrawal may allow further source ticks before detachment.
 */
typedef struct {
    nx_request_t base;
    nx_lptim_domain_t domain;
    uint32_t ticks;
} nx_lptim_request_t;
/** \brief Shared readonly methods implement the corresponding public contract.
 */
typedef struct {
    nx_result_t (*sample)(const void* context, nx_lptim_sample_t* sample);
    nx_result_t (*submit)(void* context, nx_lptim_request_t* request);
    nx_result_t (*cancel)(void* context, nx_lptim_request_t* request);
    nx_result_t (*service)(void* context);
    nx_result_t (*stop)(void* context);
} nx_lptim_ops_t;
/** \brief Immutable two-pointer face; provider storage remains private. */
typedef struct {
    const nx_lptim_ops_t* ops;
    void* context;
} nx_lptim_port_t;
/** \brief Prepare initialized unborrowed storage with copied shot metadata.
 * \param[in,out] request: READY or consumed SETTLED with all observers joined.
 * \param[in] domain: Separate nonzero owner/generation value copied here.
 * \param[in] ticks: Exact positive count, 1..65536, in this domain's ticks.
 * \param[in] deadline: Absolute Core monotonic deadline or NX_DEADLINE_NEVER.
 * \return SUCCESS or INVALID/STATE; failure leaves every request byte
 * unchanged. \note Complete spans must be aligned, nonwrapping and disjoint. No
 * loan, clock access, unit conversion or physical rate qualification occurs.
 */
nx_result_t nx_lptim_request_prepare(nx_lptim_request_t* request,
                                     const nx_lptim_domain_t* domain,
                                     uint32_t ticks, nx_time_us_t deadline);
/** \brief Copy the live counter domain and declared units on one task owner.
 * \param[in] port: Serialized immutable face; no concurrent model/lifecycle.
 * \param[out] sample: Separate aligned writable snapshot.
 * \return SUCCESS, STATE if closed, or INVALID/UNSUPPORTED; rejected output is
 * unchanged. \note Open idle reports counter zero and not running. This finite
 * interface does not reconstruct unobserved wraps or provide an absolute clock.
 */
nx_result_t nx_lptim_port_sample(const nx_lptim_port_t* port,
                                 nx_lptim_sample_t* sample);
/** \brief Borrow one READY shot after all idle/domain/range checks.
 * \param[in] port: Same serialized owner with a copied external Core clock.
 * \param[in,out] request: Caller request; no queue or hidden worker.
 * \return SUCCESS means ACCEPTED;
 * INVALID/STATE/BUSY/TIMEOUT/EXHAUSTED/UNSUPPORTED rejects without retained
 * references or effects. \note Expired finite deadline rejects TIMEOUT before
 * source effects. Reserve a final nonwrapping domain increment before
 * acceptance: EXHAUSTED never abandons a previously accepted loan. Clock
 * context lifetime, complete extent/disjointness and non-reentry are assembly
 * responsibilities.
 */
nx_result_t nx_lptim_port_submit(const nx_lptim_port_t* port,
                                 nx_lptim_request_t* request);
/** \brief Accept cancellation, retaining the loan until proved withdrawal.
 * \param[in] port: Same serialized executor; no implicit cross-task locking.
 * \param[in,out] request: Exact active identity; foreign identity is not read.
 * \return SUCCESS may still mean QUARANTINED; rejection is
 * STATE/INVALID/UNSUPPORTED. \note Cancel/stop may win before unobserved
 * event/expiry. First cause stays fixed; transferred counts final source
 * progress at actual withdrawal.
 */
nx_result_t nx_lptim_port_cancel(const nx_lptim_port_t* port,
                                 nx_lptim_request_t* request);
/** \brief Observe deadline/event and continue bounded real withdrawal.
 * \param[in] port: Same serialized task owner; no implicit source advance.
 * \return SUCCESS when idle/settled, BUSY while active or withdrawal is held;
 * INVALID/STATE/UNSUPPORTED may reject dispatch. \note First observed expiry
 * wins over pending completion. Clear all source, pending and owner references
 * before final release SETTLED publication.
 */
nx_result_t nx_lptim_port_service(const nx_lptim_port_t* port);
/** \brief Close admission; cancelled/expired is never a loan-release proof.
 * \param[in] port: Serialized owner, with all external publishers quiescent.
 * \return SUCCESS after detachment, BUSY retains quarantined storage;
 * INVALID/STATE/UNSUPPORTED may reject dispatch. \note Repeated clean stop is
 * safe; it does not reset generation or lifetime. Tick rate/source and
 * backup/Stop/VBAT continuity need separate hardware contracts; this interface
 * establishes none of those qualifications.
 */
nx_result_t nx_lptim_port_stop(const nx_lptim_port_t* port);
#ifdef __cplusplus
}
#endif
#endif
