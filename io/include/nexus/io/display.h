/**
 * \file            display.h
 * \brief           Explicit scanout frame lifetime and retirement proof
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_DISPLAY_H
#define NEXUS_IO_DISPLAY_H

#include "nexus/io/frame_stream.h"
#include "nexus/io/image.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Readonly constraints for one explicitly queried layer.
 * \note Width/height describe the
 *       Board-authorized output window; panel timing, pixel clock, RGB route,
 *       enable/reset/backlight and power policy are not inferred by this API.
 *       Format mask uses (1U << nx_image_format_t).
 */
typedef struct {
    size_t width;
    size_t height;
    size_t address_alignment;
    size_t pitch_alignment;
    size_t maximum_pitch_bytes;
    uint32_t formats;
    uint8_t layer;
} nx_display_constraints_t;

/** \brief Scanout value copied at prepare; frame storage stays caller-owned.
 * \note The view is a complete one-plane byte rectangle, not a copied image.
 *       Rows/row_bytes match the selected window and pixel format; stride is
 *       bytes. span_bytes includes every actual read and declared padding.
 *       Retain any originating consumer loan and follow its own release
 *       contract. SETTLED ends this provider borrow; it does not fabricate
 *       engine-disabled proof for another adapter. A view creates no pool or
 *       stream ownership.
 */
typedef struct {
    nx_frame_dma_view_t frame;
    nx_image_format_t format;
    uint8_t layer;
} nx_display_operation_t;

/** \brief A frame remains borrowed while scanout may still read it.
 * \note ACCEPTED retains descriptor and full frame span until acquire-
 *       observed SETTLED after verified replacement or full stop. VISIBLE,
 *       VSYNC, a CPU DSB or a queued shadow reload is not SETTLED. Retiring
 *       SUCCESS means a previously visible frame was safely replaced, not a
 *       count of displayed pixels or frames; transferred is always zero.
 *       Cancel/timeout/stop initiates retirement and can finish after the
 *       request deadline. Failed withdrawal retains QUARANTINED and the loan.
 */
typedef struct {
    nx_request_t base;
    nx_display_operation_t operation;
} nx_display_request_t;

/** \brief Readonly current observations, not a frame counter or an event queue.
 * \note No field grants storage reuse. visible describes the active frame;
 *       a pending replacement is a separate loan. first_failure is latched
 *       by the provider; no physical presentation time is inferred.
 */
typedef struct {
    nx_result_t first_failure;
    bool active;
    bool visible;
    bool pending;
    bool closing;
    bool quarantined;
    uint8_t layer;
} nx_display_snapshot_t;

/** \brief Shared immutable methods; one controller owns scanout and errors.
 * \note Each selected layer has one active request. All faces of the same
 *       controller share at most one pending hardware commit, with no extra
 *       queue or buffers. One controller face selects a layer explicitly at
 *       query/snapshot or in the operation; layers never own another clock,
 *       IRQ or reset transport.
 */
typedef struct {
    nx_result_t (*query)(void*, uint8_t, nx_display_constraints_t*);
    nx_result_t (*submit)(void*, nx_display_request_t*);
    nx_result_t (*service)(void*);
    nx_result_t (*cancel)(void*, nx_display_request_t*);
    nx_result_t (*snapshot)(void*, uint8_t, nx_display_snapshot_t*);
    nx_result_t (*stop)(void*);
    nx_result_t (*attach_wake)(void*, const nx_irq_wake_t*, uint8_t);
} nx_display_ops_t;

/** \brief Stable controller face; lookup never starts scanout or a scheduler.
 */
struct nx_display_port {
    const nx_display_ops_t* ops;
    void* context;
};

/** \brief Read selected constraints without MMIO effects or borrowing.
 * \param[in] port: Stable selected face, serialized Task use.
 * \param[in] layer: Exact hardware layer identity, never an inferred default.
 * \param[out] constraints: Independent output, unchanged on rejection.
 * \return SUCCESS, INVALID, UNSUPPORTED or provider error.
 * \note A supported format does not prove panel, bandwidth or HIL results.
 */
nx_result_t nx_display_port_query(const nx_display_port_t* port, uint8_t layer,
                                  nx_display_constraints_t* constraints);

