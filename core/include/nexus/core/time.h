/**
 * \file            time.h
 *
 * \brief           Absolute monotonic microsecond deadlines and wrap
 *                  extension.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_TIME_H
#define NEXUS_TIME_H

#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Microseconds in one boot/clock domain; no wall-clock
 *                  semantics.
 */
typedef uint64_t nx_time_us_t;
#define NX_DEADLINE_NEVER UINT64_MAX
/**
 * \brief           An explicitly injected clock; its context outlives all
 *                  borrowers.
 */
typedef struct {
    nx_time_us_t (*read)(void* context);
    void* context;
} nx_clock_t;
/**
 * \brief           State for extending one 32-bit free-running hardware
 *                  counter.
 */
typedef struct {
    uint64_t ticks;
    uint32_t previous;
    uint32_t frequency_hz;
    bool initialized;
} nx_clock32_t;
/**
 * \brief           Read the selected provider's monotonic clock.
 *
 * \return          Microseconds since domain initialization.
 *
 * \note            Physical providers require successful platform startup and
 *                  a privileged task/configurable IRQ; NMI/HardFault callers
 *                  are unsupported. No OS, allocation or wait. Physical clocks
 *                  return zero outside a successfully started live epoch; zero
 *                  alone cannot prove readiness. Startup status is
 * authoritative. All consumers join before clock withdrawal. Stop/restart or a
 * clock change invalidates every old deadline, even if a new epoch has the same
 * numerical value. UINT64_MAX is reserved; clock exhaustion requires controlled
 * restart. An observed hardware overflow acknowledgement failure ends the live
 * epoch and is terminal, even when finite source isolation succeeds. The
 * provider retains ownership, masks interrupts, calls nx_fault_observe and
 * parks if the observer returns. It never resumes deadline consumers with
 * invalid zero time. Observation neither releases loans nor proves safe
 * outputs; products own separately reviewed non-returning recovery. Ordinary
 * shutdown still requires every consumer and optional low-power owner joined
 * first.
 */
nx_time_us_t nx_time_now_us(void);
/**
 * \brief           Test an absolute deadline against a clock snapshot.
 *
 * \param[in]       deadline: Absolute time or NX_DEADLINE_NEVER.
 *
 * \param[in]       now: Snapshot in the same clock domain.
 *
 * \return          True when a finite deadline has expired, including
 *                  equality.
 */
bool nx_deadline_expired(nx_time_us_t deadline, nx_time_us_t now);
/**
 * \brief           Add a duration without wrapping the monotonic domain.
 *
 * \param[in]       now: Current finite time.
 *
 * \param[in]       duration_us: Relative duration in microseconds.
 *
 * \return          Absolute deadline, saturating at NX_DEADLINE_NEVER.
 */
nx_time_us_t nx_deadline_after(nx_time_us_t now, uint64_t duration_us);
/**
 * \brief           Construct a finite deadline without selecting infinite wait.
 *
 * \param[in]       now: Current finite time in the consumer's clock domain.
 *
 * \param[in]       duration_us: Relative duration in microseconds; zero is
 * valid.
 *
 * \param[out]      deadline: Finite result, unchanged on failure.
 *
 * \return          SUCCESS; INVALID for NULL output or nonfinite now; EXHAUSTED
 *                  when the sum reaches NX_DEADLINE_NEVER or would overflow.
 *
 * \note            Bounded task/IRQ arithmetic, without reads of a clock,
 *                  allocation or retained references. Clock readiness and
 *                  domain lifetime remain the caller's responsibility. Use
 *                  nx_deadline_after only when saturation to an infinite wait
 *                  is intentional.
 */
nx_result_t nx_deadline_finite_after(nx_time_us_t now, uint64_t duration_us,
                                     nx_time_us_t* deadline);
/**
 * \brief           Initialize counter extension and its clock domain.
 *
 * \param[out]      clock: Caller-owned extension state.
 *
 * \param[in]       counter: First hardware snapshot.
 *
 * \param[in]       frequency_hz: Nonzero stable counter frequency.
 *
 * \return          NX_SUCCESS, or NX_ERROR_INVALID for invalid arguments.
 *
 * \note            Single execution owner. Reset creates a new clock domain;
 *                  do not reuse outstanding deadlines after reset/clock
 *                  change.
 */
nx_result_t nx_clock32_initialize(nx_clock32_t* clock, uint32_t counter,
                                  uint32_t frequency_hz);
/**
 * \brief           Extend a wrapping counter and convert to microseconds.
 *
 * \param[in,out]   clock: Initialized, single-owner extension state.
 *
 * \param[in]       counter: Current counter snapshot.
 *
 * \param[out]      time_us: Distinct extended elapsed microseconds.
 *
 * \return          SUCCESS, INVALID for NULL/overlapping output, STATE for an
 *                  uninitialized clock, or EXHAUSTED. Failure changes neither
 *                  extension state nor output.
 *
 * \note            Observe strictly before 2^32 ticks elapse, even when idle.
 *                  Multiple missed wraps cannot be detected from two
 *                  snapshots, nor can one complete wrap at equal snapshots.
 *                  Counter must continue through waits; frequency cannot
 *                  change. No concurrent initialize/reset or observation.
 */
nx_result_t nx_clock32_observe(nx_clock32_t* clock, uint32_t counter,
                               nx_time_us_t* time_us);
#ifdef __cplusplus
}
#endif

#endif
