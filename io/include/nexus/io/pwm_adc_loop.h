/**
 * \file            pwm_adc_loop.h
 * \brief           Selected bounded IRQ control over coherent PWM/ADC hardware
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_PWM_ADC_LOOP_H
#define NEXUS_IO_PWM_ADC_LOOP_H

#include "nexus/core/request.h"
#include "nexus/io/adc_format.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Compute one complete duty group from one selected raw scan.
 * \param[in,out] context: Caller state exclusively loaned with the session.
 * \param[in] samples: Complete raw scan, readable only during this callback.
 * \param[in] sample_count: Exact selected rank count, fixed for the session.
 * \param[in] sequence: Starts at zero; exhaustion faults instead of wrapping.
 * \param[out] duty_ticks: Complete output group, writable only in callback.
 * \param[in] duty_count: Exact selected output count and fixed channel order.
 * \return SUCCESS to stage the complete group; any error latches a fault.
 * \note Runs in the selected privileged ADC IRQ, with a declared hard budget.
 *       No pointer retention, blocking, task-only I/O, Flash, OS API, heap or
 *       concurrent access to borrowed context/storage is permitted. The
 *       callback must return within its budget; software cannot preempt it.
 *       Samples are raw codes, not calibrated physical units or timestamps.
 */
typedef nx_result_t (*nx_pwm_adc_loop_compute_t)(
    void* context, const uint16_t* samples, size_t sample_count,
    uint64_t sequence, uint32_t* duty_ticks, size_t duty_count);

/** \brief One finite-lifetime continuous control session.
 * \note Initialize base before prepare. After ACCEPTED, session, callback,
 *       context, samples and duties remain exclusively provider-borrowed until
 *       acquire-observed SETTLED. The initial duty group is caller-authored;
 *       subsequent groups are callback-authored. A timeout/cancel cannot
 *       revoke memory, code or callback-context borrowing. Context may refer to
 *       a separately owned bounded queue/atomic mailbox whose own transfer
 *       contract permits cross-context commands. The borrowed context body and
 *       live controller parameters cannot be concurrently changed by tasks.
 *       On settlement, base.transferred is the accepted commit count, saturated
 *       at SIZE_MAX. It does not count physical PWM periods or sampled pulses.
 */
typedef struct {
    nx_request_t base;
    uint16_t* samples;
    size_t sample_count;
    uint32_t* duty_ticks;
    size_t duty_count;
    nx_pwm_adc_loop_compute_t compute;
    void* context;
} nx_pwm_adc_loop_session_t;

/** \brief Selected static capabilities and ordinary-context observations.
 * \note Counters describe accepted IRQ cycles, not qualified physical pulses
 *       or aperture timestamps. Callback/data memory remains borrowed even if
 *       hardware outputs have already been disabled.
 */
typedef struct {
    nx_adc_format_t format;
    uint32_t tick_hz;
    uint32_t period_ticks;
    uint32_t trigger_ticks;
    uint32_t commit_before_ticks;
    uint32_t compute_budget_ticks;
    uint64_t completed_cycles;
    size_t duty_count;
    bool running;
    bool fault;
    bool quarantined;
} nx_pwm_adc_loop_state_t;

/** \brief Selected-only methods; ordinary PWM and finite tables stay small. */
typedef struct {
    nx_result_t (*start)(void* context, nx_pwm_adc_loop_session_t* session);
    nx_result_t (*service)(void* context);
    nx_result_t (*stop)(void* context);
    nx_result_t (*state)(const void* context, nx_pwm_adc_loop_state_t* state);
} nx_pwm_adc_loop_ops_t;

/** \brief Immutable face; no allocator, worker or device discovery. */
typedef struct {
    const nx_pwm_adc_loop_ops_t* ops;
    void* context;
} nx_pwm_adc_loop_t;

/** \brief Prepare disjoint caller storage and a finite session deadline.
 * \param[in,out] session: READY or consumed SETTLED initialized session.
 * \param[out] samples: Aligned raw-scan buffer, exact selected rank count.
 * \param[in] sample_count: Positive number of uint16_t containers.
 * \param[in,out] duty_ticks: Aligned complete initial duty group.
 * \param[in] duty_count: Positive number of uint32_t counter-tick values.
 * \param[in] compute: Stable IRQ callback, retained after admission.
 * \param[in,out] context: Non-NULL callback state, retained after admission.
 * \param[in] deadline: Finite absolute deadline in the selected Core epoch.
 * \return SUCCESS or rejection, with no hardware access or retained borrow.
 * \note Thread only. Context's starting byte must be representable and outside
 *       session/samples/duties; its full opaque extent and lifetime remain the
 *       caller's responsibility. Provider validates count, range and placement.
 */
nx_result_t nx_pwm_adc_loop_prepare(nx_pwm_adc_loop_session_t* session,
                                    uint16_t* samples, size_t sample_count,
                                    uint32_t* duty_ticks, size_t duty_count,
                                    nx_pwm_adc_loop_compute_t compute,
                                    void* context, nx_time_us_t deadline);
/** \brief Admit one session; SUCCESS means borrowing, not physical actuation.
 * \param[in] port: Stable selected coupled owner and immutable face.
 * \param[in,out] session: Exact prepared session, retained until SETTLED.
 * \return SUCCESS means ACCEPTED; rejection retains no references/effects.
 * \note Serialized privileged unmasked Thread. Warmup/service precede IRQ
 *       control. A finite deadline bounds admission/progress, not callback
 *       WCET.
 */
nx_result_t nx_pwm_adc_loop_start(const nx_pwm_adc_loop_t* port,
                                  nx_pwm_adc_loop_session_t* session);
/** \brief Advance warmup, failure withdrawal and final session publication.
 * \param[in] port: Same live owner, serialized privileged unmasked Thread.
 * \return SUCCESS, BUSY or retained error; inspect request for SETTLED.
 * \note Required for timeout/cleanup even when callbacks progress in IRQ.
 *       Never refreshes the admitted deadline or automatically restarts output.
 */
nx_result_t nx_pwm_adc_loop_service(const nx_pwm_adc_loop_t* port);
/** \brief Close session admission, withdraw outputs and join IRQ callbacks.
 * \param[in] port: Same stable owner, serialized privileged unmasked Thread.
 * \return SUCCESS proves stopped/joined; failure retains all borrowed state.
 * \note No automatic restart. Cold Platform close separately releases leases.
 */
nx_result_t nx_pwm_adc_loop_stop(const nx_pwm_adc_loop_t* port);
/** \brief Copy selected facts and observations without changing ownership.
 * \param[in] port: Same live owner, serialized privileged unmasked Thread.
 * \param[out] state: Distinct aligned writable observation.
 * \return SUCCESS or rejection; failure leaves state unchanged.
 */
nx_result_t nx_pwm_adc_loop_state(const nx_pwm_adc_loop_t* port,
                                  nx_pwm_adc_loop_state_t* state);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_IO_PWM_ADC_LOOP_H */
