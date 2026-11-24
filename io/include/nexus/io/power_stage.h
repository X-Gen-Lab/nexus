/**
 * \file            power_stage.h
 * \brief           Explicit coherent electric-drive sampling and output session
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_POWER_STAGE_H
#define NEXUS_IO_POWER_STAGE_H
#include "nexus/core/request.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief One complete raw drive observation; callback-local and read-only.
 * \note sample_at is a scheduled trigger reference, not a measured aperture.
 *       The four ADC ranks are sequential; scan_span_us bounds their declared
 *       conversion span. observed_at and angle_at are software observations in
 *       the same Core epoch. planned_apply_at is the next intended PWM update,
 *       never a physical-output receipt. Raw current order is A/B/C; bus_raw is
 *       the fourth rank. The caller owns calibration and physical units.
 */
typedef struct {
    uint16_t current_raw[3];
    uint16_t bus_raw;
    uint32_t encoder_count;
    uint64_t sequence;
    uint64_t clock_epoch;
    nx_time_us_t sample_at;
    nx_time_us_t observed_at;
    nx_time_us_t angle_at;
    nx_time_us_t planned_apply_at;
    uint32_t scan_span_us;
} nx_power_stage_sample_t;
/** \brief Complete next shadow group in the cold-selected timer tick unit. */
typedef struct {
    uint32_t duty_ticks[3];
} nx_power_stage_plan_t;
/** \brief Compute one bounded complete group in the selected ADC IRQ.
 * \param[in,out] context: Exclusively borrowed caller algorithm state.
 * \param[in] sample: Callback-local raw values and explicitly sourced times.
 * \param[out] plan: Callback-local complete output, valid only on SUCCESS.
 * \return SUCCESS or exact fault that inhibits output and closes admission.
 * \note No pointer retention, blocking, heap, task-only I/O, OS, Flash or
 *       network calls. The caller proves WCET; software cannot preempt a
 *       nonreturning callback. Separate synchronized objects may transfer
 *       commands, but tasks cannot mutate this borrowed context concurrently.
 */
typedef nx_result_t (*nx_power_stage_compute_t)(
    void* context, const nx_power_stage_sample_t* sample,
    nx_power_stage_plan_t* plan);
/** \brief Caller-owned long-lived drive session with finite startup budget.
 * \note Initialize base before prepare. base.deadline is NEVER because the
 *       nonblocking session runs until explicit stop or fault. startup_deadline
 *       separately bounds warmup. Command validity
 *       is separate and belongs to compute/Axis. ACCEPTED borrows this entire
 *       session and context_bytes until acquire-observed SETTLED. Stop failure
 *       retains QUARANTINED; output inhibition alone never releases the loan.
 */
typedef struct {
    nx_request_t base;
    nx_time_us_t startup_deadline;
    nx_power_stage_plan_t initial;
    nx_power_stage_compute_t compute;
    void* context;
    size_t context_bytes;
} nx_power_stage_session_t;
/** \brief Selected timing/limits and ordinary-context observations. */
typedef struct {
    uint32_t tick_hz;
    uint32_t period_ticks;
    uint32_t trigger_ticks;
    uint32_t minimum_duty_ticks;
    uint32_t maximum_duty_ticks;
    uint32_t scan_span_us;
    uint64_t completed_cycles;
    nx_result_t fault;
    bool running;
    bool quarantined;
} nx_power_stage_state_t;
/** \brief Shared immutable methods; the face never allocates or discovers. */
typedef struct {
    nx_result_t (*start)(void* context, nx_power_stage_session_t* session);
    nx_result_t (*service)(void* context);
    nx_result_t (*stop)(void* context);
    nx_result_t (*state)(const void* context, nx_power_stage_state_t* state);
} nx_power_stage_ops_t;
/** \brief Existing static identity returned by the typed factory. */
typedef struct {
    const nx_power_stage_ops_t* ops;
    void* context;
} nx_power_stage_t;
/** \brief Prepare disjoint unborrowed caller storage without hardware access.
 * \param[in,out] session: Initialized READY or consumed SETTLED session.
 * \param[in] initial: Independent complete initial group, copied on success.
 * \param[in] compute: Stable bounded callback retained through settlement.
 * \param[in,out] context: Full aligned caller-owned algorithm context.
 * \param[in] context_bytes: Representable nonzero complete context extent.
 * \param[in] startup_deadline: Finite Core deadline for warmup and start.
 * \return SUCCESS, INVALID or STATE; rejection leaves inputs unchanged.
 * \note Serialized Task. Join all prior users before preparing again.
 */
nx_result_t nx_power_stage_prepare(nx_power_stage_session_t* session,
                                   const nx_power_stage_plan_t* initial,
                                   nx_power_stage_compute_t compute,
                                   void* context, size_t context_bytes,
                                   nx_time_us_t startup_deadline);
/** \brief Admit one drive session without promising physical actuation.
 * \param[in] port: Stable existing selected provider face.
 * \param[in,out] session: Prepared caller state, retained when ACCEPTED.
 * \return SUCCESS means ACCEPTED; rejection retains no references/effects.
 * \note Serialized privileged unmasked Task; service advances warmup.
 */
nx_result_t nx_power_stage_start(const nx_power_stage_t* port,
                                 nx_power_stage_session_t* session);
/** \brief Advance bounded startup/withdrawal, without creating a worker.
 * \param[in] port: Same stable single-executor face.
 * \return SUCCESS, BUSY or exact retained fault; inspect base for SETTLED.
 * \note Task only. Still required for final cleanup after an IRQ fault.
 */
nx_result_t nx_power_stage_service(const nx_power_stage_t* port);
/** \brief Inhibit output and prove source/callback withdrawal in one step.
 * \param[in] port: Same stable single-executor face.
 * \return SUCCESS only after withdrawal; failure retains caller storage.
 * \note Task only. This is hardware stop, not a mechanical deceleration or
 *       power-isolation promise. Application/Axis owns controlled stopping.
 */
nx_result_t nx_power_stage_stop(const nx_power_stage_t* port);
/** \brief Copy selected facts without changing source or borrow ownership.
 * \param[in] port: Same live face.
 * \param[out] state: Separate aligned output, unchanged on rejection.
 * \return SUCCESS or exact error.
 * \note Task only. No result proves physical timing, torque or safe machinery.
 */
nx_result_t nx_power_stage_state(const nx_power_stage_t* port,
                                 nx_power_stage_state_t* state);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_IO_POWER_STAGE_H */
