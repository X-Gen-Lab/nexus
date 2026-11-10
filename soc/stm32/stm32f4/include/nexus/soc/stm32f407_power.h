/**
 * \file            stm32f407_power.h
 * \brief           Explicit drained STOP/Standby and time-domain receipts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F407_POWER_H
#define NEXUS_STM32F407_POWER_H
#include "nexus/components/clock_domain.h"
#include "nexus/io/rtc_aux.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Explicit regulator, wake routes and authorization to discard WUF.
 * \note wake_pin temporarily arms PA0 WKUP for Standby. The caller owns its
 *       pad/electrical plan; the original EWUP state is restored on return.
 *       discard_wakeup permits clearing existing PWR WUF only for Standby.
 */
typedef struct {
    uint32_t wake_irqs[3];
    uint32_t poll_limit;
    bool low_power_regulator;
    bool standby;
    uint32_t wake_events;
    bool discard_wakeup;
    bool wake_pin;
} nx_stm32_power_options_t;

#define NX_STM32_WAKE_RTC_ALARM  (UINT32_C(1) << 17U)
#define NX_STM32_WAKE_RTC_TAMPER (UINT32_C(1) << 21U)
#define NX_STM32_WAKE_RTC_TIMER  (UINT32_C(1) << 22U)

/** \brief STOP ends one boot timer epoch; no continuous Core time is claimed.
 */
typedef struct {
    uint64_t old_epoch;
    uint64_t new_epoch;
    uint64_t before_us;
    uint64_t after_us;
    bool waited;
} nx_stm32_stop_receipt_t;

/** \brief A deadline translated using an independent elapsed-time envelope.
 */
typedef struct {
    uint64_t epoch;
    uint64_t earliest_us;
    uint64_t latest_us;
} nx_stm32_deadline_bridge_t;

/**
 * \brief Enter STOP or WKUP-only Standby after explicit consumer drain.
 * \param[in] barrier: Current closed, empty domain enrolling every clock and
 *                    deadline borrower, including external OS consumers.
 * \param[in] options: Stable call-owned options and exact enabled wake IRQs.
 * \param[out] receipt: Distinct call-owned output, never retained.
 * \return SUCCESS after STOP restoration; BUSY/STATE/CONTEXT before effects;
 *         IO if Standby WFI returns and a new RUN epoch is restored.
 * \note Privileged serialized Thread, incoming masks clear. The owner joins
 *       devices and DMA, disables SysTick and suspends/rebuilds OS timing.
 *       An assembled Platform must successfully suspend first. A disabled
 *       hardware gate does not prove that logical deadline borrowers drained.
 *       STOP wake_events selects EXTI17/21/22 EMR/RTSR and one WFE; otherwise
 *       exact enabled wake_irqs select WFI. Standby selects WFI; an enabled RTC
 *       domain requires standby_enter and its prepared RTC owner receipt.
 *       Spurious returns are allowed. No SEV/WFE sequence discards an event.
 *       Hardware additionally rejects live DMA, peripheral gates, old pending
 *       SysTick and undeclared enabled IRQs. STOP clears no pending flags.
 *       Standby disables EWUP before CWUF and rearms the selected WKUP source;
 *       existing WUF requires discard_wakeup. Old deadlines end with old_epoch.
 *       No kernel tick or old deadline is resumed. VBAT policy is external.
 *       An unrecoverable restore keeps masks/owners, observes and parks.
 *       Real Standby resets and does not return; SRAM/CCM retention is absent.
 */
nx_result_t nx_stm32f407_power_enter(nx_clock_barrier_t barrier,
                                     const nx_stm32_power_options_t* options,
                                     nx_stm32_stop_receipt_t* receipt);

/**
 * \brief Request Standby using the unique RTC owner's prepared wake receipt.
 * \param[in] barrier: Closed, drained domain as for power_enter.
 * \param[in] options: Stable options with standby=true; discard_wakeup alone
 *       authorizes acknowledging an already pending raw PWR WUF.
 * \param[in] rtc: Existing suspended RTC face; never a second hardware owner.
 * \param[in,out] rtc_receipt: Exact successful RTC standby_prepare receipt.
 * \param[out] receipt: Distinct timer epoch receipt, including spurious return.
 * \return IO on returning Standby, or a pre-entry/restore failure. Real
 *       Standby resets and never returns. A failed restore retains its receipt.
 * \note Privileged serialized Thread, incoming masks clear. RTC prepare
 *       explicitly authorizes any business-event discard. This call validates
 *       disabled RTC sources, disables EWUP, clears WUF, commits RTC sources
 *       and rearms EWUP before WFI. It never writes RTC or EXTI registers.
 *       Spurious return restores RUN and RTC ownership without clearing new
 *       wake events. No pending business event is implicitly consumed.
 */
nx_result_t nx_stm32f407_standby_enter(nx_clock_barrier_t barrier,
                                       const nx_stm32_power_options_t* options,
                                       const nx_rtc_port_t* rtc,
                                       nx_rtc_standby_t* rtc_receipt,
                                       nx_stm32_stop_receipt_t* receipt);

/**
 * \brief Translate a deadline across the exact recorded old/new epochs.
 * \param[in] receipt: Successful STOP receipt from the same retained boot.
 * \param[in] old_epoch: Explicit epoch of deadline_us, must match receipt.
 * \param[in] deadline_us: Old-domain absolute deadline, including UINT64_MAX.
 * \param[in] elapsed_min_us: Independent source's lower elapsed-time bound.
 * \param[in] elapsed_max_us: Independent source's upper elapsed-time bound.
 * \param[out] output: Distinct call-owned bounds in the new timer domain.
 * \return SUCCESS or INVALID/STATE/OVERFLOW without modifying output.
 * \note Pure bounded arithmetic. The caller samples/qualifies the independent
 *       RTC and accounts for sample windows, drift and unavailable intervals.
 *       The result is an envelope, not permission to restart OS/request state.
 */
nx_result_t nx_stm32f407_deadline_bridge(const nx_stm32_stop_receipt_t* receipt,
                                         uint64_t old_epoch,
                                         uint64_t deadline_us,
                                         uint64_t elapsed_min_us,
                                         uint64_t elapsed_max_us,
                                         nx_stm32_deadline_bridge_t* output);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_STM32F407_POWER_H */
