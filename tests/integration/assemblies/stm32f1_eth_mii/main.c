/**
 * \file            main.c
 * \brief           F107 exact raw Ethernet software consumer.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;

    const nx_eth_port_t* const lan = nx_factory_eth_ref(NX_ETH_REF_LAN);
    const nx_mdio_port_t* const phy = nx_factory_mdio_ref(NX_MDIO_REF_LAN);
    nx_eth_state_t state;
    uint16_t phy_id = 0U;
    const bool valid =
        nx_eth_port_state(lan, &state) == NX_SUCCESS && state.started &&
        nx_mdio_read(phy, 1U, 2U, &phy_id,
                     nx_deadline_after(nx_time_now_us(), 1000U)) ==
            NX_SUCCESS &&
        phy_id != 0U && phy_id != UINT16_MAX;
    nx_result_t stopped = NX_ERROR_BUSY;
    const nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 5000U);
    do {
        stopped = nx_platform_stop();
    } while (stopped == NX_ERROR_BUSY &&
             !nx_deadline_expired(deadline, nx_time_now_us()));
    return valid && stopped == NX_SUCCESS ? 0 : 2;
}
