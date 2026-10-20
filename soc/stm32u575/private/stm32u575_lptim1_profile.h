/**
 * \file            stm32u575_lptim1_profile.h
 * \brief           Private fixed single-tuple LPTIM1 lifecycle profile
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32U575_LPTIM1_PROFILE_H
#define NX_STM32U575_LPTIM1_PROFILE_H
#define LPTIM_IRQ_WORD (67U / 32U)
#define LPTIM_IRQ_BIT  (1U << (67U % 32U))
#define LPTIM_OK       (LPTIM_ISR_CMP1OK | LPTIM_ISR_REPOK | LPTIM_ISR_ARROK)
static bool lptim_vector_quiet(void) {
    return (NX_LPDMA_READ(NVIC->ISER[LPTIM_IRQ_WORD]) & LPTIM_IRQ_BIT) == 0U &&
           (NX_LPDMA_READ(NVIC->ISPR[LPTIM_IRQ_WORD]) & LPTIM_IRQ_BIT) == 0U &&
           (NX_LPDMA_READ(NVIC->IABR[LPTIM_IRQ_WORD]) & LPTIM_IRQ_BIT) == 0U;
}
static bool lptim_cold(void) {
    return NX_LPDMA_READ(LPTIM1->CR) == 0U &&
           NX_LPDMA_READ(LPTIM1->DIER) == 0U &&
           NX_LPDMA_READ(LPTIM1->CFGR) == 0U &&
           NX_LPDMA_READ(LPTIM1->CFGR2) == 0U &&
           NX_LPDMA_READ(LPTIM1->CCMR1) == 0U &&
           NX_LPDMA_READ(LPTIM1->CCR1) == 0U &&
           NX_LPDMA_READ(LPTIM1->CCR2) == 0U &&
           NX_LPDMA_READ(LPTIM1->ARR) == 1U &&
           NX_LPDMA_READ(LPTIM1->RCR) == 0U &&
           NX_LPDMA_READ(LPTIM1->CNT) == 0U && NX_LPDMA_READ(LPTIM1->ISR) == 0U;
}
static bool lptim_access(const owner_t* owner) {
    uint32_t source = RCC_BDCR_LSEON | RCC_BDCR_LSERDY | RCC_BDCR_LSESYSEN |
                      RCC_BDCR_LSESYSRDY;
    return owner->peripheral.ready &&
           (NX_LPDMA_READ(RCC->BDCR) & source) == source &&
           (NX_LPDMA_READ(RCC->APB3ENR) & RCC_APB3ENR_LPTIM1EN) != 0U &&
           (NX_LPDMA_READ(RCC->CCIPR3) & RCC_CCIPR3_LPTIM1SEL) ==
               RCC_CCIPR3_LPTIM1SEL &&
           ((NX_LPDMA_READ(RCC->APB3RSTR) & RCC_APB3RSTR_LPTIM1RST) == 0U ||
            owner->peripheral.reset_asserted);
}
/* LSESYSRDY is read-only and asynchronous; compare only the owned proof. */
static bool lptim_source(bool enabled, uint32_t limit) {
    uint32_t value = NX_LPDMA_READ(RCC->BDCR);
    value = enabled ? value | RCC_BDCR_LSESYSEN
                    : value & ~(uint32_t)RCC_BDCR_LSESYSEN;
    NX_LPDMA_WRITE(RCC->BDCR, value);
    nx_arch_dsb();
    return wait_bits(&RCC->BDCR, RCC_BDCR_LSESYSEN | RCC_BDCR_LSESYSRDY,
                     enabled ? RCC_BDCR_LSESYSEN | RCC_BDCR_LSESYSRDY : 0U,
                     limit);
}
/* Start writes no timer state, so a foreign nonzero controller is never reset.
 * Partial access claims remain explicit until this joined stop succeeds. */
