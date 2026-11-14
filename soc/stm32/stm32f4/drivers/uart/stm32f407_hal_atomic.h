/**
 * \file            stm32f407_hal_atomic.h
 * \brief           Bounded owned-CPU atomic port for the selected F407 HAL
 * \author          Nexus Team
 * \note            Exact single-CPU owner only. UART CR1/CR3 have no NMI,
 *                  HardFault/panic logger, other security-domain or DMA writer.
 *                  Cold admission proves naturally aligned word addresses.
 *                  This target-private port leaves vendor source unchanged.
 */
#ifndef NEXUS_STM32F407_HAL_ATOMIC_H
#define NEXUS_STM32F407_HAL_ATOMIC_H
#include "nexus/arch/arch.h"
#include "stm32f4xx.h"

/** \brief Guard one owned word update and restore the incoming interrupt mask.
 */
static inline void nx_stm32_hal_atomic_set(volatile uint32_t* reg,
                                           uint32_t bits) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    *reg |= bits;
    nx_arch_irq_restore(saved);
}

static inline void nx_stm32_hal_atomic_clear(volatile uint32_t* reg,
                                             uint32_t bits) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    *reg &= ~bits;
    nx_arch_irq_restore(saved);
}

static inline void nx_stm32_hal_atomic_modify(volatile uint32_t* reg,
                                              uint32_t clear, uint32_t set) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    *reg = (*reg & ~clear) | set;
    nx_arch_irq_restore(saved);
}

/* Function arguments evaluate the register lvalue and each mask once. These
 * macros are installed after CMSIS for this owned HAL compile target only. */
#undef ATOMIC_SET_BIT
#undef ATOMIC_CLEAR_BIT
#undef ATOMIC_MODIFY_REG
#define ATOMIC_SET_BIT(reg, bits)                                              \
    nx_stm32_hal_atomic_set(&(reg), (uint32_t)(bits))
#define ATOMIC_CLEAR_BIT(reg, bits)                                            \
    nx_stm32_hal_atomic_clear(&(reg), (uint32_t)(bits))
#define ATOMIC_MODIFY_REG(reg, clear, set)                                     \
    nx_stm32_hal_atomic_modify(&(reg), (uint32_t)(clear), (uint32_t)(set))
#endif
