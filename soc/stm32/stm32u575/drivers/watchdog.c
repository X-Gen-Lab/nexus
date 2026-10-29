/**
 * \file            watchdog.c
 * \brief           U575 bounded IWDG activation with irreversible ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_cold.h"
#include "stm32u575_provider.h"

#ifndef NX_U575_WATCHDOG_POLL
#define NX_U575_WATCHDOG_POLL(port) ((void)(port))
#endif

/** \brief DS13737 Table84 full temperature LSI characterization envelope. */
static void timeout_bounds(nx_watchdog_state_t* state, uint32_t divisor,
                           uint32_t count) {
    uint64_t clocks = (uint64_t)divisor * count * 1000000U;
    state->minimum_timeout_us = (uint32_t)(clocks / 33600U);
    state->maximum_timeout_us = (uint32_t)((clocks + 30399U) / 30400U);
}

/** \brief Hardware option activation is irreversible even before API entry. */
static void observe_option(nx_stm32u575_watchdog_state_t* p) {
    if ((NX_U575_READ(&p->flash->OPTR) & FLASH_OPTR_IWDG_SW) != 0U)
        return;
    p->state.enabled = true;
    p->state.irreversible = true;
    p->state.debug_freeze = (NX_U575_READ(&p->debug->APB1FZR1) &
                             DBGMCU_APB1FZR1_DBG_IWDG_STOP) != 0U;
    if ((NX_U575_READ(&p->rcc->BDCR) & RCC_BDCR_LSIPREDIV) != 0U ||
        NX_U575_READ(&p->registers->SR) != 0U) {
        p->state.minimum_timeout_us = 0U;
        p->state.maximum_timeout_us = 0U;
        return;
    }
    uint32_t prescaler = NX_U575_READ(&p->registers->PR) & IWDG_PR_PR;
    uint32_t divisor = prescaler > 8U ? 1024U : 4U << prescaler;
    timeout_bounds(&p->state, divisor,
                   (NX_U575_READ(&p->registers->RLR) & IWDG_RLR_RL) + 1U);
}

static bool valid(const nx_stm32u575_watchdog_state_t* p) {
    return p != NULL && p->registers != NULL && p->rcc != NULL &&
           p->flash != NULL && p->debug != NULL && p->poll_limit != 0U;
}

static bool overlaps(uintptr_t output, const void* object, size_t bytes) {
    uintptr_t target = (uintptr_t)object;
    return target <= UINTPTR_MAX - bytes && output < target + bytes &&
           target < output + sizeof(nx_watchdog_state_t);
}

/** \brief Output publication cannot overwrite the owner or its MMIO leaves. */
static bool output_valid(const nx_stm32u575_watchdog_state_t* p,
                         const nx_watchdog_state_t* state) {
    uintptr_t output = (uintptr_t)state;
    return state != NULL && output % _Alignof(nx_watchdog_state_t) == 0U &&
           output <= UINTPTR_MAX - sizeof(*state) &&
           !overlaps(output, p, sizeof(*p)) &&
           !overlaps(output, p->registers, sizeof(*p->registers)) &&
           !overlaps(output, p->rcc, sizeof(*p->rcc)) &&
           !overlaps(output, p->flash, sizeof(*p->flash)) &&
           !overlaps(output, p->debug, sizeof(*p->debug));
}

/** \brief Withdraw only the LSI enable introduced by this failed attempt. */
static void release_lsi(nx_stm32u575_watchdog_state_t* p) {
    if (!p->lsi_added)
        return;
    NX_U575_WRITE(&p->rcc->BDCR,
                  NX_U575_READ(&p->rcc->BDCR) & ~(uint32_t)RCC_BDCR_LSION);
    nx_arch_dsb();
    if ((NX_U575_READ(&p->rcc->BDCR) & RCC_BDCR_LSION) == 0U)
        p->lsi_added = false;
}

