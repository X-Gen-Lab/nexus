/**
 * \file            stm32u575_stop2.h
 * \brief           Exact U575 TZEN0 retained-RTC Stop2 boundary
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_STM32U575_STOP2_H
#define NEXUS_COMPONENTS_STM32U575_STOP2_H
#include "nexus/components/clock_domain.h"
#include "nexus/components/stm32u575_retained_time.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Bounded transition result; snapshots belong only to the RTC domain.
 */
typedef struct {
    nx_result_t primary;
    nx_result_t cleanup;
    uint32_t remaining_effects;
    bool wait_executed;
    bool wake_flag;
    nx_stm32u575_retained_time_snapshot_t before;
    nx_stm32u575_retained_time_snapshot_t after;
} nx_stm32u575_stop2_result_t;
/** \brief Stable trusted assembly identity for retained-time wake_owner. */
const void* nx_stm32u575_stop2_identity(void);
/**
 * \brief           Bind the static owner and construct its RUN domain.
 * \param[in]       capacity: Positive bound on external consumer leases.
 * \param[in,out]   clock: Live retained-time owner bound to this identity.
 * \return          SUCCESS, INVALID, CONTEXT, STATE or clock failure.
 * \note            Serialized privileged unmasked Thread, once. Retain clock
 *                  storage throughout this owner's lifetime. No hardware
 *                  writes or heap allocation occur.
 */
nx_result_t
nx_stm32u575_stop2_initialize(size_t capacity,
                              nx_stm32u575_retained_time_storage_t* clock);
/** \brief Pure lookup; all platform/product RUN owners enroll explicitly. */
nx_clock_domain_t* nx_stm32u575_stop2_domain(void);
/**
 * \brief           Arm WUT, cross Stop2, restore and drain with PRIMASK=1.
 * \param[in]       owners: Closed, drained barrier from this exact domain.
 * \param[in]       wake_ticks: 2..65536 RTC/16 ticks, nominal 488.28125us.
 * \param[in]       poll_limit: Positive finite per-phase observation budget.
 * \return          Primary/cleanup results, retained effects and RTC samples.
 * \note            Exact STM32U575ZIT6Q TZEN0 baremetal only. Privileged MSP
 *                  Thread, CONTROL=0, PRIMASK=1, BASEPRI=FAULTMASK=0. No
 *                  scheduler may have started. Close RUN admission, stop
 *                  Platform/System and join every producer, IRQ, DMA, observer
 *                  and deadline user. Release leases only after real drain;
 *                  pass the closed barrier when no consumers remain.
 *                  Enrollment is never automatic.
 *                  End Core TIM2 deadlines; TIM2 is not read or restarted.
 *                  The independent RTC clock stays outside this RUN domain.
 *                  Requires MSIS4/MSIK4/equal buses, quiet gates, all SRAM
 * retained, caches/debug/MPU/SysTick/other IRQ and EXTI sources off. Reading
 * SysTick CTRL consumes COUNTFLAG on rejection too. Exclusive RTC: bound
 * calendar reader and this WUT writer. Boot supplies valid retained LSE
 * calendar and DBP=1. Only WUT is written; WPR is unlocked then relocked each
 * phase. Exclude NMI/HardFault, debugger, watchdogs, other masters and
 * asynchronous configuration writers externally. Zero gates cannot prove those
 * obligations. Binding is trusted assembly identity, never hardware
 * authentication. WUTR=wake_ticks-1. First WUTF is between WUTR and WUTR+2
 *                  ticks. RTC samples resolve 3906.25us; short wakes may show
 *                  zero elapsed. Retained max_gap_seconds must cover sleep,
 *                  arm/drain execution and externally bounded wake latency.
 *                  WFI has no CPU duration bound if hardware wake fails.
 *                  A missed sample gap taints the independent clock epoch.
 *                  wait_executed proves WFI ran, never physical residency.
 *                  All returns preserve masks. Success requires observed WUT,
 *                  IRQ and added gate withdrawal plus MSIS4/MSIK4/Range4
 * restore. Failed drain retains ownership. Keep admission closed and related
 * objects alive; call recover before any restart. No failure revokes a loan.
 * Restarting Platform creates a new TIM2 epoch; no old deadline survives that
 * lifecycle. RTOS Tick compensation and LPBAM remain unsupported.
 */
nx_stm32u575_stop2_result_t nx_stm32u575_stop2_enter(nx_clock_barrier_t owners,
                                                     uint32_t wake_ticks,
                                                     uint32_t poll_limit);
/**
 * \brief           Retry retained WUT/IRQ/clock drain without another WFI.
 * \note            Same masked boundary and exact closed barrier. Recovery
 *                  cannot repair a tainted RTC epoch or authorize old
 *                  deadlines. Hardware drain can finish with invalid RTC
 *                  sampling. Join all retained-clock users before its
 *                  explicit stop/reopen.
 */
nx_stm32u575_stop2_result_t
nx_stm32u575_stop2_recover(nx_clock_barrier_t owners, uint32_t poll_limit);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_STM32U575_STOP2_H */
