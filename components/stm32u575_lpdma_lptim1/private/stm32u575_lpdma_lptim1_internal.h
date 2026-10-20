/** \file stm32u575_lpdma_lptim1_internal.h
 * \brief Fixed compile alternative for the same physical DMA/Stop2 owner.
 * \author Nexus Team
 */
#ifndef NX_STM32U575_LPDMA_LPTIM1_INTERNAL_H
#define NX_STM32U575_LPDMA_LPTIM1_INTERNAL_H
#include "nexus/components/stm32u575_lpdma_lptim1.h"
#ifdef __cplusplus
extern "C" {
#endif
nx_result_t nx_stm32u575_lpdma_lptim1_validate_ready(
    const nx_stm32u575_lpdma_lptim1_t*, nx_stm32u575_lpdma_lptim1_request_t*,
    nx_stm32u575_retained_time_storage_t*, const void*, uint64_t);
nx_result_t nx_stm32u575_lpdma_lptim1_arm(const nx_stm32u575_lpdma_lptim1_t*,
                                          nx_stm32u575_lpdma_lptim1_request_t*,
                                          uint32_t);
nx_result_t nx_stm32u575_lpdma_lptim1_validate_sleep(
    const nx_stm32u575_lpdma_lptim1_t*, nx_stm32u575_lpdma_lptim1_request_t*,
    nx_stm32u575_retained_time_storage_t*, const void*, uint64_t);
bool nx_stm32u575_lpdma_lptim1_reset_owned(const nx_stm32u575_lpdma_lptim1_t*);
nx_result_t
nx_stm32u575_lpdma_lptim1_producer_start(const nx_stm32u575_lpdma_lptim1_t*,
                                         uint32_t);
nx_result_t
nx_stm32u575_lpdma_lptim1_producer_withdraw(const nx_stm32u575_lpdma_lptim1_t*,
                                            nx_result_t, uint32_t);
nx_result_t nx_stm32u575_lpdma_lptim1_drain(const nx_stm32u575_lpdma_lptim1_t*,
                                            nx_result_t, uint32_t);
nx_result_t
nx_stm32u575_lpdma_lptim1_release(const nx_stm32u575_lpdma_lptim1_t*,
                                  nx_stm32u575_lpdma_lptim1_request_t*,
                                  nx_result_t);
#ifdef __cplusplus
}
#endif
/* These aliases exist only in the mutually exclusive fixed implementation.
 * No public memory-copy face/request is accepted by the LPTIM1 alternative. */
#define NX_U575_LPDMA_COPY_STORAGE_BYTES  NX_U575_LPDMA_LPTIM1_STORAGE_BYTES
#define nx_stm32u575_lpdma_copy_storage_t nx_stm32u575_lpdma_lptim1_storage_t
#define nx_stm32u575_lpdma_copy_workspace_t                                    \
    nx_stm32u575_lpdma_lptim1_workspace_t
#define nx_stm32u575_lpdma_copy_config_t   nx_stm32u575_lpdma_lptim1_config_t
#define nx_stm32u575_lpdma_copy_request_t  nx_stm32u575_lpdma_lptim1_request_t
#define nx_stm32u575_lpdma_copy_ops_t      nx_stm32u575_lpdma_lptim1_ops_t
#define nx_stm32u575_lpdma_copy_t          nx_stm32u575_lpdma_lptim1_t
#define nx_stm32u575_lpdma_copy_ops        nx_stm32u575_lpdma_lptim1_ops
#define nx_stm32u575_lpdma_copy_initialize nx_stm32u575_lpdma_lptim1_initialize
#define nx_stm32u575_lpdma_copy_validate_ready                                 \
    nx_stm32u575_lpdma_lptim1_validate_ready
#define nx_stm32u575_lpdma_copy_arm nx_stm32u575_lpdma_lptim1_arm
#define nx_stm32u575_lpdma_copy_validate_sleep                                 \
    nx_stm32u575_lpdma_lptim1_validate_sleep
#define nx_stm32u575_lpdma_copy_drain   nx_stm32u575_lpdma_lptim1_drain
#define nx_stm32u575_lpdma_copy_release nx_stm32u575_lpdma_lptim1_release
#define nx_stm32u575_lpdma_copy_cancel  nx_stm32u575_lpdma_lptim1_cancel
#define nx_stm32u575_lpdma_copy_service nx_stm32u575_lpdma_lptim1_service
#define nx_stm32u575_lpdma_copy_stop    nx_stm32u575_lpdma_lptim1_stop
#endif
