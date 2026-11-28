/**
 * \file            main.c
 * \brief           Check independent watchdog selection without starting it
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Observe the selected face with no watchdog key writes. */
int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_watchdog_port_t* port =
        nx_factory_watchdog_ref(NX_WATCHDOG_REF_IWDG);
    nx_watchdog_state_t state;
    const nx_result_t result = nx_watchdog_port_state(port, &state);
    if (result != NX_SUCCESS) {
        return 2;
    }
    return !state.enabled && nx_platform_stop() == NX_SUCCESS ? 0 : 3;
}
