/**
 * \file            watchdog.c
 * \brief           GD32 watchdog programming and retained preparation effects
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "private/system.h"

#ifndef NX_GD32_WATCHDOG_WRITE
#define NX_GD32_WATCHDOG_WRITE(address, value) (*(address) = (value))
#endif
#ifndef NX_GD32_WATCHDOG_POLL
#define NX_GD32_WATCHDOG_POLL(port) ((void)(port))
#endif
#ifndef NX_GD32_WATCHDOG_POLL_LIMIT
#define NX_GD32_WATCHDOG_POLL_LIMIT 1000000U
#endif

static nx_gd32_watchdog_state_t* s_watchdog;
extern uint32_t g_gd32_reset_flags;

/** \brief Reject task operations before any register access. */
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_gd32_in_isr() &&
           !nx_gd32_irq_masked();
}

/** \brief Reject output writes into provider storage or owned MMIO. */
static bool output_valid(const nx_gd32_watchdog_state_t* port,
                         const nx_watchdog_state_t* state) {
    uintptr_t out = (uintptr_t)state;
    size_t size = sizeof(*state);
    return state != NULL && out % _Alignof(nx_watchdog_state_t) == 0U &&
           out <= UINTPTR_MAX - size &&
           !(out < (uintptr_t)port + sizeof(*port) &&
             (uintptr_t)port < out + size) &&
           !(out < (uintptr_t)&FWDGT_CTL + 16U &&
             (uintptr_t)&FWDGT_CTL < out + size) &&
           !(out < (uintptr_t)&RCU_CTL + 256U &&
             (uintptr_t)&RCU_CTL < out + size) &&
           !(out < (uintptr_t)&FMC_WS + 256U &&
             (uintptr_t)&FMC_WS < out + size) &&
           !(out < (uintptr_t)&DBG_ID + 16U && (uintptr_t)&DBG_ID < out + size);
}

/** \brief Report effects without inventing guaranteed IRC32K bounds. */
static nx_watchdog_state_t observed(const nx_gd32_watchdog_state_t* port) {
    nx_watchdog_state_t state = port->state;
    if ((FMC_OBCTL0 & FMC_OBCTL0_NWDG_HW) == 0U) {
        state.enabled = true;
        state.irreversible = true;
    }
    /* DS Rev2.2 Table4-19 specifies only typ32kHz, no Min/Max. */
    state.minimum_timeout_us = 0U;
    state.maximum_timeout_us = 0U;
    state.debug_freeze = (DBG_CTL1 & DBG_CTL1_FWDGT_HOLD) != 0U;
    return state;
}

/** \brief Restore acquired fields; failed restoration keeps its owner. */
static nx_result_t restore_preparation(nx_gd32_watchdog_state_t* port) {
    if (port->debug_owned) {
        NX_GD32_WATCHDOG_WRITE(&DBG_CTL1,
                               (DBG_CTL1 & ~(uint32_t)DBG_CTL1_FWDGT_HOLD) |
                                   (port->saved_debug & DBG_CTL1_FWDGT_HOLD));
        nx_gd32_peripheral_barrier();
        if ((DBG_CTL1 & DBG_CTL1_FWDGT_HOLD) !=
            (port->saved_debug & DBG_CTL1_FWDGT_HOLD)) {
            return NX_ERROR_IO;
        }
        port->debug_owned = false;
    }
    if (port->lsi_owned) {
        NX_GD32_WATCHDOG_WRITE(&RCU_RSTSCK,
                               RCU_RSTSCK & ~(uint32_t)RCU_RSTSCK_IRC32KEN);
        nx_gd32_peripheral_barrier();
        if ((RCU_RSTSCK & RCU_RSTSCK_IRC32KEN) != 0U) {
            return NX_ERROR_IO;
        }
        port->lsi_owned = false;
    }
    port->preparation_retained = false;
    return NX_SUCCESS;
}

