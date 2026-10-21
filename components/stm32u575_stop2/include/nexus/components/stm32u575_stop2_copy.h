/**
 * \file            stm32u575_stop2_copy.h
 * \brief           Exact same-owner U575 Stop2 finite SRAM4 copy boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_STM32U575_STOP2_COPY_H
#define NEXUS_COMPONENTS_STM32U575_STOP2_COPY_H
#include "nexus/components/stm32u575_lpdma_copy.h"
#include "nexus/components/stm32u575_stop2.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Separate copy admission and exact Stop2 transition observations.
 * \note accepted means the request, buffers and workspace became borrowed,
 *       including when later RTC/power setup fails. Read the final copy result
 *       and count only through acquire-observed request SETTLED. transition
 *       describes the boundary; dma_wake records observed ch0 NVIC pending.
 */
typedef struct {
    bool accepted;
    bool dma_wake;
    nx_stm32u575_stop2_result_t transition;
} nx_stm32u575_stop2_copy_result_t;
/**
 * \brief           Cross Stop2 with one finite AlarmA-triggered SRAM4 copy.
 * \param[in]       owners: Exact closed, drained RUN-domain barrier.
 * \param[in]       copy: Immutable initialized LPDMA1/channel0 face.
 * \param[in,out]   request: Prepared READY copy in the bound retained domain.
 * \param[in]       escape_ticks: 4096..65536 RTC/16 ticks, nominal 2..32 s.
 * \param[in]       poll_limit: Positive observation budget for each phase.
 * \return          Admission, transition, cleanup and retained effects.
 * \note            All masked-MSP-Thread and stopped/joined RUN preconditions
 *                  of enter remain required. The clock uses the explicit
 *                  AlarmA-copy initializer and this owner's wake identity.
 *                  Selected startup provides nonsecure SRAM4 and prepared
 *                  LPDMA1 access; unrelated channels and masters are quiet.
 *                  The receiver validates the actual binding and registers.
 *                  No caller callback or gate mask authorizes exceptions.
 * \note            All calendar mask bits are set; ALRMASSR=0, ALRAIE=0 and
 *                  ALRAFCLR=0. The receiver is ready before ALRAE enables
 *                  the next whole-second event on trigger14. ALRAF and DMA
 *                  pending remain intact while the producer is enabled. WUT is
 *                  an independent escape. The receiver owns its generated
 *                  second LLI and LPDMA1 channel0 IRQ114 wake vector.
 *                  wait_executed proves WFI, never physical residency.
 * \note            Rejection leaves the new request unborrowed. Once accepted,
 *                  setup failure retains every copy reference until RTC/WUT
 *                  producer, DMA master/reset, IRQ, gate and clock restoration
 *                  are independently quiet. Keep both owners, clock, buffers,
 *                  workspace and RUN barrier live and closed. Preserve
 *                  PRIMASK=1 through retained recovery. recover retries the
 *                  same compound chain without WFI. An observed external
 *                  LPDMA reset permanently quarantines the chain: quiet reset
 *                  values and deassertion cannot prove the prior master's
 *                  final access. Only an externally established system reset
 *                  ends that responsibility. A tainted clock may settle an
 *                  error after hardware drain, never a successful fresh elapsed
 *                  time. No old TIM2 deadline, RTOS compensation, peripheral
 *                  LPBAM plan or physical copy result is established.
 * \note            Link only Nexus::U575Stop2Copy for this mode. It owns the
 *                  same static RUN domain and WUT/recover symbols as
 *                  Nexus::U575Stop2; linking both alternatives is rejected.
 *                  WUT-only enter remains available, but rejects every retained
 *                  copy loan. Keep PRIMASK=1 until complete recovery.
 */
nx_stm32u575_stop2_copy_result_t
nx_stm32u575_stop2_copy_enter(nx_clock_barrier_t owners,
                              const nx_stm32u575_lpdma_copy_t* copy,
                              nx_stm32u575_lpdma_copy_request_t* request,
                              uint32_t escape_ticks, uint32_t poll_limit);
#ifdef __cplusplus
}
#endif
#endif
