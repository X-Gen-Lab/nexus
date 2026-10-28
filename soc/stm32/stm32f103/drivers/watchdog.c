/**
 * \file            watchdog.c
 * \brief           F103 independent watchdog effects and LSI uncertainty
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f103_provider.h"
/** \brief Cold binding observes no hardware and never activates the IWDG. */
nx_result_t nx_stm32f103_watchdog_prepare(
    nx_stm32f103_watchdog_state_t* p, IWDG_TypeDef* registers, RCC_TypeDef* rcc,
    FLASH_TypeDef* flash, DBGMCU_TypeDef* debug, uint32_t poll_limit) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (p == NULL || registers == NULL || rcc == NULL || flash == NULL ||
        debug == NULL || poll_limit == 0U) {
        return NX_ERROR_INVALID;
    }
    if (p->state.enabled || p->preparation_retained) {
        return NX_ERROR_BUSY;
    }
    p->registers = registers;
    p->rcc = rcc;
    p->flash = flash;
    p->debug = debug;
    p->poll_limit = poll_limit;
    return NX_SUCCESS;
}
static void bounds(nx_watchdog_state_t* state, uint32_t divisor,
                   uint32_t count) {
    uint64_t clocks = (uint64_t)divisor * count * 1000000U;
    /* DS5319 Rev20 Table25: 30..60kHz LSI,nominal40kHz. */
    state->minimum_timeout_us = (uint32_t)(clocks / 60000U);
    state->maximum_timeout_us = (uint32_t)((clocks + 29999U) / 30000U);
}
static bool valid(const nx_stm32f103_watchdog_state_t* p) {
    return p != NULL && p->registers != NULL && p->rcc != NULL &&
           p->flash != NULL && p->debug != NULL && p->poll_limit != 0U;
}
static bool output_valid(const nx_stm32f103_watchdog_state_t* p,
                         const nx_watchdog_state_t* state) {
    uintptr_t out = (uintptr_t)state;
    size_t size = sizeof(*state);
    return state != NULL && out <= UINTPTR_MAX - size &&
           !(out < (uintptr_t)p + sizeof(*p) && (uintptr_t)p < out + size) &&
           !(out < (uintptr_t)p->registers + sizeof(*p->registers) &&
             (uintptr_t)p->registers < out + size) &&
           !(out < (uintptr_t)p->rcc + sizeof(*p->rcc) &&
             (uintptr_t)p->rcc < out + size) &&
           !(out < (uintptr_t)p->flash + sizeof(*p->flash) &&
             (uintptr_t)p->flash < out + size) &&
           !(out < (uintptr_t)p->debug + sizeof(*p->debug) &&
             (uintptr_t)p->debug < out + size);
}
static void observed(const nx_stm32f103_watchdog_state_t* p,
                     nx_watchdog_state_t* state) {
    *state = p->state;
    /* ES096 2.2.2 excludes CPU DBGMCU_CR access. This is the supported
     * firmware setting, never an observation of an external debug agent. */
    state->debug_freeze = false;
    if ((NX_F103_READ(&p->flash->OBR) & FLASH_OBR_IWDG_SW) == 0U) {
        state->enabled = true;
        state->irreversible = true;
        if ((NX_F103_READ(&p->registers->SR) & (IWDG_SR_PVU | IWDG_SR_RVU)) !=
            0U) {
            state->minimum_timeout_us = 0U;
            state->maximum_timeout_us = 0U;
        } else {
            uint32_t prescaler = NX_F103_READ(&p->registers->PR) & 7U;
            bounds(state, prescaler > 6U ? 256U : 4U << prescaler,
                   (NX_F103_READ(&p->registers->RLR) & 0xfffU) + 1U);
        }
    }
}
static nx_result_t restore_preparation(nx_stm32f103_watchdog_state_t* p) {
    if (p->lsi_owned) {
        NX_F103_WRITE(&p->rcc->CSR,
                      NX_F103_READ(&p->rcc->CSR) & ~(uint32_t)RCC_CSR_LSION);
        nx_arch_dsb();
        if ((NX_F103_READ(&p->rcc->CSR) & RCC_CSR_LSION) != 0U) {
            return NX_ERROR_IO;
        }
        p->lsi_owned = false;
    }
    p->preparation_retained = false;
    return NX_SUCCESS;
}
nx_result_t nx_stm32f103_watchdog_stop(nx_stm32f103_watchdog_state_t* p) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(p)) {
        return NX_ERROR_INVALID;
    }
    nx_watchdog_state_t state;
    observed(p, &state);
    if (state.enabled) {
        return NX_ERROR_STATE;
    }
    return restore_preparation(p);
}
static nx_result_t get_state(const void* context, nx_watchdog_state_t* state) {
    const nx_stm32f103_watchdog_state_t* p = context;
    if (!nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(p) || !output_valid(p, state)) {
        return NX_ERROR_INVALID;
    }
    observed(p, state);
    return state->enabled && state->maximum_timeout_us == 0U
               ? NX_ERROR_UNSUPPORTED
               : NX_SUCCESS;
}
static nx_result_t feed(void* context) {
    nx_stm32f103_watchdog_state_t* p = context;
    if (!nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(p)) {
        return NX_ERROR_INVALID;
    }
    nx_watchdog_state_t state;
    observed(p, &state);
    if (!state.enabled) {
        return NX_ERROR_STATE;
    }
    NX_F103_WRITE(&p->registers->KR, 0xaaaaU);
    return NX_SUCCESS;
}
static nx_result_t enable(void* context, uint32_t timeout, bool debug_freeze,
                          nx_watchdog_state_t* state) {
    nx_stm32f103_watchdog_state_t* p = context;
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(p) || !output_valid(p, state)) {
        return NX_ERROR_INVALID;
    }
    if (debug_freeze) {
        return NX_ERROR_UNSUPPORTED;
    }
    observed(p, state);
    if (state->enabled) {
        p->state = *state;
        return NX_ERROR_STATE;
    }
    if (p->preparation_retained) {
        return NX_ERROR_BUSY;
    }
    if (timeout < 100U || timeout > 26214400U) {
        return NX_ERROR_INVALID;
    }
    if ((NX_F103_READ(&p->flash->OBR) & FLASH_OBR_OPTERR) != 0U) {
        return NX_ERROR_IO;
    }
    if ((NX_F103_READ(&p->registers->SR) & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0U) {
        return NX_ERROR_BUSY;
    }
    uint32_t divisor = 4U;
    uint32_t prescaler = 0U;
    uint32_t count;
    for (;;) {
        uint64_t unit = (uint64_t)divisor * 1000000U;
        count = (uint32_t)(((uint64_t)timeout * 40000U + unit - 1U) / unit);
        if (count <= 4096U) {
            break;
        }
        divisor *= 2U;
        ++prescaler;
    }
    p->lsi_owned = (NX_F103_READ(&p->rcc->CSR) & RCC_CSR_LSION) == 0U;
    p->preparation_retained = true;
    NX_F103_WRITE(&p->rcc->CSR, NX_F103_READ(&p->rcc->CSR) | RCC_CSR_LSION);
    uint32_t polls = p->poll_limit;
    while (polls-- != 0U) {
        NX_F103_WATCHDOG_POLL(p);
        if ((NX_F103_READ(&p->rcc->CSR) & RCC_CSR_LSIRDY) != 0U) {
            break;
        }
    }
    if ((NX_F103_READ(&p->rcc->CSR) & RCC_CSR_LSIRDY) == 0U) {
        (void)restore_preparation(p);
        return NX_ERROR_IO;
    }
    nx_watchdog_state_t envelope = {0};
    uint32_t old_pr = NX_F103_READ(&p->registers->PR) & 7U;
    uint32_t old_divisor = old_pr > 6U ? 256U : 4U << old_pr;
    /* RM0008 section19.3: CCCC starts from0xFFF. An inactive RLR value
     * has not been reloaded and cannot bound the initial counter interval. */
    uint32_t old_count = 4096U;
    p->state.enabled = true;
    p->state.irreversible = true;
    p->state.debug_freeze = false;
    bounds(&p->state, divisor, count);
    NX_F103_WRITE(&p->registers->KR, 0xccccU);
    NX_F103_WRITE(&p->registers->KR, 0x5555U);
    NX_F103_WRITE(&p->registers->PR, prescaler);
    NX_F103_WRITE(&p->registers->RLR, count - 1U);
    polls = p->poll_limit;
    while (polls-- != 0U) {
        NX_F103_WATCHDOG_POLL(p);
        if ((NX_F103_READ(&p->registers->SR) & (IWDG_SR_PVU | IWDG_SR_RVU)) ==
            0U) {
            break;
        }
    }
    if ((NX_F103_READ(&p->registers->SR) & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0U ||
        NX_F103_READ(&p->registers->PR) != prescaler ||
        NX_F103_READ(&p->registers->RLR) != count - 1U) {
        /* PR andRLR can latch independently. Include cross-combinations,
         * not merely the two intended complete timeout settings. */
        bounds(&envelope, old_divisor < divisor ? old_divisor : divisor,
               old_count < count ? old_count : count);
        p->state.minimum_timeout_us = envelope.minimum_timeout_us;
        bounds(&envelope, old_divisor > divisor ? old_divisor : divisor,
               old_count > count ? old_count : count);
        p->state.maximum_timeout_us = envelope.maximum_timeout_us;
        *state = p->state;
        return NX_ERROR_IO;
    }
    NX_F103_WRITE(&p->registers->KR, 0xaaaaU);
    *state = p->state;
    return NX_SUCCESS;
}
const nx_watchdog_ops_t nx_stm32f103_watchdog_ops = {enable, feed, get_state};
uint32_t nx_reset_cause(void) {
    uint32_t raw = g_nx_stm32f103_system.reset_cause;
    uint32_t result = 0U;
    if ((raw & RCC_CSR_PORRSTF) != 0U)
        result |= NX_RESET_POWER_ON;
    if ((raw & RCC_CSR_PINRSTF) != 0U)
        result |= NX_RESET_PIN;
    if ((raw & RCC_CSR_SFTRSTF) != 0U)
        result |= NX_RESET_SOFTWARE;
    if ((raw & RCC_CSR_IWDGRSTF) != 0U)
        result |= NX_RESET_IWDG;
    if ((raw & RCC_CSR_WWDGRSTF) != 0U)
        result |= NX_RESET_WWDG;
    if ((raw & RCC_CSR_LPWRRSTF) != 0U)
        result |= NX_RESET_LOW_POWER;
    return result;
}
