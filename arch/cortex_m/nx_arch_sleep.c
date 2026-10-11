/**
 * \file            nx_arch_sleep.c
 * \brief           Atomic masked readiness check and shallow WFI
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/arch/sleep.h"
#include "private/sleep.h"
#include <stddef.h>

nx_arch_result_t nx_arch_wait_for_interrupt(void) {
#if defined(__arm__) || defined(__thumb__) || defined(NEXUS_ARCH_SLEEP_MODEL)
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0) {
        return NX_ARCH_CONTEXT;
    }
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (masks.primask != 1u || masks.basepri != 0 || masks.faultmask != 0) {
        return NX_ARCH_CONTEXT;
    }
    if ((nx_arch_sleep_control() & (1u << 2)) != 0) {
        return NX_ARCH_UNSUPPORTED;
    }
    /* PRIMASK keeps a configurable IRQ from running between the caller's
     * final check and WFI. A pending enabled IRQ still wakes WFI; restoring
     * PRIMASK lets that IRQ publish its state before the next observation. */
    nx_arch_dsb();
    nx_arch_sleep_instruction();
    nx_arch_isb();
    return NX_ARCH_OK;
#else
    return NX_ARCH_UNSUPPORTED;
#endif
}

nx_arch_result_t nx_arch_idle_if_unchanged(const uint32_t* sequence,
                                           uint32_t expected, bool* slept) {
    if (sequence == NULL || slept == NULL ||
        (uintptr_t)sequence % sizeof(uint32_t) != 0) {
        return NX_ARCH_INVALID;
    }
    *slept = false;
#if defined(__arm__) || defined(__thumb__) || defined(NEXUS_ARCH_SLEEP_MODEL)
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0) {
        return NX_ARCH_CONTEXT;
    }
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (masks.primask != 0 || masks.basepri != 0 || masks.faultmask != 0) {
        return NX_ARCH_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_arch_result_t result = NX_ARCH_OK;
    if (__atomic_load_n(sequence, __ATOMIC_ACQUIRE) == expected) {
        result = nx_arch_wait_for_interrupt();
        *slept = result == NX_ARCH_OK;
    }
    nx_arch_irq_restore(saved);
    return result;
#else
    (void)expected;
    return NX_ARCH_UNSUPPORTED;
#endif
}
