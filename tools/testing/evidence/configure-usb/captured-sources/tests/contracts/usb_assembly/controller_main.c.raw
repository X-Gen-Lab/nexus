/**
 * \file            controller_main.c
 * \brief           Link-only explicit USB face consumer; no Board qualification
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
static nx_usb_request_t s_nx_request;
static uint8_t s_nx_packet[8];
static nx_usb_event_t s_nx_events[2];
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS) {
        return (int)start.primary;
    }
    const nx_usb_device_port_t* device =
        nx_factory_usb_device(NX_USB_DEVICE_ID_FS);
    const nx_usb_endpoint_port_t* endpoint =
        nx_factory_usb_endpoint(NX_USB_ENDPOINT_ID_BULK_IN);
    size_t count = 0U;
    nx_usb_device_state_t state;
    nx_request_initialize(&s_nx_request.base);
    nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 1000U);
    (void)nx_usb_in_prepare(&s_nx_request, s_nx_packet, sizeof(s_nx_packet),
                            NX_USB_DATA, 0U, deadline);
    (void)nx_usb_endpoint_submit(endpoint, &s_nx_request);
    (void)nx_usb_endpoint_cancel(endpoint, &s_nx_request);
    (void)nx_usb_device_service(device);
    (void)nx_usb_device_read_events(device, s_nx_events, 2U, &count);
    (void)nx_usb_device_state(device, &state);
    return (int)nx_platform_stop();
}
