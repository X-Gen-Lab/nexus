/**
 * \file            main.c
 * \brief           F103RB external CAN transmit and stop software witness.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static nx_can_tx_request_t request;

static bool send(const nx_can_port_t* bus) {
    const nx_can_frame_t frame = {.id = 0x123U,
                                  .format = NX_CAN_STANDARD,
                                  .kind = NX_CAN_DATA,
                                  .dlc = 1U,
                                  .data = {0xA5U}};
    const nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 20000U);
    nx_request_initialize(&request.base);
    if (nx_can_tx_prepare(&request, &frame, deadline) != NX_SUCCESS ||
        nx_can_port_submit(bus, &request) != NX_SUCCESS)
        return false;
    for (unsigned poll = 0U; poll < 8192U; ++poll) {
        if (nx_can_port_service(bus) != NX_SUCCESS)
            return false;
        if (nx_request_state(&request.base) == NX_REQUEST_SETTLED)
            break;
    }
    size_t transferred = 0U;
    nx_result_t transmission = NX_ERROR_IO;
    return nx_request_result(&request.base, &transmission, &transferred) ==
               NX_SUCCESS &&
           transmission == NX_SUCCESS && transferred == 1U;
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    const nx_can_port_t* bus = nx_factory_can_ref(NX_CAN_REF_BUS);
    const bool sent = send(bus);
    nx_can_state_t state;
    const nx_result_t observed = nx_can_port_state(bus, &state);
    const nx_result_t stopped = nx_platform_stop();
    return sent && observed == NX_SUCCESS && !state.bus_off &&
                   stopped == NX_SUCCESS
               ? 0
               : 2;
}
