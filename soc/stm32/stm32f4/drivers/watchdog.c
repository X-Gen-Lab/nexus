/**
 * \file            watchdog.c
 * \brief           F407 irreversible IWDG and retained preparation ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_basic_cold.h"
#include "stm32f407_provider.h"
#include "stm32f4xx_ll_iwdg.h"

nx_result_t nx_stm32_watchdog_prepare(nx_stm32_watchdog_state_t* port,
                                      uint32_t poll_limit) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || poll_limit == 0U)
        return NX_ERROR_INVALID;
    if (port->state.enabled || port->state.irreversible ||
        port->preparation_retained || port->lsi_owned || port->debug_owned)
        return NX_ERROR_BUSY;
    port->registers = IWDG;
    port->flash = FLASH;
    port->debug = DBGMCU;
    port->rcc = RCC;
    port->poll_limit = poll_limit;
    return NX_SUCCESS;
}

#ifndef NX_STM32_IO_POLL
#define NX_STM32_IO_POLL(kind, port) ((void)(kind), (void)(port))
#endif
#ifndef NX_STM32_WATCHDOG_WRITE
#define NX_STM32_WATCHDOG_WRITE(address, value) (*(address) = (value))
#endif

/** \brief Validate the private fixed instance before any MMIO access. */
static bool valid(const nx_stm32_watchdog_state_t* port) {
    return port != NULL && port->registers != NULL && port->rcc != NULL &&
           port->flash != NULL && port->debug != NULL && port->poll_limit != 0U;
}

/** \brief Reject output aliases before writing state or hardware. */
static bool output_valid(const nx_stm32_watchdog_state_t* port,
                         const nx_watchdog_state_t* state) {
    uintptr_t out = (uintptr_t)state;
    size_t size = sizeof(*state);
    return state != NULL && out % _Alignof(nx_watchdog_state_t) == 0U &&
           out <= UINTPTR_MAX - size &&
           !(out < (uintptr_t)port + sizeof(*port) &&
             (uintptr_t)port < out + size) &&
           !(out < (uintptr_t)port->registers + sizeof(*port->registers) &&
             (uintptr_t)port->registers < out + size) &&
           !(out < (uintptr_t)port->rcc + sizeof(*port->rcc) &&
             (uintptr_t)port->rcc < out + size) &&
           !(out < (uintptr_t)port->flash + sizeof(*port->flash) &&
             (uintptr_t)port->flash < out + size) &&
           !(out < (uintptr_t)port->debug + sizeof(*port->debug) &&
             (uintptr_t)port->debug < out + size);
}

/** \brief Preserve option-started effects without unconditional LSI limits. */
static nx_watchdog_state_t observed(const nx_stm32_watchdog_state_t* port) {
    nx_watchdog_state_t state = port->state;
    if ((port->flash->OPTCR & FLASH_OPTCR_WDG_SW) == 0U) {
        state.enabled = true;
        state.irreversible = true;
    }
    /* DS8626 Rev12 Table35's 17..47kHz characterization is specified only
     * at VDD=3V and TA=-40..105C. No authored operating profile establishes
     * these conditions here, so neither pending nor steady settings prove
     * physical bounds. Nominal programming remains32kHz per RM0090. */
    state.minimum_timeout_us = 0U;
    state.maximum_timeout_us = 0U;
    state.debug_freeze =
        (port->debug->APB1FZ & DBGMCU_APB1_FZ_DBG_IWDG_STOP) != 0U;
    return state;
}

/** \brief Restore owned preparation fields, retaining failed cleanup. */
static nx_result_t restore_preparation(nx_stm32_watchdog_state_t* port) {
    if (port->debug_owned) {
        NX_STM32_WATCHDOG_WRITE(
            &port->debug->APB1FZ,
            (port->debug->APB1FZ & ~(uint32_t)DBGMCU_APB1_FZ_DBG_IWDG_STOP) |
                (port->saved_debug & DBGMCU_APB1_FZ_DBG_IWDG_STOP));
        nx_arch_dsb();
        if ((port->debug->APB1FZ & DBGMCU_APB1_FZ_DBG_IWDG_STOP) !=
            (port->saved_debug & DBGMCU_APB1_FZ_DBG_IWDG_STOP)) {
            return NX_ERROR_IO;
        }
        port->debug_owned = false;
    }
    if (port->lsi_owned) {
        NX_STM32_WATCHDOG_WRITE(&port->rcc->CSR,
                                port->rcc->CSR & ~(uint32_t)RCC_CSR_LSION);
        nx_arch_dsb();
        if ((port->rcc->CSR & RCC_CSR_LSION) != 0U) {
            return NX_ERROR_IO;
        }
        port->lsi_owned = false;
    }
    port->preparation_retained = false;
    return NX_SUCCESS;
}

/** \brief Retry preparation cleanup; enabled counting is irreversible. */
nx_result_t nx_stm32_watchdog_stop(nx_stm32_watchdog_state_t* port) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    port->state = observed(port);
    if (port->state.enabled) {
        return NX_ERROR_UNSUPPORTED;
    }
    return restore_preparation(port);
}

