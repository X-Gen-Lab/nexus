/**
 * \file            stm32f103_stop.h
 * \brief           Dedicated STM32F103RB PC13 STOP and fresh TIM2 time domain.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F103_STOP_H
#define NEXUS_STM32F103_STOP_H

#include "nexus/components/clock_domain.h"
#include "nexus/core/time.h"
#include "nexus/io/exti.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Call-scoped transition result and independent cleanup evidence. */
typedef struct {
    uint64_t previous_epoch;
    uint64_t current_epoch;
    nx_time_us_t before_us;
    nx_time_us_t after_us;
    nx_result_t status;
    nx_result_t cleanup;
    bool wait_issued;
    bool wake_pending;
    bool irq_mask_retained;
    bool platform_suspended;
    bool resume_ready;
} nx_stm32f1_stop_receipt_t;

/**
 * \brief           Suspend Platform and enter STOP through its selected
 *                  autonomous PC13 falling-edge owner.
 * \param[in]       barrier: Closed and drained caller ClockDomain.
 * \param[in]       wake: Strong factory face selected as exti:stop-wake.
 * \note            Retire all old deadlines and join every borrower first.
 *                  The caller holds the barrier through this operation and
 *                  may reopen it and call nx_platform_resume only when
 *                  resume_ready is true. A failed suspend keeps the ordinary
 *                  Platform retry rules. If Platform is suspended but RUN
 *                  cannot be proved, leave admission closed for controlled
 *                  system recovery. No timestamp bridges the two epochs.
 *                  WFI and pending EXTI are not physical residency evidence.
 */
nx_stm32f1_stop_receipt_t nx_stm32f103_stop_enter(nx_clock_barrier_t barrier,
                                                  const nx_exti_port_t* wake);

#ifdef __cplusplus
}
#endif
#endif
