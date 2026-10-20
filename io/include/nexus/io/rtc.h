/**
 * \file            rtc.h
 * \brief           Raw calendar and one finite alarm in its own time domain
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_RTC_H
#define NEXUS_IO_RTC_H
#include "nexus/core/request.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Raw YY calendar; no UTC, century, timezone or oscillator policy.
 * \note YY is 00..99 and leap years satisfy YY modulo 4 == 0. Weekday is an
 *       independent 1..7 value, not inferred from the date. Other fields use
 *       month/day 1-based and 24-hour hour/minute/second values, without leap
 *       seconds. Natural year99->00 wrap does not reset the setting domain.
 */
typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} nx_rtc_calendar_t;
/** \brief Opaque owner identity and non-repeating setting incarnation.
 * \note Compare identity without dereference. Valid only in that owner storage
 *       lifetime; retire all tokens/observers before ending or reusing it.
 *       This is neither authorization nor a cross-boot/backup-reset identity.
 */
typedef struct {
    const void* identity;
    uint64_t generation;
} nx_rtc_domain_t;
/** \brief Calendar and setting domain copied in one serialized operation. */
typedef struct {
    nx_rtc_domain_t domain;
    nx_rtc_calendar_t calendar;
} nx_rtc_snapshot_t;
/** \brief Exact future day/time match; no month/year or wildcard masks. */
typedef struct {
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} nx_rtc_alarm_t;
/** \brief Caller-owned alarm, borrowed until acquire-observed SETTLED.
 * \note Initialize base before first preparation. Domain and pattern are copied
 *       by value. After ACCEPTED the complete request remains live and caller
 *       immutable through SETTLED and any separate control/observer joins.
 *       transferred counts occurrences: SUCCESS is 1, every other result is 0.
 */
typedef struct {
    nx_request_t base;
    nx_rtc_domain_t domain;
    nx_rtc_alarm_t alarm;
} nx_rtc_request_t;
/** \brief Shared immutable methods with the corresponding public contracts. */
typedef struct {
    nx_result_t (*read)(const void* context, nx_rtc_snapshot_t* snapshot);
    nx_result_t (*set)(void* context, const nx_rtc_calendar_t* calendar);
    nx_result_t (*submit)(void* context, nx_rtc_request_t* request);
    nx_result_t (*cancel)(void* context, nx_rtc_request_t* request);
    nx_result_t (*service)(void* context);
    nx_result_t (*stop)(void* context);
} nx_rtc_ops_t;
/** \brief Two-pointer identity; face, ops and context outlive loans/observers.
 */
typedef struct {
    const nx_rtc_ops_t* ops;
    void* context;
} nx_rtc_port_t;
/** \brief Validate raw calendar fields without changing any storage.
 * \param[in] calendar: Readable naturally aligned value.
 * \return SUCCESS or INVALID; no clock or provider access.
 */
nx_result_t nx_rtc_calendar_validate(const nx_rtc_calendar_t* calendar);
/** \brief Validate the finite alarm pattern's day and time ranges.
 * \param[in] alarm: Readable naturally aligned value.
 * \return SUCCESS or INVALID; not a promise that a provider can arm it.
 */
nx_result_t nx_rtc_alarm_validate(const nx_rtc_alarm_t* alarm);
/** \brief Copy a domain/pattern into initialized unborrowed caller storage.
 * \param[in,out] request: READY or consumed SETTLED storage, all joins
 * complete. \param[in] domain: Nonzero owner/generation copied during this
 * call. \param[in] alarm: Valid pattern copied during this call. \param[in]
 * deadline: Absolute Core monotonic deadline or NX_DEADLINE_NEVER. \return
 * SUCCESS or INVALID/STATE; every failure leaves request unchanged. \note
 * Inputs must be complete, aligned, nonwrapping spans disjoint from request.
 * This cold operation takes no loan and cannot know the complete extent of an
 * opaque provider or clock context. Admission checks the known provider spans;
 * assembly owns opaque-context disjointness.
 */
