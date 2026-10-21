/**
 * \file            stm32u575_lpdma_copy.h
 * \brief           Exact U575 SRAM4 finite autonomous copy receiver
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_STM32U575_LPDMA_COPY_H
#define NEXUS_COMPONENTS_STM32U575_LPDMA_COPY_H
#include "nexus/components/stm32u575_retained_time.h"
#include "nexus/core/request.h"
#ifdef __cplusplus
extern "C" {
#endif
#if UINTPTR_MAX == UINT32_MAX
#define NX_U575_LPDMA_COPY_STORAGE_BYTES 88U
#elif UINTPTR_MAX == UINT64_MAX
#define NX_U575_LPDMA_COPY_STORAGE_BYTES 120U
#else
#error "U575 copy maintains the reviewed ARM32 and host64 owner ABI"
#endif
/** \brief Zero-initialized caller owner; never copy, reset or reconstruct live.
 * \note Keep this storage, every copied face and bound clock alive until stop
 *       succeeds and all external observers have joined. One exact ch0 owner
 *       exists; there is no allocation, queue, worker or factory lookup.
 *       One privileged Thread serializes every lifecycle/control operation.
 *       No ISR, concurrent producer or unjoined delayed cancellation command
 *       may use request pointers across prepare/reuse.
 */
typedef union {
    uint64_t alignment;
    uintptr_t pointer_alignment;
    unsigned char private_bytes[NX_U575_LPDMA_COPY_STORAGE_BYTES];
} nx_stm32u575_lpdma_copy_storage_t;
/** \brief One generated full next LLI; raw caller instructions are not used.
 * \note Entire 24-byte extent must be four-byte aligned in selected SRAM4.
 *       Provider overwrites all words only after admission. Keep it immutable
 *       and disjoint from both buffers and all request/control storage while
 *       borrowed. A NOINIT section never establishes descriptor construction.
 */
typedef struct {
    uint32_t private_words[6];
} nx_stm32u575_lpdma_copy_workspace_t;
/** \brief Explicit same-owner clock binding; copied only on initialization.
 * \note clock is the live opt-in AlarmA-copy retained clock. wake_owner is the
 *       stable concrete Stop2 owner identity. Identity equality is trusted
 *       assembly binding, never hardware authentication. No RTC writer exists
 *       in this component.
 */
typedef struct {
    nx_stm32u575_retained_time_storage_t* clock;
    const void* wake_owner;
} nx_stm32u575_lpdma_copy_config_t;
/** \brief One finite two-block copy in the independent retained time domain.
 * \note Initialize base before prepare. Prepared metadata is caller immutable
 *       from ACCEPTED until acquire-observed SETTLED and external joins.
 *       source/destination/whole workspace must be disjoint exact nonsecure
 *       SRAM4 spans, not CPU-visible SRAM1 or secure aliases. length is at
 * least two bytes; byte-width incremented transfers preserve exact byte count.
 */
typedef struct {
    nx_request_t base;
    const void* identity;
    const void* source;
    void* destination;
    size_t length;
    nx_stm32u575_lpdma_copy_workspace_t* workspace;
    const void* clock_identity;
    uint64_t clock_generation;
} nx_stm32u575_lpdma_copy_request_t;
/** \brief Shared readonly methods; public service never releases the chain. */
typedef struct {
    nx_result_t (*cancel)(void* context,
                          nx_stm32u575_lpdma_copy_request_t* request);
    nx_result_t (*service)(void* context, uint32_t poll_limit);
    nx_result_t (*stop)(void* context, uint32_t poll_limit);
} nx_stm32u575_lpdma_copy_ops_t;
/** \brief Immutable typed face; copied faces still name the exact live owner.
 */
typedef struct {
    const nx_stm32u575_lpdma_copy_ops_t* ops;
    void* context;
} nx_stm32u575_lpdma_copy_t;
/** \brief Exact physical implementation identity, not arbitrary callbacks. */
extern const nx_stm32u575_lpdma_copy_ops_t nx_stm32u575_lpdma_copy_ops;
/**
 * \brief Construct one serialized privileged Thread owner without MMIO writes.
 * \param[in,out] storage: Fresh zero or clean stopped and externally joined.
 * \param[in] config: Separate immutable clock/owner binding.
 * \param[out] copy: Separate writable immutable face, unchanged on failure.
 * \note Exact STM32U575ZIT6Q TZEN0/MSIS4/MSIK4/equal buses/Range4 and selected
 * SRAM4 only. LPDMA1EN/SRAM4EN and power/retention belong to selected startup;
 *       every channel and vector must initially be quiet. The run-gate owner
 *       keeps both gates and exclusive hardware configuration stable through
 *       stop and joins; no other LPDMA channel may start. This component owns
 *       only fixed sleep/autonomous bits and preserves unrelated RTC bits.
 *       SRAM4EN and power/retention preconditions belong to startup and
 *       the combined sleep owner. This component never repairs lost RAM by
 *       changing SRAM4PD/PDS and never disables the caller's SRAM4EN.
 */
