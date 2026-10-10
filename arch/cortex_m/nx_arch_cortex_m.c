/**
 * \file            nx_arch_cortex_m.c
 *
 * \brief           Compile-time Cortex-M primitives without vendor or kernel
 *                  dependencies.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "private/compiler.h"

nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous = {nx_arch_hw_irq_save()};
    return previous;
}

void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    nx_arch_hw_irq_restore(previous.value);
}

nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    nx_arch_irq_masks_t masks = {nx_arch_hw_primask(), 0, 0};
#if NX_ARCH_CORTEX_M_PRIORITY_MASK
    masks.basepri = nx_arch_hw_basepri();
    masks.faultmask = nx_arch_hw_faultmask();
#endif
    return masks;
}

bool nx_arch_irq_is_masked(void) {
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    /* A priority mask can block completion or the tick. Save/restore remains
     * PRIMASK-only and never changes an incoming kernel priority ceiling. */
    return (masks.primask & 1u) != 0 || masks.basepri != 0 ||
           (masks.faultmask & 1u) != 0;
}

uint32_t nx_arch_exception_number(void) {
    return nx_arch_hw_ipsr();
}

bool nx_arch_is_privileged(void) {
    /* CONTROL.nPRIV describes Thread mode, including when an exception
     * interrupted an unprivileged Thread. Handler mode is privileged. */
    return nx_arch_exception_number() != 0 || (nx_arch_hw_control() & 1u) == 0;
}

bool nx_arch_in_isr(void) {
    return nx_arch_exception_number() != 0;
}

void nx_arch_dmb(void) {
    nx_arch_hw_dmb();
}
void nx_arch_dsb(void) {
    nx_arch_hw_dsb();
}
void nx_arch_isb(void) {
    nx_arch_hw_isb();
}

/** \brief Read reviewed, enabled DWT without changing its counter domain. */
bool nx_arch_cycle_snapshot(uint32_t* cycles) {
#if NEXUS_ARCH_HAS_DWT_CYCCNT
    if (cycles == 0 || !nx_arch_is_privileged()) {
        return false;
    }
    if ((nx_arch_hw_demcr() & (1u << 24)) == 0) {
        return false;
    }
    uint32_t control = nx_arch_hw_dwt_control();
    if ((control & 1u) == 0 || (control & (1u << 25)) != 0) {
        return false;
    }
    *cycles = nx_arch_hw_cyccnt();
    return true;
#else
    (void)cycles;
    return false;
#endif
}