nx_result_t nx_stm32u575_watchdog_stop(nx_stm32u575_watchdog_state_t* p) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    observe_option(p);
    if (p->state.enabled)
        return NX_ERROR_STATE;
    if (p->debug_pending) {
        uint32_t current_freeze = NX_U575_READ(&p->debug->APB1FZR1);
        NX_U575_WRITE(
            &p->debug->APB1FZR1,
            (current_freeze & ~(uint32_t)DBGMCU_APB1FZR1_DBG_IWDG_STOP) |
                p->previous_debug);
        nx_arch_dsb();
        if ((NX_U575_READ(&p->debug->APB1FZR1) &
             DBGMCU_APB1FZR1_DBG_IWDG_STOP) != p->previous_debug)
            return NX_ERROR_IO;
        p->debug_pending = false;
    }
    release_lsi(p);
    return p->lsi_added ? NX_ERROR_IO : NX_SUCCESS;
}

static nx_result_t enable(void* context, uint32_t timeout_us, bool debug_freeze,
                          nx_watchdog_state_t* state) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    nx_stm32u575_watchdog_state_t* p = context;
    if (!valid(p) || !output_valid(p, state))
        return NX_ERROR_INVALID;
    observe_option(p);
    *state = p->state;
    if (p->state.enabled)
        return NX_ERROR_STATE;
    if (p->lsi_added || p->debug_pending)
        return NX_ERROR_BUSY;
    /* RM0456 61.7.3 recommends RL>=2, so the refresh has a pre-count-one
     * opportunity. Count=RL+1 requires at least three nominal LSI periods. */
    if (timeout_us < 375U || timeout_us > 131072000U)
        return NX_ERROR_INVALID;
    if ((NX_U575_READ(&p->rcc->BDCR) & RCC_BDCR_LSIPREDIV) != 0U ||
        NX_U575_READ(&p->registers->WINR) != IWDG_WINR_WIN ||
        NX_U575_READ(&p->registers->EWCR) != 0U)
        return NX_ERROR_UNSUPPORTED;
    if (NX_U575_READ(&p->registers->SR) != 0U)
        return NX_ERROR_BUSY;
    uint32_t divisor = 4U, prescaler = 0U, count;
    for (;;) {
        count = (uint32_t)(((uint64_t)timeout_us * 32000U +
                            (uint64_t)divisor * 1000000U - 1U) /
                           ((uint64_t)divisor * 1000000U));
        if (count <= 4096U)
            break;
        divisor *= 2U;
        ++prescaler;
    }
    uint32_t old_prescaler = NX_U575_READ(&p->registers->PR) & IWDG_PR_PR;
    uint32_t old_divisor = old_prescaler > 8U ? 1024U : 4U << old_prescaler;
    /* CCCC starts IWDCNT at reset 0xfff (RM0456 61.4.3), independently of a
     * smaller inactive RLR. A failed shadow update must include that count. */
    uint32_t old_count = 4096U;
    if ((NX_U575_READ(&p->rcc->BDCR) & RCC_BDCR_LSION) == 0U)
        p->lsi_added = true;
    NX_U575_WRITE(&p->rcc->BDCR, NX_U575_READ(&p->rcc->BDCR) | RCC_BDCR_LSION);
    nx_arch_dsb();
    uint32_t remaining = p->poll_limit;
    while ((NX_U575_READ(&p->rcc->BDCR) & RCC_BDCR_LSIRDY) == 0U &&
           remaining-- != 0U)
        NX_U575_WATCHDOG_POLL(p);
    if ((NX_U575_READ(&p->rcc->BDCR) & (RCC_BDCR_LSION | RCC_BDCR_LSIRDY)) !=
        (RCC_BDCR_LSION | RCC_BDCR_LSIRDY)) {
        release_lsi(p);
        return NX_ERROR_IO;
    }
    uint32_t freeze = NX_U575_READ(&p->debug->APB1FZR1);
    uint32_t desired = debug_freeze
                           ? freeze | DBGMCU_APB1FZR1_DBG_IWDG_STOP
                           : freeze & ~(uint32_t)DBGMCU_APB1FZR1_DBG_IWDG_STOP;
    p->previous_debug = freeze & (uint32_t)DBGMCU_APB1FZR1_DBG_IWDG_STOP;
    p->debug_pending =
        (desired & DBGMCU_APB1FZR1_DBG_IWDG_STOP) != p->previous_debug;
    NX_U575_WRITE(&p->debug->APB1FZR1, desired);
    nx_arch_dsb();
    if ((NX_U575_READ(&p->debug->APB1FZR1) & DBGMCU_APB1FZR1_DBG_IWDG_STOP) !=
        (desired & DBGMCU_APB1FZR1_DBG_IWDG_STOP)) {
        (void)nx_stm32u575_watchdog_stop(p);
        return NX_ERROR_IO;
    }
    /* RM0456 61.4.4: activation precedes shadow-register programming.
     * Record conservative ownership before issuing the irreversible key. */
    p->state.enabled = true;
    p->state.irreversible = true;
    p->debug_pending = false;
    p->state.debug_freeze = debug_freeze;
    timeout_bounds(&p->state, divisor, count);
    nx_arch_dmb();
    NX_U575_WRITE(&p->registers->KR, 0xCCCCU);
    NX_U575_WRITE(&p->registers->KR, 0x5555U);
    NX_U575_WRITE(&p->registers->PR, prescaler);
    NX_U575_WRITE(&p->registers->RLR, count - 1U);
    remaining = p->poll_limit;
    while (NX_U575_READ(&p->registers->SR) != 0U && remaining-- != 0U)
        NX_U575_WATCHDOG_POLL(p);
    if (NX_U575_READ(&p->registers->SR) != 0U ||
        NX_U575_READ(&p->registers->PR) != prescaler ||
        NX_U575_READ(&p->registers->RLR) != count - 1U) {
        nx_watchdog_state_t lower = {0}, upper = {0};
        timeout_bounds(&lower, divisor < old_divisor ? divisor : old_divisor,
                       count < old_count ? count : old_count);
        timeout_bounds(&upper, divisor > old_divisor ? divisor : old_divisor,
                       count > old_count ? count : old_count);
        p->state.minimum_timeout_us = lower.minimum_timeout_us;
        p->state.maximum_timeout_us = upper.maximum_timeout_us;
        /* Relock without reload: uncertain synchronization cannot authorize
         * a refresh against an unobserved counter/window state. */
        NX_U575_WRITE(&p->registers->KR, 0U);
        *state = p->state;
        return NX_ERROR_IO;
    }
    NX_U575_WRITE(&p->registers->KR, 0xAAAAU);
    *state = p->state;
    return NX_SUCCESS;
}

