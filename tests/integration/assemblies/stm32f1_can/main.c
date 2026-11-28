/**
 * \file            main.c
 * \brief           Exercise the exact F103RB silent loopback factory face.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static nx_can_tx_request_t request;

static bool round_trip(const nx_can_port_t* port, nx_can_format_t format,
                       uint32_t id) {
    const nx_can_frame_t frame = {.id = id,
                                  .format = format,
                                  .kind = NX_CAN_DATA,
                                  .dlc = 1U,
                                  .data = {0xA5U}};
    const nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 10000U);
    if (nx_can_tx_prepare(&request, &frame, deadline) != NX_SUCCESS ||
        nx_can_port_submit(port, &request) != NX_SUCCESS)
        return false;
    for (unsigned i = 0U; i < 4096U; ++i) {
        if (nx_can_port_service(port) != NX_SUCCESS)
            return false;
        if (nx_request_state(&request.base) == NX_REQUEST_SETTLED)
            break;
    }
    size_t transferred = 0U;
    nx_result_t result = NX_ERROR_IO;
    if (nx_request_result(&request.base, &result, &transferred) != NX_SUCCESS ||
        result != NX_SUCCESS || transferred != 1U)
        return false;
    for (unsigned i = 0U; i < 4096U; ++i) {
        nx_can_event_t event;
        size_t count = 0U;
        result = nx_can_port_read_events(port, &event, 1U, &count);
        if (result != NX_ERROR_EMPTY)
            return result == NX_SUCCESS && count == 1U &&
                   event.kind == NX_CAN_EVENT_FRAME && event.frame.id == id &&
                   event.frame.format == format && event.frame.data[0] == 0xA5U;
    }
    return false;
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    const nx_can_port_t* loop = nx_factory_can_ref(NX_CAN_REF_LOOP);
    nx_request_initialize(&request.base);
    const bool received = round_trip(loop, NX_CAN_STANDARD, 0x7FFU) &&
                          round_trip(loop, NX_CAN_EXTENDED, 0x1FFFFFFFU);
    const nx_result_t stopped = nx_platform_stop();
    return received && stopped == NX_SUCCESS ? 0 : 2;
}
