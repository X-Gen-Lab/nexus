/**
 * \file            main.c
 * \brief           Dual bxCAN silent-loopback software witness.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static nx_can_tx_request_t tx[2];

static bool launch(const nx_can_port_t* port, unsigned index, uint32_t id) {
    const nx_can_frame_t value = {.id = id,
                                  .format = NX_CAN_STANDARD,
                                  .kind = NX_CAN_DATA,
                                  .dlc = 1U,
                                  .data = {0xA5U}};
    nx_request_initialize(&tx[index].base);
    return nx_can_tx_prepare(&tx[index], &value,
                             nx_deadline_after(nx_time_now_us(), 10000U)) ==
               NX_SUCCESS &&
           nx_can_port_submit(port, &tx[index]) == NX_SUCCESS;
}

static bool round_trip(const nx_can_port_t* port, unsigned index, uint32_t id) {
    bool received = false;
    for (unsigned attempt = 0U; attempt < 8192U; ++attempt) {
        if (nx_can_port_service(port) != NX_SUCCESS)
            return false;
        nx_can_event_t event;
        size_t count = 0U;
        const nx_result_t result =
            nx_can_port_read_events(port, &event, 1U, &count);
        if (result != NX_SUCCESS && result != NX_ERROR_EMPTY)
            return false;
        if (count == 1U) {
            if (event.kind != NX_CAN_EVENT_FRAME || event.frame.id != id ||
                event.filter_tag != 1U)
                return false;
            received = true;
        }
        if (received && nx_request_state(&tx[index].base) == NX_REQUEST_SETTLED)
            break;
    }
    nx_result_t outcome = NX_ERROR_IO;
    size_t transferred = 0U;
    return received &&
           nx_request_result(&tx[index].base, &outcome, &transferred) ==
               NX_SUCCESS &&
           outcome == NX_SUCCESS && transferred == 1U;
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    const nx_can_port_t* port1 = nx_factory_can_ref(NX_CAN_REF_BUS0);
    const nx_can_port_t* port2 = nx_factory_can_ref(NX_CAN_REF_BUS1);
    const bool passed =
        launch(port1, 0U, 0x123U) && launch(port2, 1U, 0x321U) &&
        round_trip(port1, 0U, 0x123U) && round_trip(port2, 1U, 0x321U) &&
        nx_can_port_stop(port1) == NX_SUCCESS && launch(port2, 1U, 0x321U) &&
        round_trip(port2, 1U, 0x321U);
    const nx_result_t stopped = nx_platform_stop();
    return passed && stopped == NX_SUCCESS ? 0 : 2;
}
