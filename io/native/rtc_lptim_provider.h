/**
 * \file            rtc_lptim_provider.h
 * \brief           Private caller-owned raw calendar and finite counter state
 * \author          Nexus Team
 */
#ifndef NEXUS_NATIVE_RTC_LPTIM_PROVIDER_H
#define NEXUS_NATIVE_RTC_LPTIM_PROVIDER_H
#include "nexus/io/native/rtc_lptim_model.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief One stable owner; construct once, never memset a live/stopped owner.
 */
typedef struct {
    nx_clock_t clock;
    nx_rtc_request_t* active;
    nx_rtc_calendar_t calendar;
    nx_rtc_alarm_t alarm;
    uint64_t generation;
    nx_result_t terminal_result;
    bool constructed;
    bool opened;
    bool valid;
    bool armed;
    bool pending;
    bool hold_withdraw;
    bool terminal;
} nx_native_rtc_state_t;
/** \brief Source progress is separate from the borrowed request metadata. */
typedef struct {
    nx_clock_t clock;
    nx_lptim_request_t* active;
    uint64_t generation;
    uint32_t tick_hz;
    uint32_t limit;
    uint32_t counted;
    nx_result_t terminal_result;
    bool constructed;
    bool opened;
    bool armed;
    bool pending;
    bool hold_withdraw;
    bool terminal;
} nx_native_lptim_state_t;
extern const nx_rtc_ops_t nx_native_rtc_ops;
extern const nx_lptim_ops_t nx_native_lptim_ops;
/** \brief Construct fresh zero caller storage exactly once, without reading
 * time. \param[in,out] state: Aligned zero owner, lifetime covers all
 * loans/joins. \param[in] clock: Separate descriptor copied here; read is
 * non-NULL, context may be NULL for a stateless reader. Context stays borrowed.
 * \return SUCCESS or INVALID/STATE, with no change on rejection.
 * \note Caller guarantees context lifetime, whole extent/disjointness, finite
 *       monotonic readings and non-reentry. No default calendar is supplied.
 *       A stopped same-lifetime owner cannot be reconstructed/reset to epoch1.
 */
nx_result_t nx_native_rtc_construct(nx_native_rtc_state_t* state,
                                    const nx_clock_t* clock);
/** \brief Construct a fresh finite counter owner without reading any clock.
 * \param[in,out] state: Fresh zero caller-owned storage, never a revoked loan.
 * \param[in] clock: Descriptor copied; context has the RTC constructor
 * contract. \param[in] tick_hz: Positive fixed declared source frequency.
 * \return SUCCESS or INVALID/STATE; failure leaves all owner bytes unchanged.
 */
nx_result_t nx_native_lptim_construct(nx_native_lptim_state_t* state,
                                      const nx_clock_t* clock,
                                      uint32_t tick_hz);
/** \brief Reopen a constructed, fully joined, closed RTC owner.
 * \param[in,out] state: Same stable owner on its single serialized executor.
 * \return SUCCESS, INVALID for a malformed span, BUSY for retained source or
 *         request responsibility, STATE if unconstructed/open, or EXHAUSTED.
 * \note Retained active/armed/pending/terminal responsibility takes BUSY
 *       priority. Every rejection leaves all owner bytes unchanged. Success
 *       increments a nonwrapping generation and preserves calendar/set-unset,
 *       copied clock binding and external withdrawal fault. It never reads or
 *       advances Core time, reconstructs storage or clears a fault. Retire all
 *       old request/control/observer loans first; old READY domains stay stale.
 */
nx_result_t nx_native_rtc_start(nx_native_rtc_state_t* state);
/** \brief Reopen a joined finite counter without resetting its incarnation.
 * \param[in,out] state: Same constructed closed owner, with all loans joined.
 * \return SUCCESS or INVALID/BUSY/STATE/EXHAUSTED; failure is byte-preserving.
 * \note BUSY has priority for active/armed/pending/terminal responsibility.
 *       Preserve clock, declared tick_hz and external faults; only then advance
 *       a nonwrapping generation and open admission. No clock read, source
 *       advance, global identity, pool or implicit reconstruction is performed.
 */
nx_result_t nx_native_lptim_start(nx_native_lptim_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