/** \brief Mark irreversible activation before commands with no readback. */
nx_result_t nx_stm32_watchdog_enable(void* context, uint32_t timeout_us,
                                     bool debug_freeze,
                                     nx_watchdog_state_t* state) {
    nx_stm32_watchdog_state_t* port = context;
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(port) || !output_valid(port, state)) {
        return NX_ERROR_INVALID;
    }
    port->state = observed(port);
    *state = port->state;
    if (port->state.enabled) {
        return NX_ERROR_STATE;
    }
    if (port->preparation_retained) {
        return NX_ERROR_BUSY;
    }
    if (timeout_us < 125U || timeout_us > 32768000U) {
        return NX_ERROR_INVALID;
    }
    if (LL_IWDG_IsReady(port->registers) == 0U) {
        return NX_ERROR_BUSY;
    }
    uint32_t divisor = 4U;
    uint32_t prescaler = 0U;
    uint32_t count;
    for (;;) {
        uint64_t unit = (uint64_t)divisor * 1000000U;
        count = (uint32_t)(((uint64_t)timeout_us * 32000U + unit - 1U) / unit);
        if (count <= 4096U) {
            break;
        }
        divisor *= 2U;
        ++prescaler;
    }
    port->lsi_owned = (port->rcc->CSR & RCC_CSR_LSION) == 0U;
    port->saved_debug = port->debug->APB1FZ;
    port->preparation_retained = true;
    NX_STM32_WATCHDOG_WRITE(&port->rcc->CSR, port->rcc->CSR | RCC_CSR_LSION);
    uint32_t remaining = port->poll_limit;
    while ((port->rcc->CSR & RCC_CSR_LSIRDY) == 0U && remaining-- != 0U) {
        NX_STM32_IO_POLL(4U, port);
    }
    if ((port->rcc->CSR & RCC_CSR_LSIRDY) == 0U) {
        (void)restore_preparation(port);
        *state = observed(port);
        return NX_ERROR_IO;
    }
    uint32_t debug = port->debug->APB1FZ;
    uint32_t selected = debug_freeze
                            ? debug | DBGMCU_APB1_FZ_DBG_IWDG_STOP
                            : debug & ~(uint32_t)DBGMCU_APB1_FZ_DBG_IWDG_STOP;
    port->debug_owned = (debug & DBGMCU_APB1_FZ_DBG_IWDG_STOP) !=
                        (selected & DBGMCU_APB1_FZ_DBG_IWDG_STOP);
    NX_STM32_WATCHDOG_WRITE(&port->debug->APB1FZ, selected);
    nx_arch_dsb();
    if ((port->debug->APB1FZ & DBGMCU_APB1_FZ_DBG_IWDG_STOP) !=
        (selected & DBGMCU_APB1_FZ_DBG_IWDG_STOP)) {
        (void)restore_preparation(port);
        *state = observed(port);
        return NX_ERROR_IO;
    }
    /* CCCC starts a FFF counter even if inactive RLR was changed. The two
     * shadow registers latch independently; never derive a precise failure
     * timeout from their visible old/new products. */
    port->state.enabled = true;
    port->state.irreversible = true;
    LL_IWDG_Enable(port->registers);
    LL_IWDG_EnableWriteAccess(port->registers);
    LL_IWDG_SetPrescaler(port->registers, prescaler);
    LL_IWDG_SetReloadCounter(port->registers, count - 1U);
    remaining = port->poll_limit;
    while (LL_IWDG_IsReady(port->registers) == 0U && remaining-- != 0U) {
        NX_STM32_IO_POLL(4U, port);
    }
    port->state = observed(port);
    *state = port->state;
    if (LL_IWDG_IsReady(port->registers) == 0U ||
        port->registers->PR != prescaler ||
        port->registers->RLR != count - 1U) {
        return NX_ERROR_IO;
    }
    LL_IWDG_ReloadCounter(port->registers);
    return NX_SUCCESS;
}

/** \brief Reload only a known enabled owner; task/IRQ feeding is bounded. */
nx_result_t nx_stm32_watchdog_feed(void* context) {
    nx_stm32_watchdog_state_t* port = context;
    if (!nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(port)) {
        return NX_ERROR_INVALID;
    }
    port->state = observed(port);
    if (!port->state.enabled) {
        return NX_ERROR_STATE;
    }
    LL_IWDG_ReloadCounter(port->registers);
    return NX_SUCCESS;
}

/** \brief Unknown timeout retains the observed enable and debug facts. */
nx_result_t nx_stm32_watchdog_state(const void* context,
                                    nx_watchdog_state_t* state) {
    const nx_stm32_watchdog_state_t* port = context;
    if (!nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    if (!valid(port) || !output_valid(port, state)) {
        return NX_ERROR_INVALID;
    }
    *state = observed(port);
    return state->enabled ? NX_ERROR_UNSUPPORTED : NX_SUCCESS;
}

/** \brief Map captured boot flags without clearing or inventing product policy.
 */
uint32_t nx_reset_cause(void) {
    uint32_t raw = g_nx_stm32_system.reset_cause;
    uint32_t result = 0U;
    if ((raw & RCC_CSR_PORRSTF) != 0U) {
        result |= NX_RESET_POWER_ON;
    }
    if ((raw & RCC_CSR_PINRSTF) != 0U) {
        result |= NX_RESET_PIN;
    }
    if ((raw & RCC_CSR_SFTRSTF) != 0U) {
        result |= NX_RESET_SOFTWARE;
    }
    if ((raw & RCC_CSR_IWDGRSTF) != 0U) {
        result |= NX_RESET_IWDG;
    }
    if ((raw & RCC_CSR_WWDGRSTF) != 0U) {
        result |= NX_RESET_WWDG;
    }
    if ((raw & RCC_CSR_LPWRRSTF) != 0U) {
        result |= NX_RESET_LOW_POWER;
    }
    if ((raw & RCC_CSR_BORRSTF) != 0U) {
        result |= NX_RESET_BROWNOUT;
    }
    return result;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_watchdog_ops_t nx_stm32_watchdog_ops = {
    .enable = nx_stm32_watchdog_enable,
    .feed = nx_stm32_watchdog_feed,
    .state = nx_stm32_watchdog_state,
};
