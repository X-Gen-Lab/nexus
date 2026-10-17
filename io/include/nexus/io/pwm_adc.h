/**
 * \file            pwm_adc.h
 * \brief           Optional finite coherent PWM / triggered ADC window
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_PWM_ADC_H
#define NEXUS_IO_PWM_ADC_H
#include "nexus/core/request.h"
#include "nexus/io/adc_format.h"
#include "nexus/io/pwm_group.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief One finite window; transferred counts are raw samples, never bytes.
 * \note Initialize base before first preparation. After successful admission,
 *       the request and duty array remain live and caller-immutable through
 *       acquire-observed SETTLED. The writable sample span belongs exclusively
 *       to the provider; the caller neither reads nor writes it while borrowed.
 *       Frame metadata is copied at preparation. No later shadow commit,
 *       automatic rearm, control algorithm or engineering-unit conversion is
 *       part of this finite-window interface. Hardware-specific sample storage
 *       and integer encoding are reported by state.format.
 */
typedef struct {
    nx_request_t base;          /**< Borrow and release publication state. */
    uint16_t* samples;          /**< Exclusive writable raw sample storage. */
    size_t sample_count;        /**< Positive number of requested samples. */
    nx_pwm_group_frame_t frame; /**< Copied metadata; duty array is borrowed. */
} nx_pwm_adc_request_t;
/** \brief Exact selected facts and observed source/recovery state.
 * \note Format describes actual supported hardware. It does not establish
 *       aperture timestamps, analog accuracy, PWM duty epochs, IRQ latency,
 *       the number of output pulses or an inspected physical circuit.
 */
typedef struct {
    nx_adc_format_t format;  /**< Actual integer storage and provenance. */
    uint32_t tick_hz;        /**< PWM counter ticks per second. */
    uint32_t period_ticks;   /**< Fixed period in counter ticks. */
    uint32_t trigger_ticks;  /**< Fixed phase in counter ticks. */
    uint32_t deadtime_ticks; /**< Deadtime in separate deadtime_hz units. */
    uint32_t deadtime_hz;    /**< Deadtime clock; never inferred from PSC. */
    size_t pwm_channels;     /**< Complete fixed-order frame width. */
    size_t max_samples;      /**< Provider/DMA finite-window count bound. */
    bool running;            /**< Observed source/counter activity. */
    bool fault;              /**< Latched fault requiring cold recovery. */
    bool quarantined;        /**< Incomplete withdrawal retains ownership. */
} nx_pwm_adc_state_t;
/** \brief Selected-only operations; no growth to ordinary PWM/ADC tables.
 * \note Each callback implements its corresponding public function contract.
 *       One privileged task serializes submit/cancel/service/stop/state; IRQs
 *       only latch bounded physical facts into the same private owner. Missing
 *       callbacks return UNSUPPORTED. There is no allocating or hidden worker.
 */
typedef struct {
    nx_result_t (*submit)(void* context, nx_pwm_adc_request_t* request);
    nx_result_t (*cancel)(void* context, nx_pwm_adc_request_t* request);
    nx_result_t (*service)(void* context);
    nx_result_t (*stop)(void* context);
    nx_result_t (*state)(const void* context, nx_pwm_adc_state_t* state);
} nx_pwm_adc_ops_t;
/** \brief Immutable cold-selected identity, never an allocating factory.
 * \note Face, operations, context and cold configuration outlive every request
 *       and IRQ publisher. One coupled face owns all selected hardware; no
 *       independently startable PWM-group or ADC face exposes that hardware.
 */
typedef struct {
    const nx_pwm_adc_ops_t* ops;
    void* context;
} nx_pwm_adc_t;
/** \brief Prepare distinct unborrowed request, frame and sample storage.
 * \param[in,out] request: Initialized READY or fully consumed SETTLED storage.
 * \param[out] samples: Naturally aligned writable caller sample array.
 * \param[in] sample_count: Positive exact sample count; provider checks DMA.
 * \param[in] frame: Readable descriptor copied by value during this call only;
 *                  its complete duty array remains live through SETTLED.
 * \param[in] deadline: Finite absolute monotonic deadline for the whole window.
 * \return SUCCESS or INVALID/STATE; no hardware access or provider borrow.
 * \note Request, sample span, source frame descriptor and duty span must be
 *       naturally aligned, nonwrapping and disjoint. The caller has consumed
 *       prior results and joined every adapter observer before preparing again.
 *       NX_DEADLINE_NEVER is rejected; an expired finite deadline can be
 *       prepared but cannot admit new physical effects. Preparation does not
 *       access sample contents, borrow hardware or establish rate/region
 *       support. Failure leaves the initialized request unchanged.
 */
