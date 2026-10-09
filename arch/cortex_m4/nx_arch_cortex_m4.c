/**
 * \file            nx_arch_cortex_m4.c
 *
 * \brief           ARMv7E-M primitives without CMSIS, vendor or kernel
 *                  dependencies.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"

#if !defined(__ARM_ARCH_7EM__) || !(defined(__GNUC__) || defined(__clang__))
#error "Nexus Cortex-M4 port requires ARMv7E-M and GCC or Clang"
#endif

nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous;
    __asm__ volatile("mrs %0, primask\n\tcpsid i\n\tdsb\n\tisb"
                     : "=r"(previous.value)::"memory");
    return previous;
}

void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    __asm__ volatile("dsb\n\tmsr primask, %0\n\tisb" ::"r"(previous.value)
                     : "memory");
}

bool nx_arch_irq_is_masked(void) {
    uint32_t primask, basepri, faultmask;
    __asm__ volatile("mrs %0, primask\n\tmrs %1, basepri\n\tmrs %2, faultmask"
                     : "=r"(primask), "=r"(basepri), "=r"(faultmask)::"memory");
    /* A priority mask can block the tick or peripheral IRQ required by a
     * blocking driver. Conservatively reject any mask, including BASEPRI.
     * Save/restore deliberately remains PRIMASK-only. */
    return (primask & 1u) != 0 || basepri != 0 || (faultmask & 1u) != 0;
}

bool nx_arch_in_isr(void) {
    uint32_t ipsr;
    __asm__ volatile("mrs %0, ipsr" : "=r"(ipsr)::"memory");
    return ipsr != 0;
}

void nx_arch_dmb(void) {
    __asm__ volatile("dmb" ::: "memory");
}
void nx_arch_dsb(void) {
    __asm__ volatile("dsb" ::: "memory");
}
void nx_arch_isb(void) {
    __asm__ volatile("isb" ::: "memory");
}

/** \brief Read enabled DWT without resetting another user's counter domain. */
bool nx_arch_cycle_snapshot(uint32_t* cycles) {
    if (cycles == 0) {
        return false;
    }
    volatile const uint32_t* demcr = (const uint32_t*)0xe000edfcu;
    volatile const uint32_t* control = (const uint32_t*)0xe0001000u;
    volatile const uint32_t* counter = (const uint32_t*)0xe0001004u;
    if ((*demcr & (1u << 24)) == 0 || (*control & 1u) == 0 ||
        (*control & (1u << 25)) != 0) {
        return false;
    }
    *cycles = *counter;
    return true;
}
