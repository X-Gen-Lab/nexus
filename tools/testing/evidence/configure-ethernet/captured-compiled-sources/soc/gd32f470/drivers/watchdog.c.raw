/**
 * \file            watchdog.c
 * \brief           GD32 independent watchdog remaining effects and reset flags
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "private/system.h"

static nx_gd32_watchdog_state_t* s_watchdog;
extern uint32_t g_gd32_reset_flags;

/** \brief           Preserve honest uncharacterized IRC32K physical bounds. */
nx_result_t nx_gd32_watchdog_initialize(nx_gd32_watchdog_state_t* port) {
    if (!port) {
        return NX_ERROR_INVALID;
    }
    if (s_watchdog) {
        return NX_ERROR_BUSY;
    }
    bool hardware_enabled = (FMC_OBCTL0 & FMC_OBCTL0_NWDG_HW) == 0u;
    *port = (nx_gd32_watchdog_state_t){
        .state = {.minimum_timeout_us = 0u,
                  .maximum_timeout_us = UINT32_MAX,
                  .enabled = hardware_enabled,
                  .irreversible = hardware_enabled,
                  .debug_freeze = (DBG_CTL1 & DBG_CTL1_FWDGT_HOLD) != 0u},
        .initialized = true};
    s_watchdog = port;
    return NX_SUCCESS;
}

/** \brief           Configure before enabling; retain every irreversible
 * effect. */
nx_result_t nx_gd32_watchdog_enable(void* context, uint32_t timeout_us,
                                    bool debug_freeze,
                                    nx_watchdog_state_t* state) {
    nx_gd32_watchdog_state_t* port = context;
    if (!port || port != s_watchdog || !state || !port->initialized ||
        timeout_us < 1000u || timeout_us > 32760000u) {
        return NX_ERROR_INVALID;
    }
    *state = port->state;
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->state.enabled) {
        return NX_ERROR_STATE;
    }
    unsigned prescaler = 0u;
    uint64_t ticks = ((uint64_t)timeout_us * 32000u + 999999u) / 1000000u;
    while (prescaler < 6u &&
           (ticks + (4u << prescaler) - 1u) / (4u << prescaler) > 4096u) {
        ++prescaler;
    }
    uint32_t reload =
        (uint32_t)((ticks + (4u << prescaler) - 1u) / (4u << prescaler));
    if (!reload || reload > 4096u) {
        return NX_ERROR_INVALID;
    }
    RCU_RSTSCK |= RCU_RSTSCK_IRC32KEN;
    for (uint32_t polls = 0u; polls < 1000000u; ++polls) {
        if ((RCU_RSTSCK & RCU_RSTSCK_IRC32KSTB) != 0u) {
            break;
        }
        if (polls + 1u == 1000000u) {
            return NX_ERROR_IO;
        }
    }
    if (fwdgt_config((uint16_t)(reload - 1u), (uint8_t)prescaler) != SUCCESS) {
        return NX_ERROR_IO;
    }
    if (debug_freeze) {
        DBG_CTL1 |= DBG_CTL1_FWDGT_HOLD;
    } else {
        DBG_CTL1 &= ~DBG_CTL1_FWDGT_HOLD;
    }
    FWDGT_CTL = FWDGT_KEY_RELOAD;
    FWDGT_CTL = FWDGT_KEY_ENABLE;
    port->state.enabled = true;
    port->state.irreversible = true;
    nx_gd32_peripheral_barrier();
    port->state.debug_freeze = (DBG_CTL1 & DBG_CTL1_FWDGT_HOLD) != 0u;
    *state = port->state;
    /* No enable readback exists. The enable write itself owns the effect. */
    if (port->state.debug_freeze != debug_freeze) {
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

/** \brief           Feed only the known unique enabled watchdog owner. */
nx_result_t nx_gd32_watchdog_feed(void* context) {
    nx_gd32_watchdog_state_t* port = context;
    if (!port || port != s_watchdog || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (!port->state.enabled) {
        return NX_ERROR_STATE;
    }
    FWDGT_CTL = FWDGT_KEY_RELOAD;
    return NX_SUCCESS;
}

/** \brief           Report enabled effects without claiming oscillator
 * accuracy. */
nx_result_t nx_gd32_watchdog_state(const void* context,
                                   nx_watchdog_state_t* state) {
    const nx_gd32_watchdog_state_t* port = context;
    if (!port || port != s_watchdog || !state) {
        return NX_ERROR_INVALID;
    }
    *state = port->state;
    return NX_SUCCESS;
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

/** \brief           Refuse to erase irreversible watchdog responsibility. */
nx_result_t nx_gd32_watchdog_stop(nx_gd32_watchdog_state_t* port) {
    if (!port || port != s_watchdog || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->state.enabled) {
        return NX_ERROR_UNSUPPORTED;
    }
    port->initialized = false;
    s_watchdog = NULL;
    return NX_SUCCESS;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_watchdog_ops_t nx_gd32_watchdog_ops = {
    .enable = nx_gd32_watchdog_enable,
    .feed = nx_gd32_watchdog_feed,
    .state = nx_gd32_watchdog_state,
};
