/**
 * \file            system.h
 * \brief           GD32F470 private startup, clock and monotonic resources
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32_NEXTGEN_SYSTEM_H
#define NEXUS_GD32_NEXTGEN_SYSTEM_H

#include "nexus/arch/arch.h"
#include "nexus/core/time.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * \brief           Start the reviewed 25 MHz HXTAL / 200 MHz clock profile.
 * \return          Zero on success; negative on a diagnosed partial failure.
 * \note            Task startup only. Failures retain ownership until stop.
 */
int nx_gd32_soc_start(void);
/**
 * \brief           Read-only reset-like clock and reserved timer admission.
 * \return          SUCCESS or CONTEXT/BUSY before any global clock writes.
 * \note            No external IO, scheduler or DMA owners may be live. GPIO
 *                  and selected extra-memory gates are not controller owners.
 *                  Unknown running profiles and bootloader handoff are
 * rejected.
 */
nx_result_t nx_gd32_soc_preflight(void);
/**
 * \brief           Stop after external IO owners and interrupts are quiescent.
 * \return          Zero when clocks are released; negative if effects remain.
 * \note            A running scheduler is unsupported. The defensive SysTick
 *                  CTRL read clears COUNTFLAG; it does not promise a strict
 *                  no-effect rejection for a violated consumer-join contract.
 */
int nx_gd32_soc_stop(void);
/**
 * \brief           Read the 1 MHz TIMER1 clock after successful start.
 *
 * \return          Microseconds in the fixed boot clock domain.
 *
 * \note            Privileged task/configurable IRQ only, never NMI/HardFault.
 *                  Returns zero before successful start, after failed start or
 *                  after withdrawal or observed overflow-ACK failure. Zero
 *                  does not prove readiness;
 *                  the caller observes successful platform startup first.
 *                  Overflow IRQ must run at least once per 2^32 us.
 *                  Stop/restart creates a new epoch; outstanding deadlines and
 *                  borrowers must already be joined before clock withdrawal.
 *                  Clock changes and deep suspend are unsupported.
 */
uint64_t nx_gd32_now_us(void);
/**
 * \brief           Read the latched runtime overflow acknowledgement fault.
 * \return          SUCCESS or IO, independent of startup readiness.
 * \note            Stop preserves this diagnostic; a newly admitted start
 *                  resets it independently of subsequent startup readiness.
 *                  Failure invalidates old timestamps and deadlines, retains
 *                  timer/clock ownership and attempts DIER/CEN/NVIC isolation
 *                  once. Confirmed isolation permits ordinary task stop retry;
 *                  optional low-power owners retain their busy/drain contract.
 *                  Unconfirmed isolation keeps the IRQ mask, observes the
 *                  terminal PLATFORM/IO fault and parks. No old deadline or
 *                  elapsed-time continuity is recovered by stop/restart.
 */
nx_result_t nx_gd32_timebase_error(void);
/**
 * \brief           Save and mask interrupts preserving the incoming PRIMASK.
 * \return          Incoming mask to restore with nx_gd32_critical_leave.
 */
static inline uint32_t nx_gd32_critical_enter(void) {
    return nx_arch_irq_save().value;
}
/**
 * \brief           Restore the incoming interrupt mask.
 * \param[in]       mask: Value returned by nx_gd32_critical_enter.
 */
static inline void nx_gd32_critical_leave(uint32_t mask) {
    nx_arch_irq_state_t previous = {mask};
    nx_arch_irq_restore(previous);
}
/**
 * \brief           Report exception context.
 * \return          True in an exception or interrupt handler.
 */
static inline bool nx_gd32_in_isr(void) {
    return nx_arch_in_isr();
}
/**
 * \brief           Inspect incoming PRIMASK, BASEPRI and FAULTMASK.
 * \return          True when interrupts may be masked by the caller.
 */
static inline bool nx_gd32_irq_masked(void) {
    return nx_arch_irq_is_masked();
}
/**
 * \brief           Finish peripheral writes before a settlement publication.
 */
static inline void nx_gd32_peripheral_barrier(void) {
    nx_arch_dsb();
    nx_arch_isb();
}

/**
 * \brief           Check the reset-like IRC16M bootstrap without writes.
 * \return          SUCCESS or CONTEXT/BUSY for unavailable clock ownership.
 * \note            Requires unused PLL/HXTAL, default bus dividers and idle
 * FMC.
 */
nx_result_t nx_gd32_clock_preflight(void);
/** \brief Configure the fixed 25 MHz crystal profile; task startup only. */
int nx_gd32_clock_start(void);
/**
 * \brief           Return to IRC16M then restore acquired PLL/Flash/PMU fields.
 * \return          Zero only after owned fields/gates are restored and
 * observed. \note            Unrelated PMU fields and pre-existing interface
 * clocks are preserved. Failed source switching retains the live PLL; failed
 * restoration retains cleanup responsibility.
 */
int nx_gd32_clock_stop(void);
/** \brief Fixed TIMER1 overflow vector; owned solely by the monotonic clock. */
void TIMER1_IRQHandler(void);
#ifdef NX_GD32_CLOCK_MODEL
/** \brief Advance host clock phases; absent from production ARM compilation. */
void nx_gd32_model_clock_poll(void);
#endif
#endif
