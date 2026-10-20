/**
 * \file            stm32u575_lpdma_lptim1.h
 * \brief           Exact U575 finite LPTIM1 autonomous register update
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_STM32U575_LPDMA_LPTIM1_H
#define NEXUS_COMPONENTS_STM32U575_LPDMA_LPTIM1_H
#include "nexus/components/stm32u575_retained_time.h"
#include "nexus/core/request.h"
#ifdef __cplusplus
extern "C" {
#endif
#if UINTPTR_MAX == UINT32_MAX
#define NX_U575_LPDMA_LPTIM1_STORAGE_BYTES 104U
#elif UINTPTR_MAX == UINT64_MAX
#define NX_U575_LPDMA_LPTIM1_STORAGE_BYTES 136U
#else
#error "U575 LPTIM1 maintains the reviewed ARM32 and host64 owner ABI"
#endif
/** \brief One caller-owned immutable register tuple, in exact SRAM4.
 * \note CCR1 < ARR, RCR=0 and 1023<=ARR<=65535. There is exactly one tuple;
 *       repeated updates need a separate write-OK proof and are not admitted.
 *       No GPIO route or physical waveform playback is established.
 */
typedef struct {
    uint32_t ccr1;
    uint32_t rcr;
    uint32_t arr;
} nx_stm32u575_lptim1_tuple_t;
/** \brief Exactly two generated continuation LLIs; never caller instructions.
 * \note Entire aligned 48-byte extent is separate nonsecure SRAM4. The owner
 *       writes all words only after ACCEPTED; keep immutable until SETTLED.
 */
typedef struct {
    uint32_t private_words[12];
} nx_stm32u575_lpdma_lptim1_workspace_t;
/** \brief Zero caller storage, never copy/reset/reconstruct a live owner.
 * \note One privileged Thread serializes all calls; no ISR/concurrent writer.
 *       Keep owner, clock and every face alive through stop and observer joins.
 */
typedef union {
    uint64_t alignment;
    uintptr_t pointer_alignment;
    unsigned char private_bytes[NX_U575_LPDMA_LPTIM1_STORAGE_BYTES];
} nx_stm32u575_lpdma_lptim1_storage_t;
/** \brief Explicit live retained-clock and same concrete Stop2 binding.
 * \note Use the ordinary retained-clock initializer with wake_owner equal to
 *       nx_stm32u575_stop2_identity(). No AlarmA-copy opt-in is required.
 */
typedef struct {
    nx_stm32u575_retained_time_storage_t* clock;
    const void* wake_owner;
} nx_stm32u575_lpdma_lptim1_config_t;
/** \brief Prepared metadata; all fields are provider-controlled after prepare.
 * \note Initialize base first. ACCEPTED borrows request, tuple and workspace
 *       until acquire-observed SETTLED and external joins. Cancel/deadline do
 *       not revoke a borrow. CPU control storage must be separate from SRAM4
 *       data/LLIs. Never mutate or reprepare while borrowed.
 */
typedef struct {
    nx_request_t base;
    const void* identity;
    const nx_stm32u575_lptim1_tuple_t* tuple;
    void* private_destination;
    size_t private_length;
    nx_stm32u575_lpdma_lptim1_workspace_t* workspace;
    const void* clock_identity;
    uint64_t clock_generation;
} nx_stm32u575_lpdma_lptim1_request_t;
/** \brief Shared immutable methods; service never publishes SETTLED. */
typedef struct {
    nx_result_t (*cancel)(void*, nx_stm32u575_lpdma_lptim1_request_t*);
    nx_result_t (*service)(void*, uint32_t);
    nx_result_t (*stop)(void*, uint32_t);
    nx_result_t (*start)(void*, uint32_t);
} nx_stm32u575_lpdma_lptim1_ops_t;
/** \brief Copied immutable faces retain the exact physical owner identity. */
typedef struct {
    const nx_stm32u575_lpdma_lptim1_ops_t* ops;
    void* context;
} nx_stm32u575_lpdma_lptim1_t;
extern const nx_stm32u575_lpdma_lptim1_ops_t nx_stm32u575_lpdma_lptim1_ops;
/** \brief Bind one pure owner; no MMIO writes, allocation or worker.
 * \note Exact STM32U575ZIT6Q TZEN0, MSIS4/MSIK4/equal buses/Range4, caches off.
 *       Selected SRAM4/LPDMA1 startup and retained LSE RTC access remain live.
 *       LPDMA0 and the entire LPTIM1 controller are exclusive; no other
 * channel, timer writer, IRQ user or external reset exists. Config/face/control
 *       storage must be separate. Rejection retains no new reference.
 */
