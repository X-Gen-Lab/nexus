/**
 * \file            stm32u575_stop2_lptim1.h
 * \brief           Same concrete Stop2 owner and finite LPTIM1 LPBAM boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_STM32U575_STOP2_LPTIM1_H
#define NEXUS_COMPONENTS_STM32U575_STOP2_LPTIM1_H
#include "nexus/components/stm32u575_lpdma_lptim1.h"
#include "nexus/components/stm32u575_stop2.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Admission and software sleep observations, not the final result.
 * \note accepted borrows request/tuple/workspace even when setup later fails.
 *       dma_wake is pending IRQ114; wait_executed proves WFI execution only.
 */
typedef struct {
    bool accepted;
    bool dma_wake;
    nx_stm32u575_stop2_result_t transition;
} nx_stm32u575_stop2_lptim1_result_t;
/** \brief One finite hardware-requested LPTIM1 tuple across Stop2.
 * \note Link only Nexus::U575Stop2Lptim1; WUT/copy alternatives are exclusive.
 *       Same static owner, RUN ClockDomain and recovery. Ordinary retained time
 *       uses this wake identity; explicitly start the dedicated receiver first.
 *       Closed/drained exact RUN barrier, stopped Platform, privileged MSP
 *       Thread CONTROL0/PRIMASK1/BASEPRI0/FAULTMASK0 remain required. Keep
 * masks and RUN admission closed through all retained recovery. \note LPTIM1
 * LSE/preload/CC1E, initial ARR1023/RCR0; request13/BREQ1 gates CCR1, two
 * generated software-requested LLIs write RCR and ARR last. No AlarmA trigger.
 * WUT escapes at 4096..65536 RTC/16 ticks. Positive poll_limit bounds
 * observations, not WFI duration or physical response. \note UEDE
 * withdrawal/DIEROK precede DMA suspend/reset/idle and reference detach. Fresh
 * write-OK gates success. Owned RCC reset disables LPTIM1 per ES0499. All
 * peripheral/IRQ/gate/clock restoration precedes SETTLED; start access gates
 * stay until joined public stop. Cancel/deadline never revoke storage. Unknown
 * profile permits no peripheral MMIO. External peripheral reset permanently
 * quarantines an active loan until system reset. Recovery never runs WFI. No
 * GPIO/Board route, output playback, physical residency, timing/power or HIL
 * result is implied.
 */
nx_stm32u575_stop2_lptim1_result_t
nx_stm32u575_stop2_lptim1_enter(nx_clock_barrier_t owners,
                                const nx_stm32u575_lpdma_lptim1_t* receiver,
                                nx_stm32u575_lpdma_lptim1_request_t* request,
                                uint32_t escape_ticks, uint32_t poll_limit);
#ifdef __cplusplus
}
#endif
#endif