nx_result_t nx_pwm_adc_prepare(nx_pwm_adc_request_t* request, uint16_t* samples,
                               size_t sample_count,
                               const nx_pwm_group_frame_t* frame,
                               nx_time_us_t deadline);
/** \brief Admit and arm one complete finite window on the unique task owner.
 * \param[in] port: Initialized stable selected coupled face and cold leases.
 * \param[in,out] request: Exact READY window prepared by the caller.
 * \return SUCCESS means ACCEPTED, including a possibly already SETTLED error.
 *         Rejection means no retained pointer, payload access or new effect.
 * \note Post-admission arm/prime failures drain under the accepted request.
 *       Normal completion stops the PWM sources before SETTLED and never
 *       rearms. DMA TC alone is insufficient for completion or safe output.
 *       An error/cancellation reports zero guaranteed valid samples; an aborted
 *       DMA remaining count cannot establish a valid prefix. A successful
 *       window reports exactly sample_count. After successful ordinary
 *       completion, another explicitly prepared submission may begin; an
 *       explicit stop or latched fault requires cold close/reinitialization.
 *       Provider checks context, fixed frame, region and overlap with all cold
 *       descriptors/MMIO before admission. The complete duty frame is constant
 *       for the window; sample count does not promise an exact PWM pulse count.
 *       Without a trusted completion timestamp, completion first observed at
 *       or after the request deadline reports TIMEOUT, even if DMA is already
 *       complete. This conservative observation rule promises no physical
 *       execution-time bound and never relaxes the required source drain.
 */
nx_result_t nx_pwm_adc_submit(const nx_pwm_adc_t* port,
                              nx_pwm_adc_request_t* request);
/** \brief Latch cancellation while retaining the exact live window loan.
 * \param[in] port: Same live coupled face and serialized privileged task.
 * \param[in,out] request: Exact admitted request, still live until SETTLED.
 * \return SUCCESS starts/observes cancellation, never implies SETTLED.
 * \note Cancellation is task-local and does not provide a cross-task stable
 *       identity. Use the established adapter slot/epoch mechanism externally.
 *       A prior terminal fault is retained; cancellation never clears it.
 */
nx_result_t nx_pwm_adc_cancel(const nx_pwm_adc_t* port,
                              nx_pwm_adc_request_t* request);
/** \brief Advance bounded completion or source drain on the same task owner.
 * \param[in] port: Same live coupled face and serialized privileged task.
 * \return SUCCESS after progress/settlement, BUSY while drain remains, or a
 *         fault requiring continued recovery service and explicit stop.
 * \note No hidden worker/wait/allocation. Only observed source/vector
 *       withdrawal permits final pointer detach and release publication.
 */
nx_result_t nx_pwm_adc_service(const nx_pwm_adc_t* port);
/** \brief Close admission and attempt final drain without revoking a loan.
 * \param[in] port: Same live coupled face and serialized privileged task.
 * \return SUCCESS proves stopped sources; BUSY/IO retains recovery storage.
 * \note Continue the unique service/stop until quiet. Cold close additionally
 *       joins vectors and restores saved leases. Fault recovery does not
 *       automatically restart PWM or resume acquisition.
 *       SUCCESS does not release pads, clocks or private storage. Those cold
 *       leases remain held until the platform's final joined close succeeds.
 */
nx_result_t nx_pwm_adc_stop(const nx_pwm_adc_t* port);
/** \brief Copy exact fixed facts and observed active/fault/quarantine state.
 * \param[in] port: Same live coupled face and serialized privileged task.
 * \param[out] state: Distinct aligned output, unchanged on rejection.
 * \return SUCCESS, INVALID/STATE/CONTEXT or UNSUPPORTED for a missing method.
 * \note State is an observation, not a source-drain or release operation.
 */
nx_result_t nx_pwm_adc_state(const nx_pwm_adc_t* port,
                             nx_pwm_adc_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
