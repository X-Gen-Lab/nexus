/**
 * \file            crypto_irq.c
 * \brief           Static crypto vector acquisition and cold withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "stm32f4_crypto_irq.h"
#include "stm32f4xx.h"

/** \brief Cold controls run under the same serialized Platform task. */
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Static receipts cannot alias their own hardware publisher state. */
static bool valid(const nx_stm32f4_crypto_irq_t* receipt, uint16_t irq) {
    uintptr_t p = (uintptr_t)receipt;
    uintptr_t nvic = (uintptr_t)NVIC;
    return receipt != NULL && p % _Alignof(nx_stm32f4_crypto_irq_t) == 0U &&
           sizeof(*receipt) <= UINTPTR_MAX - p &&
           (p >= nvic + sizeof(*NVIC) || p + sizeof(*receipt) <= nvic) &&
           (irq == CRYP_IRQn || irq == HASH_RNG_IRQn);
}
/** \brief Enabled, pending and active are separate ownership observations. */
static bool quiet(uint16_t irq) {
    unsigned bank = irq / 32U;
    uint32_t mask = UINT32_C(1) << (irq % 32U);
    return ((NVIC->ISER[bank] | NVIC->ISPR[bank] | NVIC->IABR[bank]) & mask) ==
           0U;
}
nx_result_t nx_stm32f4_crypto_irq_admit(const nx_stm32f4_crypto_irq_t* receipt,
                                        uint16_t irq, uint8_t priority) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(receipt, irq) || priority >= (1U << __NVIC_PRIO_BITS))
        return NX_ERROR_INVALID;
    return receipt->owned || !quiet(irq) ? NX_ERROR_BUSY : NX_SUCCESS;
}
nx_result_t nx_stm32f4_crypto_irq_acquire(nx_stm32f4_crypto_irq_t* receipt,
                                          uint16_t irq, uint8_t priority) {
    nx_result_t result = nx_stm32f4_crypto_irq_admit(receipt, irq, priority);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!quiet(irq)) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    receipt->saved_priority = NVIC->IPR[irq];
    receipt->owned = true;
    uint8_t encoded = (uint8_t)(priority << (8U - __NVIC_PRIO_BITS));
    NVIC->IPR[irq] = encoded;
    nx_arch_dsb();
    if (NVIC->IPR[irq] != encoded) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_IO;
    }
    uint32_t mask = UINT32_C(1) << (irq % 32U);
    WRITE_REG(NVIC->ISER[irq / 32U], mask);
    nx_arch_dsb();
    result = (NVIC->ISER[irq / 32U] & mask) != 0U ? NX_SUCCESS : NX_ERROR_IO;
    nx_arch_irq_restore(guard);
    return result;
}
nx_result_t nx_stm32f4_crypto_irq_release(nx_stm32f4_crypto_irq_t* receipt,
                                          uint16_t irq) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(receipt, irq))
        return NX_ERROR_INVALID;
    if (!receipt->owned)
        return NX_SUCCESS;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    unsigned bank = irq / 32U;
    uint32_t mask = UINT32_C(1) << (irq % 32U);
    WRITE_REG(NVIC->ICER[bank], mask);
    WRITE_REG(NVIC->ICPR[bank], mask);
    nx_arch_dsb();
    if (!quiet(irq)) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    NVIC->IPR[irq] = receipt->saved_priority;
    nx_arch_dsb();
    if (NVIC->IPR[irq] != receipt->saved_priority) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    receipt->owned = false;
    receipt->saved_priority = 0U;
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
