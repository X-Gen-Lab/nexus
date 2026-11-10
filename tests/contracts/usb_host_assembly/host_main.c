/**
 * \file            host_main.c
 * \brief           Link-only static Host consumer, no physical USB claim
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/usb_role.h"
#include "nexus/io/usb_host.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"

static nx_usb_host_request_t s_request;
_Alignas(4) static uint8_t s_bytes[64];

int main(void) {
    /* External product must establish PHY/VBUS ownership before this start.
     * This consumer is linked, never executed as physical station evidence. */
    nx_platform_start_result_t result = nx_platform_start();
    if (result.primary != NX_SUCCESS) {
        return (int)result.primary;
    }
    const nx_usb_host_port_t* port = nx_factory_usb_host(NX_USB_HOST_ID_LINK);
    nx_usb_host_state_t snapshot = {0};
    nx_usb_host_pipe_t pipe = {64U, 0U, 0U, NX_USB_HOST_FULL_SPEED,
                               NX_USB_HOST_CONTROL};
    nx_request_initialize(&s_request.base);
    nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 500000U);
    (void)nx_usb_host_state(port, &snapshot);
    (void)nx_usb_host_reset(port, deadline);
    (void)nx_usb_host_suspend(port, true, deadline);
    (void)nx_usb_host_suspend(port, false, deadline);
    (void)nx_usb_host_open(port, 0U, &pipe);
    (void)nx_usb_host_prepare(&s_request, s_bytes, sizeof(s_bytes), 8U, true,
                              false, deadline);
    (void)nx_usb_host_submit(port, 0U, &s_request);
    (void)nx_usb_host_cancel(port, 0U, &s_request);
    (void)nx_usb_host_service(port);
    (void)nx_usb_host_close(port, 0U);
    (void)nx_usb_host_stop(port);
    return (int)nx_platform_stop();
}
