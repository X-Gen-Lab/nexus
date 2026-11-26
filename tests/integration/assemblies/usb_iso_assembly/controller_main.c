/**
 * \file            controller_main.c
 * \brief           Link-only ISO caller with explicit borrowed storage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"
static nx_usb_request_t s_nx_in;
static nx_usb_request_t s_nx_out;
static uint8_t s_nx_payload[8];
static uint8_t s_nx_receive[8];
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS)
        return (int)start.primary;
    const nx_usb_device_port_t* device = nx_factory_usb_device_ref(
        NX_FACTORY_REF_VALUE(nx_usb_device_ref_t, NX_USB_DEVICE_ID_FS));
    const nx_usb_endpoint_port_t* in =
        nx_factory_usb_endpoint_ref(NX_FACTORY_REF_VALUE(
            nx_usb_endpoint_ref_t, NX_USB_ENDPOINT_ID_DATA_IN));
    const nx_usb_endpoint_port_t* out =
        nx_factory_usb_endpoint_ref(NX_FACTORY_REF_VALUE(
            nx_usb_endpoint_ref_t, NX_USB_ENDPOINT_ID_DATA_OUT));
    const nx_usb_device_profile_t* profile =
        nx_factory_usb_profile(NX_USB_DEVICE_ID_FS);
    if (nx_usb_profile_validate(profile) != NX_SUCCESS)
        return (int)NX_ERROR_INVALID;
    nx_request_initialize(&s_nx_in.base);
    nx_request_initialize(&s_nx_out.base);
    nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 1000U);
    (void)nx_usb_in_prepare(&s_nx_in, s_nx_payload, sizeof(s_nx_payload),
                            NX_USB_DATA, 0U, deadline);
    (void)nx_usb_out_prepare(&s_nx_out, s_nx_receive, sizeof(s_nx_receive),
                             NX_USB_DATA, 0U, deadline);
    /* Link coverage only: actual class/activation/ENUMERATED remain external.
     * A successful submit retains these static loans until real final drain. */
    if (nx_usb_endpoint_submit(in, &s_nx_in) == NX_SUCCESS)
        (void)nx_usb_endpoint_cancel(in, &s_nx_in);
    if (nx_usb_endpoint_submit(out, &s_nx_out) == NX_SUCCESS)
        (void)nx_usb_endpoint_cancel(out, &s_nx_out);
    (void)nx_usb_device_service(device);
    return (int)nx_platform_stop();
}
