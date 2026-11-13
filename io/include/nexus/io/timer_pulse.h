/**
 * \file            timer_pulse.h
 * \brief           Finite hardware one-pulse output with explicit final access
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_TIMER_PULSE_H
#define NEXUS_IO_TIMER_PULSE_H
#include "nexus/core/request.h"
#include "nexus/io/fwd.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief One complete pulse period on the statically ordered channels.
 * \note Initialize/prepare base before submit. Each width is in the selected
 *       post-prescaler ticks, from zero through period_ticks minus one. The
 *       remaining leading ticks are inactive; all pulses finish at the same
 *       hardware one-pulse boundary. Zero width remains inactive throughout.
 *       A full-period active level has no inactive one-pulse end and rejects.
 *       ACCEPTED borrows this request and the complete immutable width array
 *       through acquire-observed SETTLED. Error/cancel reports zero completed
 *       periods. SUCCESS reports one period, not a measured electrical edge.
 */
typedef struct {
    nx_request_t base;
    const uint32_t* width_ticks;
    size_t channel_count;
} nx_timer_pulse_request_t;

/** \brief Nominal units and actual source/recovery observations. */
typedef struct {
    uint32_t tick_hz;
    uint32_t period_ticks;
    size_t channel_count;
    bool armed;
    bool running;
    bool fault;
    bool quarantined;
} nx_timer_pulse_state_t;

/** \brief One static owner, driven by a serialized privileged unmasked task.
 * \note Platform prepare leaves counter and outputs off. Factory returns the
 *       existing face. submit accepts a READY request before enabling any
 *       source; rejection retains no references or new hardware effects.
 *       Hardware stops the counter after the one-pulse update. Explicit
 *       service still joins outputs and actual IRQ observers before SETTLED.
 *       If an internal master is selected, arm the slave before starting that
 *       master and supply only one start event until settlement. There is no
 *       worker, task or implicit clock owner. A completion first observed at
 *       or after the absolute deadline reports TIMEOUT. Failed stop, clock
 *       history loss and quarantine never revoke a borrow. Platform close
 *       withdraws admission and must complete the same join before System
 *       resources can be released. state output must be disjoint from every
 *       accepted control and width span; it remains unchanged on rejection.
 */
typedef struct {
    nx_result_t (*submit)(void* context, nx_timer_pulse_request_t* request);
    nx_result_t (*cancel)(void* context, nx_timer_pulse_request_t* request);
    nx_result_t (*service)(void* context);
    nx_result_t (*stop)(void* context);
    nx_result_t (*state)(const void* context, nx_timer_pulse_state_t* state);
} nx_timer_pulse_ops_t;

/** \brief Immutable identity with private retained controller storage. */
struct nx_timer_pulse_port {
    const nx_timer_pulse_ops_t* ops;
    void* context;
};
/** \brief The same typed face when operations are invoked directly. */
typedef nx_timer_pulse_port_t nx_timer_pulse_t;
/** \brief Admit one finite pulse without changing the selected waveform shape.
 * \param[in] port: Stable face under the selected serialized execution
 * contract. \param[in,out] request: Original stable storage under the operation
 * loan contract. \return Provider result, INVALID for malformed public objects,
 * or UNSUPPORTED when this selected operation is absent. \note The selected
 * owner defines context and timing. Dispatch retains no references, starts no
 * worker and never revokes a live borrow. Provider admission still checks
 * private state and payload aliases. The original argument reaches that owner
 * without a temporary or format conversion.
 */
nx_result_t nx_timer_pulse_port_submit(const nx_timer_pulse_port_t* port,
                                       nx_timer_pulse_request_t* request);
/** \brief Request pulse withdrawal while retaining the original request loan.
 * \param[in] port: Stable face under the selected serialized execution
 * contract. \param[in,out] request: Original stable storage under the operation
 * loan contract. \return Provider result, INVALID for malformed public objects,
 * or UNSUPPORTED when this selected operation is absent. \note The selected
 * owner defines context and timing. Dispatch retains no references, starts no
 * worker and never revokes a live borrow. Provider admission still checks
 * private state and payload aliases. The original argument reaches that owner
 * without a temporary or format conversion.
 */
nx_result_t nx_timer_pulse_port_cancel(const nx_timer_pulse_port_t* port,
                                       nx_timer_pulse_request_t* request);
/** \brief Advance bounded completion or withdrawal; SUCCESS alone is not
 * settlement. \param[in] port: Stable face under the selected serialized
 * execution contract. \return Provider result, INVALID for malformed public
 * objects, or UNSUPPORTED when this selected operation is absent. \note The
 * selected owner defines context and timing. Dispatch retains no references,
 * starts no worker and never revokes a live borrow. Provider admission still
 * checks private state and payload aliases. The original argument reaches that
 * owner without a temporary or format conversion.
 */
nx_result_t nx_timer_pulse_port_service(const nx_timer_pulse_port_t* port);
/** \brief Join the finite pulse before reporting that its source has stopped.
 * \param[in] port: Stable face under the selected serialized execution
 * contract. \return Provider result, INVALID for malformed public objects, or
 *         UNSUPPORTED when this selected operation is absent.
 * \note The selected owner defines context and timing. Dispatch retains no
 *       references, starts no worker and never revokes a live borrow. Provider
 *       admission still checks private state and payload aliases. The original
 *       argument reaches that owner without a temporary or format conversion.
 */
nx_result_t nx_timer_pulse_port_stop(const nx_timer_pulse_port_t* port);
/** \brief Copy selected units and current source observations.
 * \param[in] port: Stable face under the selected serialized execution
 * contract. \param[out] state: Independent output, changed only as the provider
 * contract permits. \return Provider result, INVALID for malformed public
 * objects, or UNSUPPORTED when this selected operation is absent. \note The
 * selected owner defines context and timing. Dispatch retains no references,
 * starts no worker and never revokes a live borrow. Provider admission still
 * checks private state and payload aliases. The original argument reaches that
 * owner without a temporary or format conversion.
 */
nx_result_t nx_timer_pulse_port_state(const nx_timer_pulse_port_t* port,
                                      nx_timer_pulse_state_t* state);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_IO_TIMER_PULSE_H */
