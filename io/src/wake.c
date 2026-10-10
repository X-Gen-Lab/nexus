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
nx_result_t nx_irq_wake_validate(const nx_irq_wake_t* wake, uint8_t priority,
                                 uint8_t priority_bits,
                                 uint8_t syscall_ceiling) {
    if (priority_bits == 0U || priority_bits > 8U) {
        return NX_ERROR_INVALID;
    }
    uint16_t levels = (uint16_t)(1U << priority_bits);
    if (priority >= levels || syscall_ceiling >= levels ||
        (wake != NULL && wake->notify == NULL)) {
        return NX_ERROR_INVALID;
    }
    if (wake != NULL && wake->calls_kernel &&
        (syscall_ceiling == 0U || priority < syscall_ceiling)) {
        return NX_ERROR_PERMISSION;
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