/** \brief Copy a display tuple into unborrowed request storage.
 * \param[in,out] request: Initialized READY or consumed SETTLED descriptor.
 * \param[in] operation: Value copied; this metadata object is not retained.
 * \param[in] deadline: Core absolute deadline for the whole frame lifetime;
 *                     NX_DEADLINE_NEVER explicitly permits persistent scanout.
 * \return SUCCESS or INVALID/STATE/EXHAUSTED, unchanged on rejection.
 * \note Deadline is not silently reinterpreted as a latch-only deadline.
 *       All frame bytes are disjoint from the whole request and full private
 *       config/state/resource facts. No pixel bytes are read or copied here.
 */
nx_result_t nx_display_prepare(nx_display_request_t* request,
                               const nx_display_operation_t* operation,
                               nx_time_us_t deadline);

/** \brief Accept an initial frame or one pending replacement.
 * \param[in] port: Privileged, IRQ-unmasked controller owner.
 * \param[in,out] request: READY tuple and stable immutable frame storage.
 * \return SUCCESS means ACCEPTED; BUSY if the one pending slot is occupied.
 * \note Rejection keeps request/frame unchanged and has no residual effect.
 *       Active/pending requests and their full frame spans are mutually
 *       disjoint; no in-place redraw.
 *       All required clock domains must remain alive throughout register
 *       access, scanout and retirement. Products own panel timing and wiring.
 */
nx_result_t nx_display_port_submit(const nx_display_port_t* port,
                                   nx_display_request_t* request);

/** \brief Advance verified latching, replacement and finite retirement.
 * \param[in] port: Same serialized privileged unmasked Task executor.
 * \return SUCCESS after progress; BUSY/IO may retain quarantined frame loans.
 * \note IRQ can capture documented commit, matching active addresses and
 *       actual blanking for the current stage. Task revalidates its exact
 *       stage/withdrawal mask, current state and clock epoch before joining
 *       sources and publishing SETTLED. A wake/reload hint alone is no proof.
 *       A missed reload window can use bounded hardware line IRQ retries;
 *       normal operation does not require a high-frequency polling task.
 *       GCR readback, HAL_OK or DSB alone does not prove old bus reads ended.
 *       Clock loss can stall MMIO itself; deadlines do not interrupt a
 *       stalled bus transaction. A failed proof retains both frame loans.
 */
nx_result_t nx_display_port_service(const nx_display_port_t* port);

/** \brief Request retirement without revoking an active or pending frame.
 * \param[in] port: Same serialized owner.
 * \param[in,out] request: Exact accepted request, kept alive through
 * settlement. \return SUCCESS requests retirement; STATE means another request.
 * \note Pending/visible status does not permit reuse; continue service.
 */
nx_result_t nx_display_port_cancel(const nx_display_port_t* port,
                                   nx_display_request_t* request);

/** \brief Copy current observations without advancing hardware.
 * \param[in] port: Same serialized Task owner; no concurrent public control.
 * \param[in] layer: Exact hardware layer whose active frame is observed.
 * \param[out] snapshot: Output disjoint from all loans and private storage.
 * \return SUCCESS, INVALID, STATE or provider error; unchanged on rejection.
 * \note Task-only; provider synchronizes the copy against IRQ publication.
 *       No allocation, callbacks or physical timing guarantee.
 */
nx_result_t nx_display_port_snapshot(const nx_display_port_t* port,
                                     uint8_t layer,
                                     nx_display_snapshot_t* snapshot);

/** \brief Close the whole controller and retire all its selected layer loans.
 * \param[in] port: Controller face; all layer producers/observers joined.
 * \return SUCCESS only after all scanout/bus/IRQ/publisher references exit;
 *         BUSY/IO retains the cold receipt, request and frame responsibilities.
 * \note A failed clock, reload or register proof never permits reuse. A reset
 *       bit readback does not independently establish outstanding-bus drain.
 *       Successful repeated stop must not access returned configuration.
 *       An independent layer cancellation preserves its peer; stop is
 *       controller-scoped and cannot return resources while any peer lives.
 */
nx_result_t nx_display_port_stop(const nx_display_port_t* port);

/** \brief Bind an optional hint using the existing backend IRQ policy.
 * \param[in] port: Live serialized owner with no accepted frame.
 * \param[in] wake: Stable hint or NULL to detach.
 * \param[in] syscall_ceiling: Must match the selected backend policy.
 * \return SUCCESS or INVALID/CONTEXT/STATE/BUSY/UNSUPPORTED.
 * \note Request predicates remain authoritative. Retain sink/context until
 *       both interrupt sources and all publishers have been joined.
 */
nx_result_t nx_display_port_attach_wake(const nx_display_port_t* port,
                                        const nx_irq_wake_t* wake,
                                        uint8_t syscall_ceiling);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_IO_DISPLAY_H */