nx_result_t nx_rtc_request_prepare(nx_rtc_request_t* request,
                                   const nx_rtc_domain_t* domain,
                                   const nx_rtc_alarm_t* alarm,
                                   nx_time_us_t deadline);
/** \brief Copy a set calendar/domain on the single serialized task executor.
 * \param[in] port: Valid immutable live face; no concurrent lifecycle writer.
 * \param[out] snapshot: Separate aligned writable output.
 * \return SUCCESS or STATE for closed/unset, INVALID or UNSUPPORTED; failure
 * leaves output unchanged.
 */
nx_result_t nx_rtc_port_read(const nx_rtc_port_t* port,
                             nx_rtc_snapshot_t* snapshot);
/** \brief Set an idle calendar and advance its nonwrapping setting domain.
 * \param[in] port: Same single serialized privileged task owner.
 * \param[in] calendar: Separate raw value, copied on successful validation.
 * \return SUCCESS, BUSY for a loan, STATE if closed, INVALID, EXHAUSTED or
 * UNSUPPORTED. \note Rejection changes no state. No implicit backup reset,
 * synchronization, Core-time adjustment or authorization to overwrite retained
 * hardware.
 */
nx_result_t nx_rtc_port_set(const nx_rtc_port_t* port,
                            const nx_rtc_calendar_t* calendar);
/** \brief Borrow and arm one future alarm; no queue, allocation or worker.
 * \param[in] port: Initialized serialized owner with a valid copied Core clock.
 * \param[in,out] request: Exact READY request for the current setting domain.
 * \return SUCCESS means ACCEPTED; INVALID/STATE/BUSY/TIMEOUT/UNSUPPORTED
 * rejects without retained references or effects. \note
 * Closed/stale/already-matching pattern returns STATE; active is BUSY. An
 * observed expired finite deadline returns TIMEOUT before admission. Calendar,
 * counter and Core deadline are separate domains. Assembly keeps the copied
 * clock face's borrowed context live, disjoint and non-reentrant for the entire
 * owner lifetime and all external joins.
 */
nx_result_t nx_rtc_port_submit(const nx_rtc_port_t* port,
                               nx_rtc_request_t* request);
/** \brief Latch cancellation of the exact borrowed request, without revocation.
 * \param[in] port: Same serialized executor; cross-task controls need an
 * adapter. \param[in,out] request: Exact active identity; foreign identity is
 * not read. \return SUCCESS accepts cancellation; it may still be
 * QUARANTINED/borrowed. Rejection is STATE/INVALID/UNSUPPORTED. \note
 * Cancel/stop executed before service may win over unobserved expiry or event.
 * The first terminal cause never changes while withdrawal waits.
 */
nx_result_t nx_rtc_port_cancel(const nx_rtc_port_t* port,
                               nx_rtc_request_t* request);
/** \brief Observe deadline/event or continue actual source withdrawal.
 * \param[in] port: Same serialized task executor; bounded, no hidden wait.
 * \return SUCCESS when idle/settled, BUSY while active or withdrawal is held;
 * INVALID/STATE/UNSUPPORTED may reject dispatch. \note At first observation
 * expiry wins over pending event. Source/pending references detach before
 * release SETTLED, the final provider access.
 */
nx_result_t nx_rtc_port_service(const nx_rtc_port_t* port);
/** \brief Close admission and prove withdrawal before returning any loan.
 * \param[in] port: Same serialized owner with external publishers quiescent.
 * \return SUCCESS when closed/joined; BUSY retains the quarantined request.
 * INVALID/STATE/UNSUPPORTED may reject dispatch. \note Clean repeated stop is
 * idempotent. Stop does not reset identity or revoke pointers. Source/provider
 * lifetime still covers external joins.
 */
nx_result_t nx_rtc_port_stop(const nx_rtc_port_t* port);
#ifdef __cplusplus
}
#endif
#endif
