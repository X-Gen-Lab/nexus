/** \file stm32u575_lpdma_copy_internal.h
 * \brief Concrete Stop2/copy chain collaboration; not product callbacks.
 * \author Nexus Team
 */
#ifndef NX_STM32U575_LPDMA_COPY_INTERNAL_H
#define NX_STM32U575_LPDMA_COPY_INTERNAL_H
#include "nexus/components/stm32u575_lpdma_copy.h"
#ifdef __cplusplus
extern "C" {
#endif
nx_result_t nx_stm32u575_lpdma_copy_validate_ready(
    const nx_stm32u575_lpdma_copy_t* copy,
    nx_stm32u575_lpdma_copy_request_t* request,
    nx_stm32u575_retained_time_storage_t* clock, const void* wake_owner,
    uint64_t generation);
/* Only the concrete Stop2 owner calls arm with its closed/drained barrier and
 * quiet RTC producer. SUCCESS is ACCEPTED, including later setup failures.
 * Rejection leaves no retained reference, workspace write or hardware effect.
 */
nx_result_t
nx_stm32u575_lpdma_copy_arm(const nx_stm32u575_lpdma_copy_t* copy,
                            nx_stm32u575_lpdma_copy_request_t* request);
nx_result_t nx_stm32u575_lpdma_copy_validate_sleep(
    const nx_stm32u575_lpdma_copy_t* copy,
    nx_stm32u575_lpdma_copy_request_t* request,
    nx_stm32u575_retained_time_storage_t* clock, const void* wake_owner,
    uint64_t generation);
/* Stop2 withdraws and observes AlarmA/WUT first. First non-success cause wins;
 * observed completion rejects late public cancellation, but a compound owner
 * failure still prevents successful release. SUCCESS here means DMA/IRQ/gates
 * isolated, never request settlement. Failure retains every reference. */
nx_result_t nx_stm32u575_lpdma_copy_drain(const nx_stm32u575_lpdma_copy_t* copy,
                                          nx_result_t cause,
                                          uint32_t poll_limit);
/* Call only after complete Stop2 RTC/IRQ/power/clock/gate restore (effects=0),
 * with the Stop2 active face/request references already detached. Failure
 * performs no publication and retains the exact provider/request references;
 * caller restores its references. Success publishes final SETTLED and makes
 * no subsequent request/workspace/buffer access. */
nx_result_t
nx_stm32u575_lpdma_copy_release(const nx_stm32u575_lpdma_copy_t* copy,
                                nx_stm32u575_lpdma_copy_request_t* request,
                                nx_result_t final_result);
#ifdef __cplusplus
}
#endif
#endif