static bool lptim_stop_access(owner_t* owner, uint32_t limit) {
    if (!owner->peripheral.effects)
        return true;
    if (owner->peripheral.dirty)
        return false;
    if (owner->peripheral.gate_added &&
        !update(&RCC->APB3ENR, RCC_APB3ENR_LPTIM1EN, 0U))
        return false;
    owner->peripheral.gate_added = false;
    if (!update(&RCC->CCIPR3, RCC_CCIPR3_LPTIM1SEL, 0U))
        return false;
    if (owner->peripheral.source_added && !lptim_source(false, limit))
        return false;
    owner->peripheral.source_added = false;
    owner->peripheral.effects = owner->peripheral.ready = false;
    return true;
}
static nx_result_t start(void* pointer, uint32_t limit) {
    if (!context(false))
        return NX_ERROR_CONTEXT;
    nx_stm32u575_lpdma_copy_t copy = {&nx_stm32u575_lpdma_copy_ops, pointer};
    owner_t owner;
    nx_result_t result = load(&copy, &owner);
    if (result != NX_SUCCESS)
        return result;
    if (limit == 0U)
        return NX_ERROR_INVALID;
    if (!owner.opened || owner.closing || owner.request != NULL ||
        owner.peripheral.effects)
        return NX_ERROR_STATE;
    if (!profile(NULL) ||
        (NX_LPDMA_READ(RCC->APB3RSTR) & RCC_APB3RSTR_LPTIM1RST) != 0U ||
        (NX_LPDMA_READ(RCC->APB3ENR) & RCC_APB3ENR_LPTIM1EN) != 0U ||
        (NX_LPDMA_READ(RCC->CCIPR3) & RCC_CCIPR3_LPTIM1SEL) != 0U ||
        !lptim_vector_quiet())
        return NX_ERROR_BUSY;
    nx_stm32u575_retained_time_snapshot_t sample;
    result = clock_valid(&owner, &sample);
    if (result != NX_SUCCESS)
        return result;
    owner.peripheral.effects = true;
    owner.peripheral.source_added =
        (NX_LPDMA_READ(RCC->BDCR) & RCC_BDCR_LSESYSEN) == 0U;
    if ((owner.peripheral.source_added && !lptim_source(true, limit)) ||
        (NX_LPDMA_READ(RCC->BDCR) & (RCC_BDCR_LSESYSEN | RCC_BDCR_LSESYSRDY)) !=
            (RCC_BDCR_LSESYSEN | RCC_BDCR_LSESYSRDY)) {
        save(&copy, &owner);
        return NX_ERROR_BUSY;
    }
    owner.peripheral.gate_added = true;
    if (!update(&RCC->APB3ENR, RCC_APB3ENR_LPTIM1EN, RCC_APB3ENR_LPTIM1EN) ||
        !lptim_cold() || !lptim_vector_quiet() ||
        !update(&RCC->CCIPR3, RCC_CCIPR3_LPTIM1SEL, RCC_CCIPR3_LPTIM1SEL)) {
        save(&copy, &owner);
        return NX_ERROR_BUSY;
    }
    owner.peripheral.ready = true;
    save(&copy, &owner);
    return NX_SUCCESS;
}
nx_result_t
nx_stm32u575_lpdma_lptim1_start(const nx_stm32u575_lpdma_lptim1_t* receiver,
                                uint32_t limit) {
    owner_t owner;
    nx_result_t result = load(receiver, &owner);
    return result == NX_SUCCESS ? start(receiver->context, limit) : result;
}
bool nx_stm32u575_lpdma_lptim1_reset_owned(
    const nx_stm32u575_lpdma_lptim1_t* receiver) {
    owner_t owner;
    return load(receiver, &owner) == NX_SUCCESS && owner.request != NULL &&
           owner.peripheral.reset_asserted && owner.reset_done;
}
static bool lptim_clear(uint32_t flags, uint32_t limit) {
    NX_LPDMA_WRITE(LPTIM1->ICR, flags);
    nx_arch_dsb();
    return wait_bits(&LPTIM1->ISR, flags, 0U, limit);
}
/* The retained 256Hz clock shares LSE. Two ticks after ENABLE include at least
 * one complete 128-cycle tick despite initial phase: more than two LSE cycles.
 * This proves elapsed source cycles, not readiness from ENABLE readback. */
