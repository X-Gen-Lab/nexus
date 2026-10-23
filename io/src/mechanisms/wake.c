/**
 * \file            wake.c
 *
 * \brief           Explicit IRQ hint delivery and unshifted priority validation
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/wake.h"

/** \brief Match NVIC numerical urgency to the optional kernel syscall ceiling.
 */
nx_result_t nx_irq_wake_validate(const nx_irq_wake_t* wake,
                                 const nx_irq_policy_t* policy,
                                 const nx_irq_source_t* source) {
    if (policy == NULL || source == NULL || policy->priority_bits == 0U ||
        policy->priority_bits > 8U || policy->external_irq_count == 0U ||
        policy->external_irq_count > 480U || source->irq_number < 0 ||
        (uint16_t)source->irq_number >= policy->external_irq_count ||
        source->priority_group > 7U || (wake != NULL && wake->notify == NULL)) {
        return NX_ERROR_INVALID;
    }
    uint16_t levels = (uint16_t)(1U << policy->priority_bits);
    if (source->priority >= levels || policy->syscall_ceiling >= levels) {
        return NX_ERROR_INVALID;
    }
    switch (policy->kernel) {
        case NX_IRQ_KERNEL_NONE:
            if (policy->syscall_ceiling != 0U) {
                return NX_ERROR_INVALID;
            }
            return wake != NULL && wake->calls_kernel ? NX_ERROR_PERMISSION
                                                      : NX_SUCCESS;
        case NX_IRQ_KERNEL_PRIMASK:
            if (policy->syscall_ceiling != 0U || source->priority_group != 0U) {
                return NX_ERROR_INVALID;
            }
            break;
        case NX_IRQ_KERNEL_BASEPRI:
            if (policy->syscall_ceiling == 0U) {
                return NX_ERROR_INVALID;
            }
            if (wake != NULL && wake->calls_kernel) {
                uint8_t maximum_group =
                    policy->priority_bits < 7U
                        ? (uint8_t)(7U - policy->priority_bits)
                        : 0U;
                if (source->priority_group > maximum_group ||
                    source->priority < policy->syscall_ceiling) {
                    return NX_ERROR_PERMISSION;
                }
            }
            break;
        default:
            return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}

/** \brief The caller latches hardware facts before sending this optional hint.
 */
nx_result_t nx_irq_wake_signal(const nx_irq_wake_t* wake) {
    if (wake == NULL) {
        return NX_SUCCESS;
    }
    if (wake->notify == NULL) {
        return NX_ERROR_INVALID;
    }
    return wake->notify(wake->context);
}