/** \brief Assemble one owner without enabling the irreversible counter. */
nx_result_t nx_gd32_watchdog_initialize(nx_gd32_watchdog_state_t* port) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (s_watchdog != NULL) {
        return NX_ERROR_BUSY;
    }
    *port = (nx_gd32_watchdog_state_t){.initialized = true};
    port->state = observed(port);
    s_watchdog = port;
    return NX_SUCCESS;
}

/** \brief Prepare reversible fields, then own activation before programming. */
nx_result_t nx_gd32_watchdog_enable(void* context, uint32_t timeout_us,
                                    bool debug_freeze,
                                    nx_watchdog_state_t* state) {
    nx_gd32_watchdog_state_t* port = context;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port == NULL || port != s_watchdog || !port->initialized ||
        !output_valid(port, state)) {
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
    if (timeout_us < 32U || timeout_us > 32760031U) {
        return NX_ERROR_INVALID;
    }
    if (FWDGT_STAT != 0U) {
        return NX_ERROR_BUSY;
    }
    uint32_t divisor = 4U;
    uint8_t prescaler = 0U;
    uint64_t clocks = ((uint64_t)timeout_us * 32000U + 999999U) / 1000000U;
    uint32_t reload;
    for (;;) {
        /* UM Rev3.3 Table16-1: (RLD*divider+1)/IRC32K; not the
         * STM32 (RLR+1)*divider formula. Round nominal duration upward. */
        reload = (uint32_t)((clocks - 1U + divisor - 1U) / divisor);
        if (reload <= 4095U) {
            break;
        }
        divisor *= 2U;
        ++prescaler;
    }
    port->saved_debug = DBG_CTL1;
    port->lsi_owned = (RCU_RSTSCK & RCU_RSTSCK_IRC32KEN) == 0U;
    port->preparation_retained = true;
    NX_GD32_WATCHDOG_WRITE(&RCU_RSTSCK, RCU_RSTSCK | RCU_RSTSCK_IRC32KEN);
    uint32_t remaining = NX_GD32_WATCHDOG_POLL_LIMIT;
    while ((RCU_RSTSCK & RCU_RSTSCK_IRC32KSTB) == 0U && remaining-- != 0U) {
        NX_GD32_WATCHDOG_POLL(port);
    }
    if ((RCU_RSTSCK & RCU_RSTSCK_IRC32KSTB) == 0U) {
        (void)restore_preparation(port);
        *state = observed(port);
        return NX_ERROR_IO;
    }
    uint32_t debug = DBG_CTL1;
    uint32_t selected = debug_freeze ? debug | DBG_CTL1_FWDGT_HOLD
                                     : debug & ~(uint32_t)DBG_CTL1_FWDGT_HOLD;
    port->debug_owned =
        (debug & DBG_CTL1_FWDGT_HOLD) != (selected & DBG_CTL1_FWDGT_HOLD);
    NX_GD32_WATCHDOG_WRITE(&DBG_CTL1, selected);
    nx_gd32_peripheral_barrier();
    if ((DBG_CTL1 & DBG_CTL1_FWDGT_HOLD) != (selected & DBG_CTL1_FWDGT_HOLD)) {
        (void)restore_preparation(port);
        *state = observed(port);
        return NX_ERROR_IO;
    }
    /* The official combined fwdgt_config starts the counter before setup.
     * Own that same irreversible enable explicitly instead of treating the
     * helper as reversible preparation. Disabled shadow progress is unproven.
     */
    port->state.enabled = true;
    port->state.irreversible = true;
    NX_GD32_WATCHDOG_WRITE(&FWDGT_CTL, FWDGT_KEY_ENABLE);
    NX_GD32_WATCHDOG_WRITE(&FWDGT_CTL, FWDGT_WRITEACCESS_ENABLE);
    NX_GD32_WATCHDOG_WRITE(&FWDGT_PSC, prescaler);
    remaining = NX_GD32_WATCHDOG_POLL_LIMIT;
    while ((FWDGT_STAT & FWDGT_STAT_PUD) != 0U && remaining-- != 0U) {
        NX_GD32_WATCHDOG_POLL(port);
    }
    if ((FWDGT_STAT & FWDGT_STAT_PUD) != 0U || FWDGT_PSC != prescaler) {
        port->state = observed(port);
        *state = port->state;
        return NX_ERROR_IO;
    }
    NX_GD32_WATCHDOG_WRITE(&FWDGT_RLD, reload);
    remaining = NX_GD32_WATCHDOG_POLL_LIMIT;
    while ((FWDGT_STAT & FWDGT_STAT_RUD) != 0U && remaining-- != 0U) {
        NX_GD32_WATCHDOG_POLL(port);
    }
    nx_gd32_peripheral_barrier();
    port->state = observed(port);
    *state = port->state;
    if (FWDGT_STAT != 0U || FWDGT_PSC != prescaler || FWDGT_RLD != reload ||
        port->state.debug_freeze != debug_freeze) {
        return NX_ERROR_IO;
    }
    NX_GD32_WATCHDOG_WRITE(&FWDGT_CTL, FWDGT_KEY_RELOAD);
    return NX_SUCCESS;
}

