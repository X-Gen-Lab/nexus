/**
 * \file            wake.h
 *
 * \brief           Explicit borrowed IRQ wake target and syscall-ceiling check
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_WAKE_H
#define NEXUS_IO_WAKE_H

#include "nexus/core/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Optional immutable wake target borrowed by an IRQ provider.
 *
 * \note            notify runs in publisher context and must be bounded and
 *                  legal in that IRQ. Storage and context outlive all
 *                  publishers. Stop and join publishers before destroying the
 *                  sink. A wake is only a hint; latched RX/request predicates
 *                  stay authoritative.
 */
typedef struct {
    void* context;
    nx_result_t (*notify)(void* context);
    bool calls_kernel;
} nx_irq_wake_t;

/**
 * \brief           Reject wake targets that would call a kernel above its
 *                  ceiling.
 *
 * \param[in]       wake: Optional immutable target; NULL disables notification.
 *
 * \param[in]       priority: Unshifted NVIC priority of the actual publisher
 *                  IRQ.
 *
 * \param[in]       priority_bits: Implemented NVIC priority width, one through
 *                  eight.
 *
 * \param[in]       syscall_ceiling: Unshifted minimum kernel-safe IRQ priority.
 *
 * \return          Success, INVALID for malformed priorities/target, PERMISSION
 *                  for a kernel-calling target at an unsafe IRQ priority.
 *
 * \note            Cold check without attachment or callback effects.
 *                  Numerically smaller priorities are more urgent on maintained
 *                  Cortex-M.
 */
nx_result_t nx_irq_wake_validate(const nx_irq_wake_t* wake, uint8_t priority,
                                 uint8_t priority_bits,
                                 uint8_t syscall_ceiling);

/**
 * \brief           Deliver one bounded hint after authoritative state is
 *                  latched.
 *
 * \param[in]       wake: Previously validated live target or NULL.
 *
 * \return          Callback result, success for NULL, INVALID for a bad target.
 *
 * \note            Task/IRQ as allowed by the target, no hidden task or retry.
 *                  A failed wake does not revoke state or request ownership.
 */
nx_result_t nx_irq_wake_signal(const nx_irq_wake_t* wake);

#ifdef __cplusplus
}
#endif

#endif
