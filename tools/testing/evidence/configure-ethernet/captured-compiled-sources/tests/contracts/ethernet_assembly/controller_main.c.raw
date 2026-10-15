/**
 * \file            controller_main.c
 * \brief           Link-only selected MAC/MDIO storage and public operations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"

static nx_eth_tx_request_t s_request;
_Alignas(4) static uint8_t s_frame[64];
static nx_eth_state_t s_snapshot;
static nx_eth_rx_frame_t s_received;
static uint16_t s_phy_value;

int main(void) {
    const nx_eth_port_t* mac = nx_factory_eth(NX_ETH_ID_MAC);
    const nx_mdio_port_t* mdio = nx_factory_mdio(NX_MDIO_ID_MAC);
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary == NX_SUCCESS) {
        const nx_eth_link_t down = {false, false, 100U};
        (void)nx_eth_port_apply_link(mac, &down);
        nx_request_initialize(&s_request.base);
        s_frame[12] = 8U;
        (void)nx_eth_tx_prepare(&s_request, s_frame, sizeof(s_frame),
                                nx_deadline_after(nx_time_now_us(), 1000U));
        (void)nx_eth_port_submit(mac, &s_request);
        (void)nx_eth_port_cancel(mac, &s_request);
        (void)nx_eth_port_service(mac);
        (void)nx_eth_port_state(mac, &s_snapshot);
        if (nx_eth_port_acquire(mac, &s_received) == NX_SUCCESS) {
            (void)nx_eth_port_release(mac, &s_received);
        }
        (void)nx_mdio_read(mdio, 1U, 0U, &s_phy_value,
                           nx_deadline_after(nx_time_now_us(), 1000U));
    }
    (void)nx_platform_stop();
    /* This artifact never claims an observed PHY/link. All borrowed storage
     * remains static even if hardware refuses shutdown and stays quarantined.
     */
    for (;;) {
    }
}
