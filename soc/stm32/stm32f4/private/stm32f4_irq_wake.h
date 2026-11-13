/**
 * \file            stm32f4_irq_wake.h
 * \brief           Private exact STM32F4 publisher wake admission
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_IRQ_WAKE_H
#define NX_STM32F4_IRQ_WAKE_H

#include "nexus/io/wake.h"
#include "stm32f4xx.h"
#if !defined(NEXUS_STM32_MODEL)
#include "nexus_config.h"
#endif

/** \brief Check the actual publisher before borrowing any notification target.
 */
static inline nx_result_t nx_stm32_irq_wake_validate(const nx_irq_wake_t* wake,
                                                     int irq,
                                                     uint8_t syscall_ceiling) {
#if defined(NEXUS_STM32_MODEL)
    nx_irq_policy_t policy = {NX_IRQ_KERNEL_BASEPRI, 82U, 4U, 5U};
#else
    nx_irq_policy_t policy = {
        (nx_irq_kernel_policy_t)NEXUS_IRQ_KERNEL_POLICY,
        NEXUS_CPU_EXTERNAL_IRQ_COUNT,
        NEXUS_IRQ_PRIORITY_BITS,
#if NEXUS_IRQ_KERNEL_POLICY == 2
        NEXUS_IRQ_SYSCALL_PRIORITY
#else
        0U
#endif
    };
#endif
    if (irq < 0 || (unsigned)irq >= policy.external_irq_count) {
        return NX_ERROR_INVALID;
    }
    if (wake != NULL && wake->calls_kernel) {
        if (policy.kernel == NX_IRQ_KERNEL_PRIMASK && syscall_ceiling != 0U) {
            return NX_ERROR_INVALID;
        }
        if (policy.kernel == NX_IRQ_KERNEL_BASEPRI &&
            syscall_ceiling > policy.syscall_ceiling) {
            policy.syscall_ceiling = syscall_ceiling;
        }
    }
    const nx_irq_source_t source = {
        (int16_t)irq,
#if defined(NEXUS_STM32_MODEL)
        5U,
        0U
#else
        (uint8_t)NVIC_GetPriority((IRQn_Type)irq),
        (uint8_t)NVIC_GetPriorityGrouping()
#endif
    };
    return nx_irq_wake_validate(wake, &policy, &source);
}

#endif
