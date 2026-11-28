/**
 * \file            main.c
 * \brief           Exercise F103RB window watchdog binding without activation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Exercise bound methods through documented non-activating inputs. */
int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    const nx_watchdog_port_t* port =
        nx_factory_watchdog_ref(NX_WATCHDOG_REF_WWDG);
    nx_watchdog_state_t state;
    if (nx_watchdog_port_enable(port, 5000U, true, &state) !=
            NX_ERROR_UNSUPPORTED ||
        nx_watchdog_port_feed(port) != NX_ERROR_STATE ||
        nx_watchdog_port_state(port, &state) != NX_SUCCESS || state.enabled)
        return 2;
    return nx_platform_stop() == NX_SUCCESS ? 0 : 3;
}