nx_result_t nx_stm32u575_lpdma_copy_initialize(
    nx_stm32u575_lpdma_copy_storage_t* storage,
    const nx_stm32u575_lpdma_copy_config_t* config,
    nx_stm32u575_lpdma_copy_t* copy);
/**
 * \brief Prepare copied metadata in an unborrowed request, without MMIO.
 * \param[in,out] request: Initialized READY or consumed SETTLED, fully joined.
 * \param[in] domain: Separate successful retained clock sample.
 * \param[in] source: Borrowed immutable bytes wholly inside nonsecure SRAM4.
 * \param[out] destination: Separate borrowed writable SRAM4 span.
 * \param[in] length: At least two bytes; both full extents must fit SRAM4.
 * \param[in,out] workspace: Separate aligned 24-byte SRAM4 LLI workspace.
 * \param[in] deadline: Absolute microseconds in that domain, or NEVER.
 * \note Rejecting a span, alias, wrap or state leaves every request byte
 *       unchanged. Preparation does not borrow or write any buffer/workspace.
 *       Old Core TIM2 deadlines are never translated into this domain.
 */
nx_result_t nx_stm32u575_lpdma_copy_request_prepare(
    nx_stm32u575_lpdma_copy_request_t* request,
    const nx_stm32u575_retained_time_snapshot_t* domain, const void* source,
    void* destination, size_t length,
    nx_stm32u575_lpdma_copy_workspace_t* workspace, nx_time_us_t deadline);
/** \brief Latch cancellation of the exact active identity without release.
 * \param[in] copy: Immutable face of the initialized physical receiver.
 * \param[in,out] request: Exact active request, retained through SETTLED.
 * \note Late cancellation after observed channel completion rejects STATE.
 *       Cancellation before unobserved completion fixes the first failure.
 *       DMA drain alone cannot release an AlarmA/WUT/clock restoration loan.
 */
nx_result_t
nx_stm32u575_lpdma_copy_cancel(const nx_stm32u575_lpdma_copy_t* copy,
                               nx_stm32u575_lpdma_copy_request_t* request);
/** \brief Observe and perform bounded DMA withdrawal; never publish SETTLED.
 * \param[in] copy: Immutable face of the initialized physical receiver.
 * \param[in] poll_limit: Positive observation budget for each drain phase.
 * \note Positive finite per-phase observation budget, not a time estimate.
 *       BUSY retains request, both buffers and workspace, even after DMA idle.
 *       Only the concrete Stop2 owner can release after the entire RTC/IRQ/
 *       DMA/clock/gate chain is restored. Unknown profile performs no DMA MMIO.
 *       An externally asserted LPDMA reset observed while borrowed leaves an
 *       uncertain loan permanently quarantined; reset-default IDLE cannot
 *       prove earlier master accesses joined. No software force release exists.
 *       Preserve incoming masks; while borrowed the concrete Stop2 boundary
 *       requires PRIMASK1, BASEPRI/FAULTMASK0 until complete recovery.
 */
nx_result_t
nx_stm32u575_lpdma_copy_service(const nx_stm32u575_lpdma_copy_t* copy,
                                uint32_t poll_limit);
/** \brief Close admission, retaining all live loans until compound recovery.
 * \param[in] copy: Immutable face of the initialized physical receiver.
 * \param[in] poll_limit: Positive observation budget for each drain phase.
 * \note Keep every related object alive and RUN admission closed on BUSY.
 *       Repeated clean stop is safe after external publishers have joined.
 *       No Stop2 residency, LPBAM peripheral, physical copy or HIL result is
 *       implied by the public type or successful software execution.
 */
nx_result_t nx_stm32u575_lpdma_copy_stop(const nx_stm32u575_lpdma_copy_t* copy,
                                         uint32_t poll_limit);
#ifdef __cplusplus
}
#endif
#endif
