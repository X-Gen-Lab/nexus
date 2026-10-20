/**
 * \file            g474-can-fd.c
 * \brief           Public installed-SDK CAN FD ownership link verification
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"

static nx_can_fd_tx_request_t s_request;
static const nx_can_fd_frame_t s_frame = {
    .id = 0U,
    .format = NX_CAN_STANDARD,
    .kind = NX_CAN_DATA,
    .wire = NX_CAN_WIRE_FD,
    .payload_bytes = 12U,
    .brs = true,
    .data = {0x12U, 0x34U},
};
static nx_can_fd_state_t s_state;

/** \brief Preserve accepted storage if transmission or IRQ drain remains. */
static void keep_storage(void) {
    for (;;) {
    }
}

/** \brief Provider stop alone does not release the outer platform leases. */
static int close_platform(int result) {
    if (nx_platform_stop() != NX_SUCCESS) {
        keep_storage();
    }
    return result;
}

/** \brief Link separate normal/loopback selections without running hardware. */
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS) {
        return close_platform(1);
    }
    const nx_can_fd_port_t* can = nx_factory_can_fd(NX_CAN_FD_ID_FIELDBUS);
    nx_request_initialize(&s_request.base);
    if (can == NULL ||
        nx_can_fd_tx_prepare(&s_request, &s_frame,
                             nx_deadline_after(nx_time_now_us(), 10000U)) !=
            NX_SUCCESS ||
        nx_can_fd_port_submit(can, &s_request) != NX_SUCCESS) {
        return close_platform(2);
    }
    if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
        (void)nx_can_fd_port_cancel(can, &s_request);
    }
    for (unsigned i = 0U;
         i != 64U && nx_request_state(&s_request.base) != NX_REQUEST_SETTLED;
         ++i) {
        (void)nx_can_fd_port_service(can);
    }
    (void)nx_can_fd_port_state(can, &s_state);
    (void)nx_can_fd_port_stop(can);
    if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
        (void)nx_platform_stop();
        if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
            keep_storage();
        }
    }
    return close_platform(0);
}
