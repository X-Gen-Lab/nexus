/**
 * \file            ethernet-frames.c
 * \brief           Installed public SDK enhanced Ethernet ownership linking
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"

static nx_eth_frames_tx_request_t s_request;
_Alignas(4) static uint8_t s_head[13] = {2U, 0U, 0U, 0U, 0U, 2U,   2U,
                                         0U, 0U, 0U, 0U, 1U, 0x81U};
_Alignas(4) static uint8_t s_tail[51] = {0U, 0xb0U, 0x25U, 0x88U, 0xb5U};
static const nx_eth_fragment_t s_fragments[] = {{s_head, sizeof(s_head)},
                                                {s_tail, sizeof(s_tail)}};
static nx_eth_frames_rx_loan_t s_loan;
static nx_eth_fragment_t s_views[8];
static nx_eth_frames_state_t s_state;
static nx_io_timestamp_t s_clock;

/** \brief Retain static accepted storage after incomplete drain or release. */
static void keep_storage(void) {
    for (;;) {
    }
}
/** \brief Release outer pins/gates/timebase only after real provider stop. */
static int close_platform(int result) {
    if (nx_platform_stop() != NX_SUCCESS) {
        keep_storage();
    }
    return result;
}
/** \brief Link the same public source for F407ZG and GD470 exact assemblies. */
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS) {
        return close_platform(1);
    }
    const nx_eth_frames_port_t* port =
        nx_factory_eth_frames(NX_ETH_FRAMES_ID_MAC);
    const nx_mdio_port_t* mdio = nx_factory_mdio(NX_MDIO_ID_MAC);
    nx_request_initialize(&s_request.base);
    if (port == NULL || mdio == NULL ||
        nx_eth_frames_tx_prepare(&s_request, s_fragments, 2U, true,
                                 nx_deadline_after(nx_time_now_us(), 10000U)) !=
            NX_SUCCESS) {
        return close_platform(2);
    }
    /* Compilation/linking does not invent a PHY link observation. A board
     * product supplies that policy; rejection before link-up keeps no loan. */
    nx_result_t admission = nx_eth_frames_port_submit(port, &s_request);
    if (admission == NX_SUCCESS) {
        (void)nx_eth_frames_port_cancel(port, &s_request);
        for (unsigned i = 0U;
             i < 64U && nx_request_state(&s_request.base) != NX_REQUEST_SETTLED;
             ++i) {
            (void)nx_eth_frames_port_service(port);
        }
    }
    nx_result_t received =
        nx_eth_frames_port_acquire(port, &s_loan, s_views, 8U);
    if (received == NX_SUCCESS &&
        nx_eth_frames_port_release(port, &s_loan) != NX_SUCCESS) {
        keep_storage();
    }
    (void)nx_eth_frames_port_state(port, &s_state);
    (void)nx_eth_frames_port_clock(port, &s_clock);
    (void)nx_eth_frames_port_stop(port);
    if (admission == NX_SUCCESS &&
        nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
        (void)nx_platform_stop();
        if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
            keep_storage();
        }
    }
    return close_platform(0);
}
