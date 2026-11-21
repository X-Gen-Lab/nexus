/**
 * \file            threadx.h
 * \brief           Explicit ThreadX fault, Tick and IRQ inspection extensions
 * \author          Nexus Team
 * \note            Common OS operations use the public Nexus headers.
 *                  Static layouts belong in nexus/os/storage.h. These hooks
 *                  remain backend-specific.
 */
#ifndef NEXUS_OS_THREADX_H
#define NEXUS_OS_THREADX_H
#include "nexus/core/status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Product failure hook; the default retains effects and parks
 * \param[in]       reason: Context, permanent-entry or kernel-return failure
 * \note            Actual fault context is not repaired or normalized; it may
 *                  be masked, ISR or unprivileged. Do not assume a working
 *                  kernel, clock, stack or permission to access a peripheral.
 *                  The adapter observes Core faults before invoking this hook.
 *                  Returning does not authorize cleanup; the adapter parks.
 */
void nx_threadx_fail_stop(nx_result_t reason);
/**
 * \brief           Product hook required only for an explicit external Tick
 * \param[in]       tick_hz: Exact immutable resolved kernel frequency
 * \return          SUCCESS after binding a bounded real periodic IRQ source
 * \note            Actual kernel bootstrap, privileged with PRIMASK masked.
 *                  The provider owns its timer and IRQ ceiling; it must not
 *                  change the continuous Core deadline clock. A missing hook
 *                  fails linking. Failure retains effects in fail-stop.
 */
nx_result_t nx_threadx_external_tick_setup(uint32_t tick_hz);
/**
 * \brief           Inspect the reviewed IRQ and selected kernel mask policy
 * \return          True only in an admitted privileged unmasked external IRQ
 * \note            Checks actual IRQ range, grouping and syscall ceiling when
 *                  applicable. The Linux port has no physical IRQ source and
 *                  returns false. This read-only check grants no ownership.
 */
bool nx_threadx_isr_allowed(void);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_OS_THREADX_H */
