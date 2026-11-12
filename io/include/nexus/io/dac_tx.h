/**
 * \file            dac_tx.h
 * \brief           Borrowed encoded DAC frames under one serialized owner
 * \author          Nexus Team
 * \note            Transport selection is static. No allocation, queue or
 *                  worker is created by preparation or interface lookup.
 */
#ifndef NEXUS_IO_DAC_TX_H
#define NEXUS_IO_DAC_TX_H
#include "nexus/core/request.h"
#include "nexus/io/dac.h"
#include "nexus/io/wake.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Descriptor and immutable encoded frames remain live until SETTLED.
 * \note Raw CPU writes use unshifted codes. This separate transfer interface
 *       borrows hardware-encoded frames without conversion or a bounce buffer.
 *       Single u8 frames are bytes; single 12-bit frames are uint16_t, with
 *       code in bits 11:0 or 15:4. Dual u8 frames put channel 1 in bits 7:0
 *       and channel 2 in bits 15:8. Dual 12-bit uint32_t frames place each
 *       channel in its own 16-bit lane, using the selected alignment. Unused
 *       bits must be zero; the caller owns that invariant without a hidden
 *       whole-buffer conversion/validation pass. Length observations are bytes.
 */
typedef struct {
    nx_request_t base;
    const void* data;
    size_t length;
    uint32_t
        trigger_hz; /**< Zero explicitly denotes an unknown external rate. */
} nx_dac_tx_request_t;

/** \brief Shared immutable methods receive the selected instance context. */
typedef struct {
    nx_result_t (*submit)(void* context, nx_dac_tx_request_t* request);
    nx_result_t (*cancel)(void* context, nx_dac_tx_request_t* request);
    void (*service)(void* context);
    nx_result_t (*attach_wake)(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling);
    nx_time_us_t (*next_service)(const void* context);
} nx_dac_tx_ops_t;

/** \brief Stable optional transfer face; CPU-only modes need no TX storage. */
struct nx_dac_tx_port {
    const nx_dac_tx_ops_t* ops;
    void* context;
};

/**
 * \brief Prepare an unborrowed descriptor without copying encoded frames.
 * \param[in,out] request: Initialized READY or consumed SETTLED storage.
 * \param[in] data: Independent immutable frames, live through settlement.
 * \param[in] length: Nonzero byte extent; provider validates frame geometry.
 * \param[in] trigger_hz: Requested internal rate; zero for an external source
 *                        whose frequency is unknown to this port.
 * \param[in] deadline: Absolute live Core deadline, never a duration.
 * \return SUCCESS, INVALID or STATE; rejection leaves the descriptor intact.
 * \note Serialized task owner only. Preparation does not touch hardware or
 *       validate an unknown provider's memory reachability or frequency.
 */
nx_result_t nx_dac_tx_prepare(nx_dac_tx_request_t* request, const void* data,
                              size_t length, uint32_t trigger_hz,
                              nx_time_us_t deadline);
/**
 * \brief Admit one direct finite or circular transfer selected by the port.
 * \param[in] port: Initialized face owned by this serialized task executor.
 * \param[in,out] request: READY descriptor and immutable encoded payload.
 * \return SUCCESS means ACCEPTED, including later hardware start failure;
 *         INVALID, BUSY, STATE, CONTEXT, TIMEOUT, PERMISSION or UNSUPPORTED
 *         is REJECTED.
 * \note REJECTED retains nothing. ACCEPTED borrows until acquire-observed
 *       SETTLED, including timeout and quarantine. Service must be progressed.
 *       Finite SUCCESS includes the last digital latch, not analog settling.
 *       The first trigger outputs the declared idle code before frame zero is
 *       fetched. Delayed service can repeat the final code; finite mode does
 *       not promise exactly length/frame-size analog conversions. A finite
 *       tail may pause and restart the exclusively owned trigger timer,
 *       extending the last interval; finite output is not strictly periodic.
 *       Selected recovery can internally trigger the declared idle code before
 *       waveform timing starts, then briefly disable converter outputs. These
 *       conversions are not payload progress. No uninterrupted analog hold or
 *       waveform is promised between requests.
 *       External circular modes can retain an exclusively leased timer only
 *       for the same idle-output recovery. Playback starts detection only
 *       after its memory route is armed and never starts that timer. A mode
 *       with no proven final external conversion rejects finite playback.
 *       Circular transfers settle only after cancellation, stop or failure.
 *       Circular byte progress describes the final cycle's consumed prefix,
 *       not total playback. DMA progress never counts digital conversions.
 */
nx_result_t nx_dac_tx_port_submit(const nx_dac_tx_port_t* port,
                                  nx_dac_tx_request_t* request);
/**
 * \brief Request withdrawal; this never revokes the descriptor or frame loan.
 * \param[in] port: Same serialized executor and stable instance face.
 * \param[in,out] request: Exact live descriptor, retained through SETTLED.
 * \return SUCCESS, INVALID, STATE, BUSY, CONTEXT or UNSUPPORTED.
 * \note Cross-task controls require a separate stable slot/epoch adapter.
 */
nx_result_t nx_dac_tx_port_cancel(const nx_dac_tx_port_t* port,
                                  nx_dac_tx_request_t* request);
/**
 * \brief Progress deadlines, final digital latch and complete withdrawal.
 * \param[in] port: Same nonreentrant serialized task owner; stable storage.
 * \note No hidden worker or blocking completion wait. An IRQ is a hint;
 *       request state is authoritative. An invalid or absent method is a no-op.
 *       The matching DAC control face owns stop, idle code and observation.
 */
void nx_dac_tx_port_service(const nx_dac_tx_port_t* port);
/**
 * \brief Bind an explicit IRQ hint target while the controller is idle.
 * \param[in] port: Same serialized task owner with all IRQ storage live.
 * \param[in] wake: Borrowed immutable target, or NULL to detach it.
 * \param[in] syscall_ceiling: Selected backend's unshifted IRQ ceiling.
 * \return SUCCESS, INVALID, STATE, BUSY, CONTEXT, PERMISSION or UNSUPPORTED.
 * \note The provider validates actual publisher priorities. Target and context
 *       outlive attachment and all publishers; shutdown/detachment joins them.
 *       A hint never returns a borrow or substitutes for latched request state.
 */
nx_result_t nx_dac_tx_port_attach_wake(const nx_dac_tx_port_t* port,
                                       const nx_irq_wake_t* wake,
                                       uint8_t syscall_ceiling);
/**
 * \brief Query when the caller should next progress service.
 * \param[in] port: Same serialized task owner; no concurrent controls.
 * \return Absolute live Core deadline; NEVER means wait for a new IRQ/control.
 * \note This is a scheduling hint, not a completion or recovery guarantee.
 *       A quarantined request still requires the caller's recovery policy.
 */
nx_time_us_t nx_dac_tx_port_next_service(const nx_dac_tx_port_t* port);
#ifdef __cplusplus
}
#endif
#endif
