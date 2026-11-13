/**
 * \file            timer_sequence.h
 * \brief           Native finite update-DMA rows and complete source loans
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_TIMER_SEQUENCE_H
#define NEXUS_IO_TIMER_SEQUENCE_H
#include "nexus/core/request.h"
#include "nexus/io/frame_stream.h"
#include "nexus/io/fwd.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Selected native row layout; every item occupies one DMA transfer. */
typedef enum nx_timer_sequence_layout {
    NX_TIMER_SEQUENCE_ARR_RCR_CCR1_CCR4 = 1
} nx_timer_sequence_layout_t;

/** \brief Logical period and duties in the selected post-prescaler ticks.
 * \note duty_ticks follows the statically selected logical channel order.
 *       A period is positive and each duty is zero through that period.
 *       Encoding may reject values the selected native format cannot express.
 *       Descriptors and duty arrays are immutable ordinary memory during the
 *       synchronous encode call; they are never retained on return.
 */
typedef struct {
    uint32_t period_ticks;
    const uint32_t* duty_ticks;
    size_t channel_count;
} nx_timer_sequence_step_t;

/** \brief One immutable whole frame loan containing complete native rows.
 * \note Initialize/prepare base first. source must be the exact outstanding
 *       frame consumer control. Its single plane has offset zero, contiguous
 *       rows, no padding between rows, and the reported items_per_frame times
 *       native_bits/8 bytes in each row. ARR_RCR_CCR1_CCR4 contains ARR,
 *       RCR/reserved-zero, then CCR1..CCR4 in that order. Unselected compares
 *       and RCR/reserved are zero. ARR plus one meets min_period_ticks; each
 *       selected compare fits that row's period and the native register bits.
 *       ACCEPTED borrows request, control and the whole source through
 *       acquire-observed SETTLED. No source conversion or per-row copy occurs.
 *       SUCCESS reports source storage bytes staged by DMA; it does not count
 *       electrical pulses. Cancellation/error reports zero guaranteed bytes.
 */
typedef struct {
    nx_request_t base;
    nx_frame_dma_loan_t* source;
} nx_timer_sequence_request_t;

/** \brief Nominal format, declared transfer budget and observed owner state.
 * \note The budget is an application-declared upper bound, not a physical
 *       measurement. native_bits describes DMA item storage, independently of
 *       counter width. Tail values may repeat until explicit service joins.
 */
typedef struct {
    nx_timer_sequence_layout_t layout;
    uint32_t tick_hz;
    uint32_t min_period_ticks;
    uint32_t hclk_hz;
    uint32_t burst_budget_hclk_cycles;
    size_t items_per_frame;
    size_t max_dma_items;
    uint8_t native_bits;
    uint8_t channel_mask;
    bool running;
    bool fault;
    bool quarantined;
} nx_timer_sequence_state_t;

/** \brief Hardware UPDATE requests advance normal DMA without per-row service.
 * \note Platform prepare leaves source and DMA off; factory returns only the
 *       existing immutable face. One privileged unmasked task serializes all
 *       methods. submit rejects without retained references or new effects;
 *       acceptance precedes every source/DMA effect. Explicit service handles
 *       terminal status and full withdrawal; no hidden worker exists.
 *       Each complete native burst must finish before the next update. The
 *       selected declared bus budget must be validated under actual load.
 *       Breaking that condition can publish mixed compare values; the API
 *       makes no unconditional cross-update atomicity or continuity claim.
 *       DMA TC/zero NDTR alone cannot release the loan. The owner withdraws
 *       requests and DMA bus activity, observes fresh updates for final shadow
 *       application and its tail period, then joins output/IRQ/address access.
 *       Tail repetitions and initial idle cycles do not imply missing DMA
 *       rows. An absolute deadline includes all final withdrawal. Failed stop
 *       retains the complete borrow and resources; old clock/reset history
 *       cannot be recreated by restoring register appearances. Platform close
 *       must join the same owner before ending the System/time domain. state
 *       output cannot overlap any accepted source or control and is unchanged
 *       on rejection. Physical waveform and throughput qualification is
 * separate.
 */
typedef struct {
    nx_result_t (*submit)(void* context, nx_timer_sequence_request_t* request);
    nx_result_t (*cancel)(void* context, nx_timer_sequence_request_t* request);
    nx_result_t (*service)(void* context);
    nx_result_t (*stop)(void* context);
    nx_result_t (*state)(const void* context, nx_timer_sequence_state_t* state);
    /** \brief Optional explicit logical-to-native preparation, without MMIO. */
    nx_result_t (*encode)(const void* context,
                          const nx_timer_sequence_step_t* steps, size_t count,
                          void* storage, size_t capacity, size_t* bytes);
} nx_timer_sequence_ops_t;

/** \brief Immutable typed identity; vendor state and mutable handles are
 * private.
 */
