/**
 * \file            watchdog.c
 * \brief           GD303 bounded FWDGT activation and retained preparation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f303_provider.h"
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_gd32f303_watchdog_state_t* p) {
    return p != NULL && p->initialized && p->poll_limit != 0U;
}
static bool output_valid(const nx_gd32f303_watchdog_state_t* p,
                         const nx_watchdog_state_t* out) {
    uintptr_t address = (uintptr_t)out;
    size_t size = sizeof(*out);
    return out != NULL && address <= UINTPTR_MAX - size &&
           !(address < (uintptr_t)p + sizeof(*p) &&
             (uintptr_t)p < address + size) &&
           !(address < (uintptr_t)FWDGT + 16U &&
             (uintptr_t)FWDGT < address + size) &&
           !(address < (uintptr_t)RCU_BASE + 0x40U &&
             (uintptr_t)RCU_BASE < address + size) &&
           !(address < (uintptr_t)FMC + 0x40U &&
             (uintptr_t)FMC < address + size) &&
           !(address < (uintptr_t)DBG_BASE + 0x10U &&
             (uintptr_t)DBG_BASE < address + size);
}
static nx_watchdog_state_t observed(const nx_gd32f303_watchdog_state_t* p) {
    nx_watchdog_state_t state = p->state;
    if ((NX_GD303_READ(&FMC_OBSTAT) & 4U) == 0U) {
        state.enabled = true;
        state.irreversible = true;
    }
    /* DS Rev3 Table4-18 gives only typ40kHz. Neither nominal programming
     * nor mixed old/new reload settings establishes physical Min/Max. */
    state.minimum_timeout_us = 0U;
    state.maximum_timeout_us = 0U;
    state.debug_freeze = (NX_GD303_READ(&DBG_CTL0) & DBG_CTL0_FWDGT_HOLD) != 0U;
    return state;
}
static nx_result_t restore(nx_gd32f303_watchdog_state_t* p) {
    bool success = true;
    if (p->debug_owned) {
        NX_GD303_WRITE(&DBG_CTL0, (NX_GD303_READ(&DBG_CTL0) &
                                   ~(uint32_t)DBG_CTL0_FWDGT_HOLD) |
                                      (p->saved_debug & DBG_CTL0_FWDGT_HOLD));
        nx_arch_dsb();
        if ((NX_GD303_READ(&DBG_CTL0) & DBG_CTL0_FWDGT_HOLD) ==
            (p->saved_debug & DBG_CTL0_FWDGT_HOLD))
            p->debug_owned = false;
        else
            success = false;
    }
    if (p->lsi_owned) {
        NX_GD303_WRITE(&RCU_RSTSCK, NX_GD303_READ(&RCU_RSTSCK) &
                                        ~(uint32_t)RCU_RSTSCK_IRC40KEN);
        nx_arch_dsb();
        if ((NX_GD303_READ(&RCU_RSTSCK) & RCU_RSTSCK_IRC40KEN) == 0U)
            p->lsi_owned = false;
        else
            success = false;
    }
    p->preparation_retained = p->debug_owned || p->lsi_owned;
    return success ? NX_SUCCESS : NX_ERROR_IO;
}
nx_result_t nx_gd32f303_watchdog_initialize(nx_gd32f303_watchdog_state_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL || p->poll_limit == 0U)
        return NX_ERROR_INVALID;
    if (p->initialized || p->state.enabled || p->preparation_retained)
        return NX_ERROR_BUSY;
    if ((NX_GD303_READ(&FMC_OBSTAT) & FMC_OBSTAT_OBERR) != 0U)
        return NX_ERROR_IO;
    p->state = observed(p);
    p->initialized = true;
    return NX_SUCCESS;
}
nx_result_t nx_gd32f303_watchdog_stop(nx_gd32f303_watchdog_state_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL)
        return NX_ERROR_INVALID;
    if (!p->initialized && !p->preparation_retained)
        return NX_SUCCESS;
    if ((NX_GD303_READ(&FMC_OBSTAT) & FMC_OBSTAT_OBERR) != 0U)
        return NX_ERROR_IO;
    p->state = observed(p);
    if (p->state.enabled)
        return NX_ERROR_STATE;
    nx_result_t result = restore(p);
    if (result == NX_SUCCESS)
        p->initialized = false;
    return result;
}
static nx_result_t state(const void* context, nx_watchdog_state_t* out) {
    const nx_gd32f303_watchdog_state_t* p = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !output_valid(p, out))
        return NX_ERROR_INVALID;
    if ((NX_GD303_READ(&FMC_OBSTAT) & FMC_OBSTAT_OBERR) != 0U) {
        /* Invalid options cannot establish automatic activation. Preserve
         * only software-known effects; an IO result invalidates option facts.
         */
        *out = p->state;
        out->minimum_timeout_us = out->maximum_timeout_us = 0U;
        out->debug_freeze =
            (NX_GD303_READ(&DBG_CTL0) & DBG_CTL0_FWDGT_HOLD) != 0U;
        return NX_ERROR_IO;
    }
    *out = observed(p);
    return out->enabled ? NX_ERROR_UNSUPPORTED : NX_SUCCESS;
}
static nx_result_t feed(void* context) {
    nx_gd32f303_watchdog_state_t* p = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if ((NX_GD303_READ(&FMC_OBSTAT) & FMC_OBSTAT_OBERR) != 0U)
        return NX_ERROR_IO;
    p->state = observed(p);
    if (!p->state.enabled)
        return NX_ERROR_STATE;
    NX_GD303_WRITE(&FWDGT_CTL, FWDGT_KEY_RELOAD);
    return NX_SUCCESS;
}
static nx_result_t enable(void* context, uint32_t timeout, bool freeze,
                          nx_watchdog_state_t* out) {
    nx_gd32f303_watchdog_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !output_valid(p, out))
        return NX_ERROR_INVALID;
    if ((NX_GD303_READ(&FMC_OBSTAT) & FMC_OBSTAT_OBERR) != 0U) {
        *out = p->state;
        out->minimum_timeout_us = out->maximum_timeout_us = 0U;
        return NX_ERROR_IO;
    }
    p->state = observed(p);
    *out = p->state;
    if (p->state.enabled)
        return NX_ERROR_STATE;
    if (p->preparation_retained)
        return NX_ERROR_BUSY;
    if (timeout < 25U || timeout > 26208025U)
        return NX_ERROR_INVALID;
    if ((NX_GD303_READ(&FMC_OBSTAT) & FMC_OBSTAT_OBERR) != 0U)
        return NX_ERROR_IO;
    if (NX_GD303_READ(&FWDGT_STAT) != 0U)
        return NX_ERROR_BUSY;
    uint32_t divisor = 4U;
    uint32_t prescaler = 0U;
    uint64_t ticks = ((uint64_t)timeout * 40000U + 999999U) / 1000000U;
    uint32_t reload;
    for (;;) {
        /* UM Rev3.4 Table14-1: duration=(RLD*divisor+1)/IRC40K,
         * including RLD0=one IRC40K cycle for every divider. */
        reload = (uint32_t)((ticks - 1U + divisor - 1U) / divisor);
        if (reload <= 4095U)
            break;
        divisor *= 2U;
        ++prescaler;
    }
    p->lsi_owned = (NX_GD303_READ(&RCU_RSTSCK) & RCU_RSTSCK_IRC40KEN) == 0U;
    p->saved_debug = NX_GD303_READ(&DBG_CTL0);
    p->debug_owned = false;
    p->preparation_retained = true;
    NX_GD303_WRITE(&RCU_RSTSCK,
                   NX_GD303_READ(&RCU_RSTSCK) | RCU_RSTSCK_IRC40KEN);
    uint32_t remaining = p->poll_limit;
    while ((NX_GD303_READ(&RCU_RSTSCK) & RCU_RSTSCK_IRC40KSTB) == 0U &&
           remaining-- != 0U)
        NX_GD303_WATCHDOG_POLL();
    if ((NX_GD303_READ(&RCU_RSTSCK) & RCU_RSTSCK_IRC40KSTB) == 0U) {
        (void)restore(p);
        *out = observed(p);
        return NX_ERROR_IO;
    }
    uint32_t selected = freeze
                            ? p->saved_debug | DBG_CTL0_FWDGT_HOLD
                            : p->saved_debug & ~(uint32_t)DBG_CTL0_FWDGT_HOLD;
    p->debug_owned = (selected & DBG_CTL0_FWDGT_HOLD) !=
                     (p->saved_debug & DBG_CTL0_FWDGT_HOLD);
    NX_GD303_WRITE(&DBG_CTL0, selected);
    nx_arch_dsb();
    if ((NX_GD303_READ(&DBG_CTL0) & DBG_CTL0_FWDGT_HOLD) !=
        (selected & DBG_CTL0_FWDGT_HOLD)) {
        (void)restore(p);
        *out = observed(p);
        return NX_ERROR_IO;
    }
    /* The command has no enable readback. Mark irreversible before issuing
     * it; no later error may return preparation leases or pretend disabled. */
    p->state.enabled = true;
    p->state.irreversible = true;
    p->state.debug_freeze = freeze;
    NX_GD303_WRITE(&FWDGT_CTL, FWDGT_KEY_ENABLE);
    NX_GD303_WRITE(&FWDGT_CTL, FWDGT_WRITEACCESS_ENABLE);
    NX_GD303_WRITE(&FWDGT_PSC, prescaler);
    NX_GD303_WRITE(&FWDGT_RLD, reload);
    remaining = p->poll_limit;
    while (NX_GD303_READ(&FWDGT_STAT) != 0U && remaining-- != 0U)
        NX_GD303_WATCHDOG_POLL();
    *out = observed(p);
    if (NX_GD303_READ(&FWDGT_STAT) != 0U ||
        NX_GD303_READ(&FWDGT_PSC) != prescaler ||
        NX_GD303_READ(&FWDGT_RLD) != reload)
        return NX_ERROR_IO;
    NX_GD303_WRITE(&FWDGT_CTL, FWDGT_KEY_RELOAD);
    /* Successful nominal programming does not establish physical bounds. */
    return NX_SUCCESS;
}
const nx_watchdog_ops_t nx_gd32f303_watchdog_ops = {enable, feed, state};
uint32_t nx_reset_cause(void) {
    uint32_t raw = g_nx_gd32f303_system.reset_cause;
    return ((raw & RCU_RSTSCK_PORRSTF) != 0U ? NX_RESET_POWER_ON : 0U) |
           ((raw & RCU_RSTSCK_EPRSTF) != 0U ? NX_RESET_PIN : 0U) |
           ((raw & RCU_RSTSCK_SWRSTF) != 0U ? NX_RESET_SOFTWARE : 0U) |
           ((raw & RCU_RSTSCK_FWDGTRSTF) != 0U ? NX_RESET_IWDG : 0U) |
           ((raw & RCU_RSTSCK_WWDGTRSTF) != 0U ? NX_RESET_WWDG : 0U) |
           ((raw & RCU_RSTSCK_LPRSTF) != 0U ? NX_RESET_LOW_POWER : 0U);
}