/** \brief Feed a known owner, including hardware-option automatic enable. */
nx_result_t nx_gd32_watchdog_feed(void* context) {
    nx_gd32_watchdog_state_t* port = context;
    if (!nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    if (port == NULL || port != s_watchdog || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    port->state = observed(port);
    if (!port->state.enabled) {
        return NX_ERROR_STATE;
    }
    NX_GD32_WATCHDOG_WRITE(&FWDGT_CTL, FWDGT_KEY_RELOAD);
    return NX_SUCCESS;
}

/** \brief Unknown physical timeout never hides an observed enabled effect. */
nx_result_t nx_gd32_watchdog_state(const void* context,
                                   nx_watchdog_state_t* state) {
    const nx_gd32_watchdog_state_t* port = context;
    if (!nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    if (port == NULL || port != s_watchdog || !port->initialized ||
        !output_valid(port, state)) {
        return NX_ERROR_INVALID;
    }
    *state = observed(port);
    return state->enabled ? NX_ERROR_UNSUPPORTED : NX_SUCCESS;
}

/** \brief           Decode boot-latched flags without clearing or rebooting. */
uint32_t nx_reset_cause(void) {
    uint32_t raw = g_gd32_reset_flags;
    return (raw & RCU_RSTSCK_PORRSTF ? NX_RESET_POWER_ON : 0u) |
           (raw & RCU_RSTSCK_EPRSTF ? NX_RESET_PIN : 0u) |
           (raw & RCU_RSTSCK_SWRSTF ? NX_RESET_SOFTWARE : 0u) |
           (raw & RCU_RSTSCK_FWDGTRSTF ? NX_RESET_IWDG : 0u) |
           (raw & RCU_RSTSCK_WWDGTRSTF ? NX_RESET_WWDG : 0u) |
           (raw & RCU_RSTSCK_LPRSTF ? NX_RESET_LOW_POWER : 0u) |
           (raw & RCU_RSTSCK_BORRSTF ? NX_RESET_BROWNOUT : 0u);
}

/** \brief Retry preparation cleanup, never erase an enabled effect. */
nx_result_t nx_gd32_watchdog_stop(nx_gd32_watchdog_state_t* port) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port == NULL || port != s_watchdog || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    port->state = observed(port);
    if (port->state.enabled) {
        return NX_ERROR_UNSUPPORTED;
    }
    nx_result_t result = restore_preparation(port);
    if (result == NX_SUCCESS) {
        port->initialized = false;
        s_watchdog = NULL;
    }
    return result;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_watchdog_ops_t nx_gd32_watchdog_ops = {
    .enable = nx_gd32_watchdog_enable,
    .feed = nx_gd32_watchdog_feed,
    .state = nx_gd32_watchdog_state,
};
