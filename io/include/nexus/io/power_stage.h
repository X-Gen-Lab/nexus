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
/** \brief Two independent active-vector observations in one DC-link cycle.
 * \note Vector bits 2/1/0 identify upper switches A/B/C. The selected provider
 *       proves these states match the adopted PWM plan. sample_ticks retain
 *       scheduled carrier references, not measured analog apertures. This
 *       record is callback-local; retain only complete independent copies.
 */
typedef struct {
    uint16_t code[2];
    uint32_t sample_ticks[2];
    uint8_t switching_vector[2];
    uint64_t sequence;
} nx_power_stage_dc_capture_t;
/** \brief Selected DC-link sampling descriptor, copied by the provider.
 * \note Two ordered observable ticks and independent active vectors describe
 *       one complete plan. The descriptor must be contained in the session's
 *       declared algorithm context; callback stack pointers are not allowed.
 *       Selected hardware rejects unobservable plans without inventing pulses.
 */
typedef struct {
    uint32_t sample_ticks[2];
    uint8_t switching_vector[2];
} nx_power_stage_dc_plan_t;
/** \brief Selected ADC observation; callback-local and read-only.
 * \note current_raw uses logical A/B/C slots. current_phase_mask marks actual
 *       measured slots; zero-filled unselected slots are not measurements.
 *       current_count is two or three for phase sampling. A DC-link profile
 *       instead has count/mask zero and a non-NULL dc record. Phase sampling
 *       always has dc=NULL. bus_raw and encoder_count may only be
 *       consumed when their corresponding valid flag is true. Raw scaling and
 *       source qualification remain caller responsibilities. plan_sequence
 *       identifies the complete previously staged plan that caused this scan.
 *       sample_at is the scheduled trigger reference, not a measured aperture;
 *       trigger_ticks retains the finer carrier coordinate. scan_span_us bounds
 *       the complete observation span, including both trigger references for
 *       a DC-link pair and their final conversion. planned_apply_at is an
 * intended boundary, never a physical-output receipt. Times use the same Core
 * epoch.
 */
typedef struct {
    uint16_t current_raw[3];
    uint16_t bus_raw;
    uint8_t current_count;
    uint8_t current_phase_mask;
    bool bus_valid;
    bool encoder_valid;
    uint64_t plan_sequence;
    uint32_t encoder_count;
    uint64_t sequence;
    uint64_t clock_epoch;
    nx_time_us_t sample_at;
    nx_time_us_t observed_at;
    nx_time_us_t angle_at;
    nx_time_us_t planned_apply_at;
    uint32_t scan_span_us;
    uint32_t trigger_ticks;
    const nx_power_stage_dc_capture_t* dc;
} nx_power_stage_sample_t;
/** \brief Complete output and observation plan in full-carrier tick units.
 * \note duty_ticks is each high pulse's total width; start_ticks is its
 *       modulo-carrier beginning. Selected providers reject unsupported pulse
 *       shapes rather than ignoring starts. trigger_ticks and phase mask belong
 *       to the same plan. Initial sequence is zero; a callback returns its
 *       sample sequence plus one. dc is NULL for phase-current profiles; a
 *       DC-link profile requires a complete context-contained descriptor.
 *       Successful staging is not physical adoption.
 */
typedef struct {
    uint32_t duty_ticks[3];
    uint32_t start_ticks[3];
    uint32_t trigger_ticks;
    uint8_t current_phase_mask;
    uint64_t sequence;
    const nx_power_stage_dc_plan_t* dc;
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
/** \brief Selected timing/limits and ordinary-context observations.
 * \note Every *_ticks field uses the selected full-carrier tick unit. The
 *       aperture/conversion bounds are conservative configured scan bounds,
 *       never measured analog or WCET qualification. Current count/mask and
 *       optional-source availability describe the cold-selected capabilities;
 *       each callback separately identifies its actual captured phases.
 */
typedef struct {
    uint64_t clock_epoch;
    uint32_t tick_hz;
    uint32_t period_ticks;
    uint32_t trigger_ticks;
    uint32_t minimum_duty_ticks;
    uint32_t maximum_duty_ticks;
    uint32_t scan_span_us;
    uint32_t capture_begin_ticks;
    uint32_t capture_end_ticks;
    uint32_t deadtime_ticks;
    uint32_t settling_ticks;
    uint32_t aperture_ticks;
    uint32_t conversion_ticks;
    uint32_t irq_ticks;
    uint32_t compute_ticks;
    uint32_t rearm_ticks;
    uint32_t commit_before_ticks;
    uint64_t completed_cycles;
    uint8_t current_count;
    uint8_t current_phase_mask;
    bool bus_available;
    bool encoder_available;
    bool center_aligned;
    bool dc_link;
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
