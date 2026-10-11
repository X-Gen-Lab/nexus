/**
 * \file            i2c_bus_clear.h
 *
 * \brief           Explicit open-drain I2C bus clear with retained leases.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_I2C_BUS_CLEAR_H
#define NEXUS_I2C_BUS_CLEAR_H

#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Maximum completed recovery low/high periods per attempt. */
#define NX_I2C_BUS_CLEAR_MAX_PULSES 9U

/**
 * \brief           Read-only methods for an authorized two-pin GPIO handoff.
 *
 * \note            Task-only, one executor. acquire is a bounded, nonblocking
 *                  handoff: success proves the controller, DMA, IRQs, other
 *                  masters and pin users quiescent, both physical pins
 *                  exclusively leased in reviewed open-drain GPIO mode, with
 *                  both lines released.
 *                  Failure retains no lease or effects. drive only sinks LOW
 *                  or releases a pin; false never means drive HIGH. When
 *                  scl_low is true, drive establishes SCL LOW before changing
 *                  SDA, including a retry from unknown previous pin state.
 *                  sample reads physical levels, not output latches. Methods
 *                  are bounded and preserve the lease on errors. release
 *                  restores the saved peripheral/pin ownership only on
 *                  success; failure retains all lease/storage responsibility.
 *                  wait_until uses the injected clock's finite absolute domain
 *                  and reaches target or reports an error within a caller-
 *                  proved finite bound, even if the clock stops. Late waits
 *                  cannot qualify an expired operation. No callback may
 *                  recursively operate on this session or run a transaction.
 *                  The adapter must verify reviewed open-drain, pull-up,
 *                  voltage and two-pin authorization facts before acquire.
 */
typedef struct {
    nx_result_t (*acquire)(void* context);
    nx_result_t (*drive)(void* context, bool scl_low, bool sda_low);
    nx_result_t (*sample)(void* context, bool* scl_high, bool* sda_high);
    nx_result_t (*wait_until)(void* context, nx_time_us_t target);
    nx_result_t (*release)(void* context);
} nx_i2c_bus_clear_ops_t;

/** \brief Explicit immutable GPIO methods and caller-owned adapter state. */
typedef struct {
    const nx_i2c_bus_clear_ops_t* ops;
    void* context;
} nx_i2c_bus_clear_port_t;

/**
 * \brief           Caller-selected electrical timings in microseconds.
 *
 * \note            All fields are nonzero. low/high bound each clear pulse;
 *                  stop_setup bounds the observed SCL-high to SDA-release
 *                  interval. stretch_poll bounds the interval between actual
 *                  SCL samples. Values need board/device electrical review;
 *                  the mechanism supplies no universal safe defaults.
 */
typedef struct {
    uint32_t low_us;
    uint32_t high_us;
    uint32_t stop_setup_us;
    uint32_t stretch_poll_us;
} nx_i2c_bus_clear_timing_t;

/**
 * \brief           Independent recovery and STOP/ownership-return results.
 *
 * \note            pulses counts completed recovery low/high periods, not
 *                  the STOP preparation edge. stop_observed proves the
 *                  requested STOP sequence ended with both sampled lines
 *                  HIGH. lease_held means the adapter, session and context
 *                  must remain alive and no controller transaction may start.
 *                  A cleanup failure never replaces the primary failure.
 */
typedef struct {
    nx_result_t primary;
    nx_result_t cleanup;
    uint8_t pulses;
    bool stop_observed;
    bool lease_held;
} nx_i2c_bus_clear_result_t;

/**
 * \brief           Caller-owned, zero-initialized single-executor session.
 *
 * \note            Public fields are mechanism storage, not caller controls.
 *                  Methods, context and clock outlive every retained lease.
 *                  No allocation, worker, lock, transaction buffer or hidden
 *                  retry exists. Abandoning a retained lease requires explicit
 *                  product quarantine/reset responsibility.
 */
typedef struct {
    nx_i2c_bus_clear_port_t port;
    nx_clock_t clock;
    nx_i2c_bus_clear_timing_t timing;
    nx_i2c_bus_clear_result_t result;
    nx_time_us_t last_time;
    bool clock_observed;
    bool initialized;
} nx_i2c_bus_clear_t;

/**
 * \brief           Bind explicit electrical operations without touching pins.
 *
 * \param[in,out]   clear: Fresh zeroed session or a session with no lease.
 *
 * \param[in]       port: All five methods required; caller verifies electrical
 *                  authorization rather than relying on generic GPIO APIs.
 *
 * \param[in]       clock: Required bounded monotonic read in one boot domain.
 *
 * \param[in]       timing: Reviewed nonzero low/high/STOP/poll durations.
 *
 * \return          SUCCESS, INVALID or BUSY for a retained lease. Rejection
 *                  leaves the session and external resources unchanged.
 */
nx_result_t nx_i2c_bus_clear_init(nx_i2c_bus_clear_t* clear,
                                  nx_i2c_bus_clear_port_t port,
                                  nx_clock_t clock,
                                  nx_i2c_bus_clear_timing_t timing);

/**
 * \brief           Clear one idle, authorized bus with at most nine pulses.
 *
 * \param[in,out]   clear: Initialized session with no retained lease.
 *
 * \param[in]       deadline: Finite absolute primary-operation deadline.
 *
 * \param[in]       cleanup_deadline: Finite absolute STOP/lease deadline in
 *                  the same clock domain, at or after deadline.
 *
 * \param[out]      result: Independent primary/cleanup and retained effects.
 *
 * \return          Primary failure, otherwise cleanup result. Invalid inputs
 *                  and an existing lease reject without callbacks or changing
 *                  result. An expired primary deadline has no pin effects.
 *                  Each released SCL must be physically HIGH before timing
 *                  its high period. A frozen/reversed clock reports IO and
 *                  exits rather than spinning. Every acquired attempt performs
 *                  explicit STOP cleanup under its independent budget; failed
 *                  STOP or release method retains the lease. If release
 *                  succeeds after its deadline, cleanup reports TIMEOUT with
 *                  lease_held false to preserve actual returned ownership.
 *                  It never replays the failed I2C transaction or retries
 *                  recovery pulses.
 *
 * \note            Task-only, all other bus users quiescent. Timing evidence
 *                  and actual open-drain behavior require physical HIL.
 */
nx_result_t nx_i2c_bus_clear_run(nx_i2c_bus_clear_t* clear,
                                 nx_time_us_t deadline,
                                 nx_time_us_t cleanup_deadline,
                                 nx_i2c_bus_clear_result_t* result);

/**
 * \brief           Retry only retained STOP cleanup and ownership return.
 *
 * \param[in,out]   clear: Initialized session with a retained lease.
 *
 * \param[in]       deadline: New finite absolute cleanup deadline.
 *
 * \param[out]      result: Preserved primary/pulse count and latest cleanup.
 *
 * \return          Latest cleanup result; INVALID/STATE rejects without
 *                  effects. A proved STOP is not repeated after release fails.
 *                  Failure retains the lease and must not trigger a new clear
 *                  or transaction. Success does not change a failed primary
 *                  operation into success.
 */
nx_result_t nx_i2c_bus_clear_finish(nx_i2c_bus_clear_t* clear,
                                    nx_time_us_t deadline,
                                    nx_i2c_bus_clear_result_t* result);

#ifdef __cplusplus
}
#endif
#endif
