/**
 * \file            watchdog.c
 * \brief           H743 bounded independent watchdog with retained effects
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_provider.h"

/** \brief Preserve unknown LSI envelope; no unreviewed timing guarantee. */
static void bounds(nx_watchdog_state_t* state, uint32_t divisor,
                   uint32_t count) {
    (void)divisor;
    (void)count;
    state->minimum_timeout_us = 0U;
    state->maximum_timeout_us = 0U;
}
static bool valid(const nx_stm32h743_watchdog_state_t* p) {
    return p != NULL && p->registers != NULL && p->rcc != NULL &&
           p->flash != NULL && p->debug != NULL && p->poll_limit != 0U;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool valid_output(const nx_stm32h743_watchdog_state_t* p,
                         const nx_watchdog_state_t* out) {
    return out != NULL && (uintptr_t)out <= UINTPTR_MAX - sizeof(*out) &&
           !overlap(out, sizeof(*out), p, sizeof(*p)) &&
           !overlap(out, sizeof(*out), p->registers, sizeof(*p->registers)) &&
           !overlap(out, sizeof(*out), p->rcc, sizeof(*p->rcc)) &&
           !overlap(out, sizeof(*out), p->flash, sizeof(*p->flash)) &&
           !overlap(out, sizeof(*out), p->debug, sizeof(*p->debug));
}
/** \brief Option-byte activation survives errors and cannot be undone. */
static nx_watchdog_state_t observed(const nx_stm32h743_watchdog_state_t* p) {
    nx_watchdog_state_t result = p->state;
    if ((!result.enabled || result.minimum_timeout_us == 0U) &&
        (NX_H743_READ(&p->flash->OPTSR_CUR) & FLASH_OPTSR_IWDG1_SW) == 0U) {
        result.enabled = true;
        result.irreversible = true;
        if (NX_H743_READ(&p->registers->SR) != 0U) {
            result.minimum_timeout_us = 0U;
            result.maximum_timeout_us = 0U;
        } else {
            uint32_t prescaler = NX_H743_READ(&p->registers->PR) & 7U;
            bounds(&result, prescaler > 6U ? 256U : 4U << prescaler,
                   (NX_H743_READ(&p->registers->RLR) & 4095U) + 1U);
        }
    }
    result.debug_freeze =
        (NX_H743_READ(&p->debug->APB4FZ1) & DBGMCU_APB4FZ1_DBG_IWDG1) != 0U;
    return result;
}
nx_result_t nx_stm32h743_watchdog_stop(nx_stm32h743_watchdog_state_t* p) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (observed(p).enabled)
        return NX_ERROR_STATE;
    bool clean = true;
    if (p->debug_pending) {
        uint32_t mask = DBGMCU_APB4FZ1_DBG_IWDG1;
        uint32_t restored = (NX_H743_READ(&p->debug->APB4FZ1) & ~mask) |
                            (p->saved_debug & mask);
        NX_H743_WRITE(&p->debug->APB4FZ1, restored);
        nx_arch_dsb();
        if ((NX_H743_READ(&p->debug->APB4FZ1) & mask) !=
            (p->saved_debug & mask))
            clean = false;
        else
            p->debug_pending = false;
    }
    if (p->lsi_added) {
        NX_H743_WRITE(&p->rcc->CSR,
                      NX_H743_READ(&p->rcc->CSR) & ~(uint32_t)RCC_CSR_LSION);
        nx_arch_dsb();
        if ((NX_H743_READ(&p->rcc->CSR) & RCC_CSR_LSION) != 0U)
            clean = false;
        else
            p->lsi_added = false;
    }
    return clean ? NX_SUCCESS : NX_ERROR_IO;
}
static nx_result_t enable(void* context, uint32_t timeout, bool freeze,
                          nx_watchdog_state_t* out) {
    nx_stm32h743_watchdog_state_t* p = context;
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !valid_output(p, out))
        return NX_ERROR_INVALID;
    p->state = observed(p);
    *out = p->state;
    if (p->state.enabled)
        return NX_ERROR_STATE;
    if (p->lsi_added || p->debug_pending)
        return NX_ERROR_BUSY;
    if (timeout < 125U || timeout > 32768000U)
        return NX_ERROR_INVALID;
    if (NX_H743_READ(&p->registers->WINR) != 4095U)
        return NX_ERROR_UNSUPPORTED;
    if (NX_H743_READ(&p->registers->SR) != 0U)
        return NX_ERROR_BUSY;
    uint32_t divisor = 4U;
    uint32_t prescaler = 0U;
    uint32_t count;
    for (;;) {
        count = (uint32_t)(((uint64_t)timeout * 32000U +
                            (uint64_t)divisor * 1000000U - 1U) /
                           ((uint64_t)divisor * 1000000U));
        if (count <= 4096U)
            break;
        divisor *= 2U;
        ++prescaler;
    }
    p->lsi_added = (NX_H743_READ(&p->rcc->CSR) & RCC_CSR_LSION) == 0U;
    NX_H743_WRITE(&p->rcc->CSR, NX_H743_READ(&p->rcc->CSR) | RCC_CSR_LSION);
    uint32_t remaining = p->poll_limit;
    while ((NX_H743_READ(&p->rcc->CSR) & RCC_CSR_LSIRDY) == 0U &&
           remaining-- != 0U)
        NX_H743_WATCHDOG_POLL(p);
    if ((NX_H743_READ(&p->rcc->CSR) & RCC_CSR_LSIRDY) == 0U) {
        (void)nx_stm32h743_watchdog_stop(p);
        return NX_ERROR_IO;
    }
    p->saved_debug = NX_H743_READ(&p->debug->APB4FZ1);
    uint32_t debug = freeze
                         ? p->saved_debug | DBGMCU_APB4FZ1_DBG_IWDG1
                         : p->saved_debug & ~(uint32_t)DBGMCU_APB4FZ1_DBG_IWDG1;
    p->debug_pending = true;
    NX_H743_WRITE(&p->debug->APB4FZ1, debug);
    nx_arch_dsb();
    if (NX_H743_READ(&p->debug->APB4FZ1) != debug) {
        (void)nx_stm32h743_watchdog_stop(p);
        return NX_ERROR_IO;
    }
    uint32_t old_prescaler = NX_H743_READ(&p->registers->PR) & 7U;
    uint32_t old_divisor = old_prescaler > 6U ? 256U : 4U << old_prescaler;
    /* RM0433 IWDG1: CCCC starts at reset0xFFF, not an inactive RLR. */
    uint32_t old_count = 4096U;
    p->state.enabled = true;
    p->state.irreversible = true;
    p->state.debug_freeze = freeze;
    /* A failed update can leave either old/new prescaler and reload. */
    nx_watchdog_state_t shortest = {0};
    bounds(&shortest, old_divisor < divisor ? old_divisor : divisor,
           old_count < count ? old_count : count);
    bounds(&p->state, old_divisor > divisor ? old_divisor : divisor,
           old_count > count ? old_count : count);
    p->state.minimum_timeout_us = shortest.minimum_timeout_us;
    /* The start command has no readable acknowledgement or undo. Publish its
     * conservative responsibility before exposing it to the peripheral. */
    nx_arch_dmb();
    NX_H743_WRITE(&p->registers->KR, 0xCCCCU);
    NX_H743_WRITE(&p->registers->KR, 0x5555U);
    NX_H743_WRITE(&p->registers->PR, prescaler);
    NX_H743_WRITE(&p->registers->RLR, count - 1U);
    remaining = p->poll_limit;
    while (NX_H743_READ(&p->registers->SR) != 0U && remaining-- != 0U)
        NX_H743_WATCHDOG_POLL(p);
    *out = p->state;
    if (NX_H743_READ(&p->registers->SR) != 0U ||
        NX_H743_READ(&p->registers->PR) != prescaler ||
        NX_H743_READ(&p->registers->RLR) != count - 1U)
        return NX_ERROR_IO;
    NX_H743_WRITE(&p->registers->KR, 0xAAAAU);
    bounds(&p->state, divisor, count);
    *out = p->state;
    return NX_SUCCESS;
}
static nx_result_t feed(void* context) {
    nx_stm32h743_watchdog_state_t* p = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    p->state = observed(p);
    if (!p->state.enabled)
        return NX_ERROR_STATE;
    if (NX_H743_READ(&p->registers->WINR) != 4095U)
        return NX_ERROR_UNSUPPORTED;
    NX_H743_WRITE(&p->registers->KR, 0xAAAAU);
    return NX_SUCCESS;
}
static nx_result_t state(const void* context, nx_watchdog_state_t* out) {
    const nx_stm32h743_watchdog_state_t* p = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !valid_output(p, out))
        return NX_ERROR_INVALID;
    *out = observed(p);
    return out->enabled && out->minimum_timeout_us == 0U ? NX_ERROR_UNSUPPORTED
                                                         : NX_SUCCESS;
}
const nx_watchdog_ops_t nx_stm32h743_watchdog_ops = {enable, feed, state};

uint32_t nx_reset_cause(void) {
    uint32_t raw = g_nx_stm32h743_system.reset_cause;
    uint32_t result = 0U;
    if ((raw & RCC_RSR_PORRSTF) != 0U)
        result |= NX_RESET_POWER_ON;
    if ((raw & RCC_RSR_BORRSTF) != 0U)
        result |= NX_RESET_BROWNOUT;
    if ((raw & RCC_RSR_PINRSTF) != 0U)
        result |= NX_RESET_PIN;
    if ((raw & RCC_RSR_SFTRSTF) != 0U)
        result |= NX_RESET_SOFTWARE;
    if ((raw & RCC_RSR_IWDG1RSTF) != 0U)
        result |= NX_RESET_IWDG;
    if ((raw & RCC_RSR_WWDG1RSTF) != 0U)
        result |= NX_RESET_WWDG;
    if ((raw & RCC_RSR_LPWRRSTF) != 0U)
        result |= NX_RESET_LOW_POWER;
    return result;
}
