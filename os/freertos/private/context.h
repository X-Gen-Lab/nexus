/**
 * \file            context.h
 * \brief           Shared privileged FreeRTOS task admission
 * \author          Nexus Team
 */
#ifndef NEXUS_FREERTOS_PRIVATE_CONTEXT_H
#define NEXUS_FREERTOS_PRIVATE_CONTEXT_H
#include "FreeRTOS.h"
#include "nexus/arch/arch.h"
#include "task.h"

#ifndef NEXUS_CPU_HAS_BASEPRI
#if defined(__arm__) || defined(__thumb__)
#error "FreeRTOS requires validated CPU interrupt-mask facts"
#else
#define NEXUS_CPU_HAS_BASEPRI 1
#endif
#endif

/** \brief Reject unsafe contexts before kernel entry or object mutation. */
static bool nx_freertos_task_context_allowed(void) {
    if (nx_arch_in_isr() || !nx_arch_is_privileged()) {
        return false;
    }
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (masks.faultmask != 0) {
        return false;
    }
#if NEXUS_CPU_HAS_BASEPRI
    if (masks.primask != 0) {
        return false;
    }
    if (masks.basepri == 0) {
        return true;
    }
    /* Mainline startup can retain only the port's exact syscall mask. The
     * numeric value cannot establish ownership of that mask. */
    return masks.basepri == configMAX_SYSCALL_INTERRUPT_PRIORITY &&
           xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED;
#else
    if (masks.basepri != 0 || masks.primask > 1) {
        return false;
    }
    return masks.primask == 0 ||
           xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED;
#endif
}
#endif /* NEXUS_FREERTOS_PRIVATE_CONTEXT_H */
