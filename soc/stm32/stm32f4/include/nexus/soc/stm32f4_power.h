/**
 * \file            stm32f4_power.h
 * \brief           Explicit drained STOP/Standby and time-domain receipts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F4_POWER_H
#define NEXUS_STM32F4_POWER_H
#include "nexus/components/clock_domain.h"
#include "nexus/io/rtc_aux.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Explicit regulator, wake routes and authorization to discard WUF.
 * \note wake_pins temporarily arms explicitly selected physical wake pins.
 *       The caller owns their pad/electrical plan and every pre-enabled wake
 *       source. Original enable bits survive a spurious Standby return.
 *       Unimplemented IRQ words/bits must be zero. F446 PC13 conflicts with
 *       crystal LSE on the maintained LQFP package; no source is substituted.
 *       discard_wakeup permits clearing existing PWR WUF only for Standby.
 */
typedef struct {
    uint32_t wake_irqs[4];
    uint32_t poll_limit;
    uint32_t wake_events;
    uint8_t wake_pins;
    bool low_power_regulator;
    bool standby;
    bool discard_wakeup;
} nx_stm32_power_options_t;

#define NX_STM32_WAKE_PIN_PA0  UINT8_C(1)
#define NX_STM32_WAKE_PIN_PC13 UINT8_C(2)

#define NX_STM32_WAKE_RTC_ALARM  (UINT32_C(1) << 17U)
#define NX_STM32_WAKE_RTC_TAMPER (UINT32_C(1) << 21U)
#define NX_STM32_WAKE_RTC_TIMER  (UINT32_C(1) << 22U)

/** \brief Read-only retained PWR observations, not a physical wake-cause proof.
 * \note F479 conservatively requires a product-authorized reset before RTC
 *       Standby rearm when Standby/wake flags remain. WKUP-only is unaffected.
 */
typedef struct {
    bool standby_observed;
    bool wake_pending;
    bool rtc_reset_required;
} nx_stm32_power_wake_status_t;

/** \brief Observe live retained flags without acknowledging or clearing them.
 * \param[out] status: Separate caller-owned output, never retained.
 * \return SUCCESS or INVALID/CONTEXT/STATE, output unchanged on rejection.
 * \note Privileged unmasked Thread with the selected System/PWR owner live.
 */
nx_result_t nx_stm32_power_wake_status(nx_stm32_power_wake_status_t* status);

/** \brief Prepare the explicit F479 RTC-Standby reset workaround.
 * \param[in] poll_limit: Positive bound for observing SBF acknowledgment.
 * \return SUCCESS after SBF is cleared; IO on refused acknowledgment;
 *         INVALID/CONTEXT/STATE or UNSUPPORTED on other maintained parts.
 * \note Product-authorized privileged unmasked Thread with live System/PWR.
 *       Clears only SBF, leaving WUF, RTC and other retained state intact.
 *       SUCCESS obliges the caller to perform its authorized system reset;
 *       it never means RTC Standby is rearmed. This function never resets.
 */
nx_result_t nx_stm32_power_standby_reset_prepare(uint32_t poll_limit);

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
 *         UNSUPPORTED for a RUN-only profile; IO if Standby WFI returns and
 *         a new RUN epoch is restored.
 * \note Privileged serialized Thread, incoming masks clear. The owner joins
 *       devices and DMA, disables SysTick and suspends/rebuilds OS timing.
 *       The closed 150 MHz RUN/SDIO high-speed profile rejects deep sleep
 *       without hardware writes, epoch changes or retained references.
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
nx_result_t nx_stm32_power_enter(nx_clock_barrier_t barrier,
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
nx_result_t nx_stm32_standby_enter(nx_clock_barrier_t barrier,
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
nx_result_t nx_stm32_deadline_bridge(const nx_stm32_stop_receipt_t* receipt,
                                     uint64_t old_epoch, uint64_t deadline_us,
                                     uint64_t elapsed_min_us,
                                     uint64_t elapsed_max_us,
                                     nx_stm32_deadline_bridge_t* output);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_STM32F4_POWER_H */