static nx_result_t feed(void* context) {
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    nx_stm32u575_watchdog_state_t* p = context;
    if (!valid(p))
        return NX_ERROR_INVALID;
    observe_option(p);
    if (!p->state.enabled)
        return NX_ERROR_STATE;
    if (NX_U575_READ(&p->registers->WINR) != IWDG_WINR_WIN)
        return NX_ERROR_UNSUPPORTED;
    NX_U575_WRITE(&p->registers->KR, 0xAAAAU);
    return NX_SUCCESS;
}

static nx_result_t current(const void* context, nx_watchdog_state_t* state) {
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    const nx_stm32u575_watchdog_state_t* p = context;
    if (!valid(p) || !output_valid(p, state))
        return NX_ERROR_INVALID;
    /* Read-only queries never mutate provider state or hardware. */
    nx_stm32u575_watchdog_state_t observed = *p;
    observe_option(&observed);
    *state = observed.state;
    state->debug_freeze = (NX_U575_READ(&p->debug->APB1FZR1) &
                           DBGMCU_APB1FZR1_DBG_IWDG_STOP) != 0U;
    if (state->enabled && state->maximum_timeout_us == 0U)
        return NX_ERROR_UNSUPPORTED;
    return NX_SUCCESS;
}

const nx_watchdog_ops_t nx_stm32u575_watchdog_ops = {enable, feed, current};

/** \brief U5 has no separate POR cause; never invent it from BOR alone. */
uint32_t nx_reset_cause(void) {
    uint32_t raw = g_nx_stm32u575_system.reset_cause;
    uint32_t result = 0U;
    if ((raw & RCC_CSR_PINRSTF) != 0U)
        result |= NX_RESET_PIN;
    if ((raw & RCC_CSR_BORRSTF) != 0U)
        result |= NX_RESET_BROWNOUT;
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

nx_result_t nx_stm32u575_watchdog_prepare(nx_stm32u575_watchdog_state_t* port) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (port->state.enabled || port->lsi_added || port->debug_pending)
        return NX_ERROR_BUSY;
    port->registers = IWDG;
    port->rcc = RCC;
    port->flash = FLASH;
    port->debug = DBGMCU;
    port->poll_limit = 1000000U;
    return NX_SUCCESS;
}
