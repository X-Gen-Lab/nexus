/**
 * \file            watchdog.c
 * \brief           Irreversible IWDG activation and captured reset cause facts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_provider.h"

#ifndef NX_STM32_IO_POLL
#define NX_STM32_IO_POLL(kind, port) ((void)(kind), (void)(port))
#endif

/** \brief Derive conservative LSI bounds from maintained 17--47 kHz limits. */
static void timeout_bounds(nx_watchdog_state_t* state, uint32_t divisor,
                           uint32_t count) {
    uint64_t clocks_us = (uint64_t)divisor * count * 1000000U;
    state->minimum_timeout_us = (uint32_t)(clocks_us / 47000U);
    state->maximum_timeout_us = (uint32_t)((clocks_us + 16999U) / 17000U);
}

/** \brief Identify hardware-option automatic activation before any failure. */
static void observe_option(nx_watchdog_port_t* port) {
    if (port->flash != NULL &&
        (port->flash->OPTCR & FLASH_OPTCR_WDG_SW) == 0U) {
        port->state.enabled = true;
        port->state.irreversible = true;
        uint32_t prescaler = port->registers->PR & 7U;
        timeout_bounds(&port->state, prescaler > 6U ? 256U : 4U << prescaler,
                       (port->registers->RLR & 0xFFFU) + 1U);
    }
}

/** \brief Enable and retain responsibility even if post-activation setup fails.
 */
nx_result_t nx_watchdog_port_enable(nx_watchdog_port_t* port,
                                    uint32_t timeout_us, bool debug_freeze,
                                    nx_watchdog_state_t* state) {
    if (port == NULL || port->registers == NULL || port->rcc == NULL ||
        port->debug == NULL || state == NULL || port->poll_limit == 0U) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    observe_option(port);
    *state = port->state;
    if (port->state.enabled) {
        return NX_ERROR_STATE;
    }
    if (timeout_us < 125U || timeout_us > 32768000U) {
        return NX_ERROR_INVALID;
    }
    uint32_t divisor = 4U;
    uint32_t prescaler = 0U;
    uint32_t count = 0U;
    for (;;) {
        count = (uint32_t)(((uint64_t)timeout_us * 32000U +
                            (uint64_t)divisor * 1000000U - 1U) /
                           ((uint64_t)divisor * 1000000U));
        if (count <= 4096U) {
            break;
        }
        divisor *= 2U;
        ++prescaler;
    }
    port->rcc->CSR |= RCC_CSR_LSION;
    uint32_t remaining = port->poll_limit;
    while ((port->rcc->CSR & RCC_CSR_LSIRDY) == 0U && remaining-- != 0U) {
        NX_STM32_IO_POLL(4U, port);
    }
    if ((port->rcc->CSR & RCC_CSR_LSIRDY) == 0U) {
        port->rcc->CSR &= ~(uint32_t)RCC_CSR_LSION;
        return NX_ERROR_IO;
    }
    if (debug_freeze) {
        port->debug->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP;
    } else {
        port->debug->APB1FZ &= ~(uint32_t)DBGMCU_APB1_FZ_DBG_IWDG_STOP;
    }
    nx_watchdog_state_t old = {0};
    uint32_t old_prescaler = port->registers->PR & 7U;
    uint32_t old_divisor = old_prescaler > 6U ? 256U : 4U << old_prescaler;
    timeout_bounds(&old, old_divisor, (port->registers->RLR & 0xFFFU) + 1U);
    port->registers->KR = 0xCCCCU;
    port->state.enabled = true;
    port->state.irreversible = true;
    port->state.debug_freeze = debug_freeze;
    timeout_bounds(&port->state, divisor, count);
    port->registers->KR = 0x5555U;
    port->registers->PR = prescaler;
    port->registers->RLR = count - 1U;
    remaining = port->poll_limit;
    while (port->registers->SR != 0U && remaining-- != 0U) {
        NX_STM32_IO_POLL(4U, port);
    }
    *state = port->state;
    if (port->registers->SR != 0U) {
        if (old.minimum_timeout_us < port->state.minimum_timeout_us) {
            port->state.minimum_timeout_us = old.minimum_timeout_us;
        }
        if (old.maximum_timeout_us > port->state.maximum_timeout_us) {
            port->state.maximum_timeout_us = old.maximum_timeout_us;
        }
        *state = port->state;
        return NX_ERROR_IO;
    }
    port->registers->KR = 0xAAAAU;
    return NX_SUCCESS;
}

/** \brief Reload only a known active watchdog owned by this platform image. */
nx_result_t nx_watchdog_port_feed(nx_watchdog_port_t* port) {
    if (port == NULL || port->registers == NULL) {
        return NX_ERROR_INVALID;
    }
    observe_option(port);
    if (!port->state.enabled) {
        return NX_ERROR_STATE;
    }
    port->registers->KR = 0xAAAAU;
    return NX_SUCCESS;
}

/** \brief Report remaining hardware effect without implying reversibility. */
nx_result_t nx_watchdog_port_state(const nx_watchdog_port_t* port,
                                   nx_watchdog_state_t* state) {
    if (port == NULL || state == NULL) {
        return NX_ERROR_INVALID;
    }
    *state = port->state;
    if (port->flash != NULL &&
        (port->flash->OPTCR & FLASH_OPTCR_WDG_SW) == 0U) {
        state->enabled = true;
        state->irreversible = true;
        uint32_t prescaler = port->registers->PR & 7U;
        timeout_bounds(state, prescaler > 6U ? 256U : 4U << prescaler,
                       (port->registers->RLR & 0xFFFU) + 1U);
        state->debug_freeze =
            (port->debug->APB1FZ & DBGMCU_APB1_FZ_DBG_IWDG_STOP) != 0U;
    }
    return NX_SUCCESS;
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
