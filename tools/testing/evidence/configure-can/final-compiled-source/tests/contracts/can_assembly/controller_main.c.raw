/**
 * \file            controller_main.c
 * \brief           External software-only CAN assembly linking probe
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_config.h"
#include "nexus_factory.h"

static volatile nx_result_t s_probe;

/** \brief Retain the actual selected CAN interface without a product worker. */
static void exercise(const nx_can_port_t* port, nx_can_tx_request_t* request) {
    nx_request_initialize(&request->base);
    const nx_can_frame_t frame = {
        0x123U, NX_CAN_STANDARD, NX_CAN_DATA, 1U, {1U}};
    nx_can_event_t events[2];
    nx_can_state_t state;
    size_t count;
    s_probe = nx_can_tx_prepare(request, &frame,
                                nx_deadline_after(nx_time_now_us(), 10000U));
    s_probe = nx_can_port_submit(port, request);
    s_probe = nx_can_port_service(port);
    s_probe = nx_can_port_cancel(port, request);
    s_probe = nx_can_port_read_events(port, events, 2U, &count);
    s_probe = nx_can_port_state(port, &state);
    s_probe = nx_can_port_attach_wake(port, NULL, 5U);
    s_probe = nx_can_port_recover(port);
    s_probe = nx_can_port_stop(port);
}

/** \brief This image is compiled and inspected; no physical execution claimed.
 */
int main(void) {
    /* Stable exact request storage remains valid even if drain quarantines it.
     */
    static nx_can_tx_request_t first_request;
#ifdef NEXUS_BUS1_SELECTED
    static nx_can_tx_request_t second_request;
#endif
    nx_platform_start_result_t started = nx_platform_start();
    s_probe = started.primary;
    if (started.primary == NX_SUCCESS) {
        exercise(nx_factory_can(NX_CAN_ID_BUS0), &first_request);
#ifdef NEXUS_BUS1_SELECTED
        exercise(nx_factory_can(NX_CAN_ID_BUS1), &second_request);
#endif
        s_probe = nx_platform_stop();
    }
    return 0;
}