struct nx_timer_sequence_port {
    const nx_timer_sequence_ops_t* ops;
    void* context;
};
/** \brief The same typed face for direct operations. */
typedef nx_timer_sequence_port_t nx_timer_sequence_t;
/** \brief Admit an already encoded native plan without copying its source.
 * \param[in] port: Stable face under the selected serialized execution
 * contract. \param[in,out] request: Original stable storage under the operation
 * loan contract. \return Provider result, INVALID for malformed public objects,
 * or UNSUPPORTED when this selected operation is absent. \note The selected
 * owner defines context and timing. Dispatch retains no references, starts no
 * worker and never revokes a live borrow. Provider admission still checks
 * private state and payload aliases. The original argument reaches that owner
 * without a temporary or format conversion.
 */
nx_result_t nx_timer_sequence_port_submit(const nx_timer_sequence_port_t* port,
                                          nx_timer_sequence_request_t* request);
/** \brief Request withdrawal without revoking the source or request loan.
 * \param[in] port: Stable face under the selected serialized execution
 * contract. \param[in,out] request: Original stable storage under the operation
 * loan contract. \return Provider result, INVALID for malformed public objects,
 * or UNSUPPORTED when this selected operation is absent. \note The selected
 * owner defines context and timing. Dispatch retains no references, starts no
 * worker and never revokes a live borrow. Provider admission still checks
 * private state and payload aliases. The original argument reaches that owner
 * without a temporary or format conversion.
 */
nx_result_t nx_timer_sequence_port_cancel(const nx_timer_sequence_port_t* port,
                                          nx_timer_sequence_request_t* request);
/** \brief Advance bounded tail and withdrawal work; query the request for
 * settlement. \param[in] port: Stable face under the selected serialized
 * execution contract. \return Provider result, INVALID for malformed public
 * objects, or UNSUPPORTED when this selected operation is absent. \note The
 * selected owner defines context and timing. Dispatch retains no references,
 * starts no worker and never revokes a live borrow. Provider admission still
 * checks private state and payload aliases. The original argument reaches that
 * owner without a temporary or format conversion.
 */
nx_result_t
nx_timer_sequence_port_service(const nx_timer_sequence_port_t* port);
/** \brief Withdraw the source; failed drain preserves every outstanding loan.
 * \param[in] port: Stable face under the selected serialized execution
 * contract. \return Provider result, INVALID for malformed public objects, or
 *         UNSUPPORTED when this selected operation is absent.
 * \note The selected owner defines context and timing. Dispatch retains no
 *       references, starts no worker and never revokes a live borrow. Provider
 *       admission still checks private state and payload aliases. The original
 *       argument reaches that owner without a temporary or format conversion.
 */
nx_result_t nx_timer_sequence_port_stop(const nx_timer_sequence_port_t* port);
/** \brief Copy native plan constraints and current owner observations.
 * \param[in] port: Stable face under the selected serialized execution
 * contract. \param[out] state: Independent output, changed only as the provider
 * contract permits. \return Provider result, INVALID for malformed public
 * objects, or UNSUPPORTED when this selected operation is absent. \note The
 * selected owner defines context and timing. Dispatch retains no references,
 * starts no worker and never revokes a live borrow. Provider admission still
 * checks private state and payload aliases. The original argument reaches that
 * owner without a temporary or format conversion.
 */
nx_result_t nx_timer_sequence_port_state(const nx_timer_sequence_port_t* port,
                                         nx_timer_sequence_state_t* state);

/** \brief Encode logical steps into caller-owned native storage before submit.
 * \param[in] port: Prepared idle owner under its serialized task contract.
 * \param[in] steps: Positive immutable descriptor array and complete duties.
 * \param[in] count: Step count within the selected native transfer limit.
 * \param[out] storage: Exclusive ordinary writable memory, not a live loan.
 * \param[in] capacity: Complete writable extent, including unused tail bytes.
 * \param[out] bytes: Separate output receiving the exact encoded byte count.
 * \return SUCCESS, INVALID for malformed or overlapping storage, UNSUPPORTED
 *         for absent encoding or unrepresentable values, OVERFLOW for short
 *         storage, BUSY for an active owner, or STATE/CONTEXT.
 * \note Synchronous task-only preparation with work bounded by the selected
 *       step and channel limits. No peripheral MMIO, admission, allocation,
 *       conversion at submit, retained reference or owner-state change occurs.
 *       All checks precede writes; rejection leaves storage and bytes
 * unchanged. All output extents are disjoint from descriptors, duties, private
 *       metadata and each other. Caller proves ordinary memory ownership and
 *       stability; the provider rejects known MMIO/private aliases. Success
 *       writes only the encoded prefix; unused capacity stays unchanged.
 *       Publish using the existing native frame geometry and loan operations.
 *       Encoding does not grant DMA reachability or prove waveform timing.
 */
nx_result_t nx_timer_sequence_port_encode(const nx_timer_sequence_port_t* port,
                                          const nx_timer_sequence_step_t* steps,
                                          size_t count, void* storage,
                                          size_t capacity, size_t* bytes);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_IO_TIMER_SEQUENCE_H */