nx_result_t nx_stm32u575_lpdma_lptim1_initialize(
    nx_stm32u575_lpdma_lptim1_storage_t* storage,
    const nx_stm32u575_lpdma_lptim1_config_t* config,
    nx_stm32u575_lpdma_lptim1_t* receiver);
/** \brief Explicitly prepare cold LPTIM1 access gates and LSE kernel.
 * \note Unmasked privileged Thread, positive per-phase poll budget. Owns only
 *       LPTIM1EN, LPTIM1SEL and added LSESYSEN, preserving RTC/LSE provenance.
 *       LPTIM1 must be reset/cold and initially unclocked with reset mux.
 *       A failed preparation can retain effects; keep owner live and call stop
 *       until it succeeds. It never starts the timer or borrows a request.
 */
nx_result_t
nx_stm32u575_lpdma_lptim1_start(const nx_stm32u575_lpdma_lptim1_t* receiver,
                                uint32_t poll_limit);
/** \brief Prepare one SRAM4 tuple without borrowing or writing data/LLIs.
 * \note Tuple contents remain unchanged from prepare through settlement.
 *       Invalid/aliased/wrapping spans leave every request byte unchanged.
 *       Deadline is absolute retained microseconds or NEVER; no TIM2
 * conversion.
 */
nx_result_t nx_stm32u575_lpdma_lptim1_request_prepare(
    nx_stm32u575_lpdma_lptim1_request_t* request,
    const nx_stm32u575_retained_time_snapshot_t* domain,
    const nx_stm32u575_lptim1_tuple_t* tuple,
    nx_stm32u575_lpdma_lptim1_workspace_t* workspace, nx_time_us_t deadline);
/** \brief Request withdrawal without revoking any borrowed storage.
 * \note Masked privileged MSP Thread through compound recovery. Late cancel
 *       after observed DMA completion rejects STATE. First failure persists.
 */
nx_result_t
nx_stm32u575_lpdma_lptim1_cancel(const nx_stm32u575_lpdma_lptim1_t* receiver,
                                 nx_stm32u575_lpdma_lptim1_request_t* request);
/** \brief Bounded producer/master isolation, never independent settlement.
 * \note Producer UEDE withdrawal precedes DMA suspend/reset/idle. All final
 *       peripheral/IRQ references and Stop2 restoration precede final release.
 *       Unknown clock/access profile permits no peripheral MMIO; external DMA
 *       or LPTIM reset during a loan permanently quarantines that loan.
 */
nx_result_t
nx_stm32u575_lpdma_lptim1_service(const nx_stm32u575_lpdma_lptim1_t* receiver,
                                  uint32_t poll_limit);
/** \brief Close admission and restore owned start effects after compound join.
 * \note Positive per-phase observation budget is not elapsed time. BUSY keeps
 *       all effects/storage alive; recover through the concrete Stop2 owner.
 *       A fresh final write-OK proof means synchronized register writes, never
 *       physical Stop2 residency, output playback, board routing or HIL.
 */
nx_result_t
nx_stm32u575_lpdma_lptim1_stop(const nx_stm32u575_lpdma_lptim1_t* receiver,
                               uint32_t poll_limit);
#ifdef __cplusplus
}
#endif
#endif