static bool lptim_enable_wait(owner_t* owner, uint64_t before, uint32_t limit) {
    while (limit-- != 0U) {
        nx_stm32u575_retained_time_snapshot_t sample;
        nx_result_t result = clock_valid(owner, &sample);
        if (result == NX_ERROR_BUSY)
            continue;
        if (result != NX_SUCCESS) {
            fail(owner, result);
            return false;
        }
        if (owner->deadline != NX_DEADLINE_NEVER &&
            sample.now >= owner->deadline) {
            fail(owner, NX_ERROR_TIMEOUT);
            return false;
        }
        if (sample.ticks >= before && sample.ticks - before >= 2U)
            return true;
    }
    fail(owner, NX_ERROR_TIMEOUT);
    return false;
}
static void lptim_descriptors(owner_t* owner) {
    uint32_t* words = owner->workspace->private_words;
    for (size_t index = 0U; index < 2U; ++index) {
        size_t at = index * 6U;
        words[at] = DATA_BITS;
        words[at + 1U] = FINAL_BITS;
        words[at + 2U] = 4U;
        words[at + 3U] =
            (uint32_t)((uintptr_t)owner->source + (index + 1U) * 4U);
        words[at + 4U] =
            (uint32_t)(uintptr_t)(index == 0U ? &LPTIM1->RCR : &LPTIM1->ARR);
        words[at + 5U] =
            index == 0U ? UPDATE_BITS |
                              ((uint32_t)((uintptr_t)words + 24U) & DMA_CLLR_LA)
                        : 0U;
    }
}
static bool lptim_prepare(owner_t* owner, uint32_t limit) {
    if (!lptim_access(owner) || !lptim_cold())
        return false;
    owner->peripheral.sleep =
        NX_LPDMA_READ(RCC->APB3SMENR) & RCC_APB3SMENR_LPTIM1SMEN;
    owner->peripheral.dirty = true;
    nx_stm32u575_retained_time_snapshot_t before;
    if (!update(&RCC->APB3SMENR, RCC_APB3SMENR_LPTIM1SMEN,
                RCC_APB3SMENR_LPTIM1SMEN) ||
        !checked(&LPTIM1->CFGR, LPTIM_CFGR_PRELOAD) ||
        !checked(&LPTIM1->CCMR1, LPTIM_CCMR1_CC1E) ||
        !checked(&LPTIM1->CR, LPTIM_CR_ENABLE) ||
        clock_valid(owner, &before) != NX_SUCCESS ||
        !lptim_enable_wait(owner, before.ticks, limit))
        return false;
    owner->peripheral.enabled = true;
    if (!lptim_clear(LPTIM_OK, limit))
        return false;
    NX_LPDMA_WRITE(LPTIM1->CCR1, 0U);
    NX_LPDMA_WRITE(LPTIM1->RCR, 0U);
    NX_LPDMA_WRITE(LPTIM1->ARR, 1023U);
    nx_arch_dsb();
    if (!wait_bits(&LPTIM1->ISR, LPTIM_OK, LPTIM_OK, limit) ||
        NX_LPDMA_READ(LPTIM1->CCR1) != 0U || NX_LPDMA_READ(LPTIM1->RCR) != 0U ||
        NX_LPDMA_READ(LPTIM1->ARR) != 1023U)
        return false;
    return lptim_clear(LPTIM_OK | LPTIM_ISR_UE, limit);
}
static bool lptim_dier(owner_t* owner, uint32_t value, uint32_t limit) {
    if (owner->peripheral.dier_pending) {
        if (!wait_bits(&LPTIM1->ISR, LPTIM_ISR_DIEROK, LPTIM_ISR_DIEROK, limit))
            return false;
        owner->peripheral.dier_pending = false;
    }
    if (!lptim_clear(LPTIM_ISR_DIEROK, limit))
        return false;
    owner->peripheral.dier_pending = true;
    NX_LPDMA_WRITE(LPTIM1->DIER, value);
    nx_arch_dsb();
    if (!wait_bits(&LPTIM1->ISR, LPTIM_ISR_DIEROK, LPTIM_ISR_DIEROK, limit) ||
        NX_LPDMA_READ(LPTIM1->DIER) != value)
        return false;
    owner->peripheral.dier_pending = false;
    return true;
}
static nx_result_t lptim_withdraw(owner_t* owner, uint32_t limit) {
    if (!owner->peripheral.producer)
        return NX_SUCCESS;
    if (!owner->failed) {
        nx_result_t result = observe(owner);
        /* No terminal state: keep the unissued request live. */
        if (result != NX_SUCCESS)
            return result;
        if (!owner->failed && !owner->peripheral.write_ok) {
            /* ES0499 2.17.3: no write-OK flag is assumed to survive DIER.
             * Finite TC/idle proves no later DMA write can replace this tuple.
             * Capture the three fresh kernel receipts before issuing DIER0. */
            if (!wait_bits(&LPTIM1->ISR, LPTIM_OK, LPTIM_OK, limit)) {
                fail(owner, NX_ERROR_IO);
                /* Next service/recover takes the saved error drain path. */
                return NX_ERROR_BUSY;
            }
            owner->peripheral.write_ok = true;
        }
    }
    if (!owner->peripheral.enabled || !lptim_dier(owner, 0U, limit))
        return NX_ERROR_BUSY;
    owner->peripheral.producer = false;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_lpdma_lptim1_producer_withdraw(
    const nx_stm32u575_lpdma_lptim1_t* receiver, nx_result_t cause,
    uint32_t limit) {
    if (!context(true))
        return NX_ERROR_CONTEXT;
    owner_t owner;
    nx_result_t result = load(receiver, &owner);
    if (result != NX_SUCCESS)
        return result;
    if (limit == 0U)
        return NX_ERROR_INVALID;
    if (owner.request == NULL || !profile(&owner) || !lptim_access(&owner)) {
        save(receiver, &owner);
        return NX_ERROR_STATE;
    }
    if (cause != NX_SUCCESS)
        fail(&owner, cause);
    result = lptim_withdraw(&owner, limit);
    save(receiver, &owner);
    return result;
}
nx_result_t nx_stm32u575_lpdma_lptim1_producer_start(
    const nx_stm32u575_lpdma_lptim1_t* receiver, uint32_t limit) {
    if (!context(true))
        return NX_ERROR_CONTEXT;
    owner_t owner;
    nx_result_t result = load(receiver, &owner);
    if (result != NX_SUCCESS)
        return result;
    if (limit == 0U)
        return NX_ERROR_INVALID;
    if (owner.request == NULL || !owner.configured || owner.failed ||
        !owner.peripheral.enabled || !profile(&owner) ||
        !lptim_access(&owner)) {
        save(receiver, &owner);
        return NX_ERROR_STATE;
    }
    owner.peripheral.producer = true;
    bool ready = lptim_dier(&owner, LPTIM_DIER_UEDE, limit);
    if (ready) {
        NX_LPDMA_WRITE(LPTIM1->CR, LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT);
        nx_arch_dsb();
        ready = (NX_LPDMA_READ(LPTIM1->CR) & LPTIM_CR_ENABLE) != 0U;
    }
    if (!ready)
        fail(&owner, NX_ERROR_IO);
    save(receiver, &owner);
    return ready ? NX_SUCCESS : NX_ERROR_IO;
}
/* DMA is already idle/reset/detached. Partial cancellation needs no flags for
 * unissued writes. Success uses only the pre-DIER kernel-receipt proof. */
static bool lptim_isolate(owner_t* owner) {
    if (!owner->peripheral.dirty)
        return true;
    if (!owner->peripheral.reset_asserted && owner->completed &&
        !owner->failed && !owner->peripheral.write_ok)
        return false;
    owner->peripheral.reset_asserted = true;
    if (!update(&RCC->APB3RSTR, RCC_APB3RSTR_LPTIM1RST,
                RCC_APB3RSTR_LPTIM1RST) ||
        !update(&RCC->APB3RSTR, RCC_APB3RSTR_LPTIM1RST, 0U))
        return false;
    owner->peripheral.reset_asserted = false;
    if (!lptim_cold())
        return false;
    NX_LPDMA_WRITE(NVIC->ICER[LPTIM_IRQ_WORD], LPTIM_IRQ_BIT);
    NX_LPDMA_WRITE(NVIC->ICPR[LPTIM_IRQ_WORD], LPTIM_IRQ_BIT);
    nx_arch_dsb();
    nx_arch_isb();
    if (!lptim_vector_quiet() ||
        !update(&RCC->APB3SMENR, RCC_APB3SMENR_LPTIM1SMEN,
                owner->peripheral.sleep))
        return false;
    owner->peripheral.dirty = owner->peripheral.enabled = false;
    owner->peripheral.dier_pending = owner->peripheral.write_ok = false;
    return true;
}
#endif
