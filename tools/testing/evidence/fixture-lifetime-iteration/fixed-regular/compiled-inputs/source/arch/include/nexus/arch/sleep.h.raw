/**
 * \file            sleep.h
 * \brief           Reviewed single-CPU shallow sleep contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_ARCH_SLEEP_H
#define NEXUS_ARCH_SLEEP_H
#include "nexus/arch/features.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Wait once with configurable exceptions already masked.
 * \return          OK after wake, CONTEXT or UNSUPPORTED without sleeping.
 * \note            Privileged Thread mode, incoming PRIMASK=1, BASEPRI and
 *                  FAULTMASK clear. SCR.SLEEPDEEP must be clear. The caller
 *                  owns the final readiness check and timer/wake source. WFI
 *                  wakes for a pending enabled interrupt despite PRIMASK; the
 *                  interrupt runs after the caller restores its saved mask.
 *                  No register, mask, clock or pending interrupt is changed.
 *                  Current CPU/security state only; no physical HIL claim.
 */
nx_arch_result_t nx_arch_wait_for_interrupt(void);
/**
 * \brief           Atomically recheck a published sequence before WFI.
 * \param[in]       sequence: Naturally aligned, shared 32-bit wake sequence.
 * \param[in]       expected: Snapshot before the caller's readiness check.
 * \param[out]      slept: True only when WFI was executed.
 * \return          OK, INVALID, CONTEXT or UNSUPPORTED.
 * \note            Privileged Thread mode with all incoming masks clear.
 *                  Saves PRIMASK, performs acquire observation under that
 *                  mask, optionally sleeps, and restores the exact mask.
 *                  Publishers must be same-CPU configurable IRQs or Threads;
 *                  NMI/HardFault, DMA, SMP and other security-world publishers
 *                  are excluded. The caller keeps storage alive, owns an
 *                  enabled IRQ timer for finite deadlines, and retries the
 *                  authoritative predicate after wake. Native is unsupported.
 */
nx_arch_result_t nx_arch_idle_if_unchanged(const uint32_t* sequence,
                                           uint32_t expected, bool* slept);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_ARCH_SLEEP_H */
