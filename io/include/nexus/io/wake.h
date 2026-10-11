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

/** \brief Explicit kernel interrupt-mask policy, independent of any RTOS. */
typedef enum {
    NX_IRQ_KERNEL_NONE = 0,
    NX_IRQ_KERNEL_PRIMASK = 1,
    NX_IRQ_KERNEL_BASEPRI = 2
} nx_irq_kernel_policy_t;

/**
 * \brief           Immutable CPU and kernel facts for cold wake binding.
 *
 * \note            NONE and PRIMASK require a zero syscall ceiling. BASEPRI
 *                  requires a nonzero unshifted ceiling. Configuration owns
 *                  these facts; a wake target cannot weaken them.
 */
typedef struct {
    nx_irq_kernel_policy_t kernel;
    uint16_t external_irq_count;
    uint8_t priority_bits;
    uint8_t syscall_ceiling;
} nx_irq_policy_t;

/** \brief Actual publisher facts read before attaching any borrowed sink. */
typedef struct {
    int16_t irq_number;
    uint8_t priority;
    uint8_t priority_group;
} nx_irq_source_t;

/**
 * \brief           Validate actual IRQ facts against an explicit kernel policy.
 *
 * \param[in]       wake: Optional immutable target; NULL disables notification.
 *
 * \param[in]       policy: Immutable validated CPU and kernel configuration.
 *
 * \param[in]       source: External IRQ number, unshifted priority and group.
 *
 * \return          Success, INVALID for malformed facts/target, PERMISSION
 *                  for a kernel-calling target in an unsafe interrupt domain.
 *
 * \note            Cold check without attachment or callback effects. No
 *                  kernel target is accepted by NONE. PRIMASK permits every
 *                  valid configurable IRQ with its fixed zero priority group.
 *                  Kernel-calling BASEPRI targets require the maintained
 *                  preemption grouping and a numerical priority at least the
 *                  ceiling. Nonkernel hints do not impose kernel grouping.
 *                  Binding does not authorize future CPU-context violations.
 */
nx_result_t nx_irq_wake_validate(const nx_irq_wake_t* wake,
                                 const nx_irq_policy_t* policy,
                                 const nx_irq_source_t* source);

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
