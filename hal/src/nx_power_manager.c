/**
 * \file            nx_power_manager.c
 * \brief           Common system power manager capability boundary
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "hal/system/nx_power_manager.h"
#include "osal/osal.h"

static nx_status_t power_manager_enter_mode(nx_power_manager_t* self,
                                            nx_power_mode_t mode);
static nx_power_mode_t power_manager_get_mode(nx_power_manager_t* self);

/* Static initialization avoids concurrent first-use writes to the interface. */
static nx_power_manager_t g_power_manager = {
    .enter_mode = power_manager_enter_mode,
    .get_mode = power_manager_get_mode,
};

nx_power_manager_t* nx_get_power_manager(void) {
    return &g_power_manager;
}

static nx_status_t power_manager_enter_mode(nx_power_manager_t* self,
                                            nx_power_mode_t mode) {
    if (!self) return NX_ERR_NULL_PTR;
    if (self != &g_power_manager) return NX_ERR_INVALID_PARAM;
    if (mode != NX_POWER_RUN && mode != NX_POWER_SLEEP && mode != NX_POWER_STOP)
        return NX_ERR_INVALID_PARAM;
    if (osal_is_isr()) return NX_ERR_INVALID_STATE;
    /* This layer has no board-specific clock/wakeup transition. A status-only
     * mode change must never be mistaken for entering a hardware power mode. */
    return mode == NX_POWER_RUN ? NX_OK : NX_ERR_NOT_SUPPORTED;
}

static nx_power_mode_t power_manager_get_mode(nx_power_manager_t* self) {
    if (!self || self != &g_power_manager) return NX_POWER_UNKNOWN;
    return NX_POWER_RUN;
}
