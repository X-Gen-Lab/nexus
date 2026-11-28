/**
 * \file            hardware.h
 * \brief           Private F1 cold ownership and checked MMIO primitives.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_HARDWARE_H
#define NEXUS_STM32F1_HARDWARE_H

#include "nexus/arch/arch.h"
#include "stm32f1xx.h"

#ifndef NX_STM32F1_WRITE
#define NX_STM32F1_WRITE(reg, value) (*(reg) = (value))
#endif

/** \brief Cold ownership is serialized by Platform in privileged Thread. */
static inline bool nx_stm32f1_cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Set an owned R/W field without changing its neighbours.
 * \note W1C, W0C and write-only registers require their own write contract.
 */
static inline bool nx_stm32f1_write_masked(volatile uint32_t* reg,
                                           uint32_t mask, uint32_t value) {
    NX_STM32F1_WRITE(reg, (*reg & ~mask) | (value & mask));
    nx_arch_dsb();
    return (*reg & mask) == (value & mask);
}
#endif
