/**
 * \file            controller_main.c
 * \brief           Link-only explicit USB face consumer; no Board qualification
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/usb_acm.h"
#include "nexus/components/usb_control.h"
#include "nexus/components/usb_descriptors.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
static nx_usb_acm_t s_nx_acm;
static nx_usb_control_t s_nx_control;
static nx_usb_control_request_t s_nx_reply;
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
    const nx_usb_device_profile_t* profile =
        nx_factory_usb_profile(NX_USB_DEVICE_ID_FS);
    const nx_usb_acm_state_t initial = {{115200U, 0U, 0U, 8U}, 0U, 0U};
    if (nx_usb_profile_validate(profile) != NX_SUCCESS ||
        nx_usb_control_initialize(&s_nx_control, profile) != NX_SUCCESS ||
        nx_usb_acm_initialize(&s_nx_acm, profile, 0U, &initial) != NX_SUCCESS) {
        return (int)NX_ERROR_INVALID;
    }
    const nx_usb_endpoint_profile_t* interrupt_in =
        nx_usb_profile_endpoint(profile, 2U, true);
    const nx_usb_endpoint_port_t* endpoint =
        interrupt_in != NULL
            ? interrupt_in->port
            : nx_factory_usb_endpoint(NX_USB_ENDPOINT_ID_BULK_IN);
    /* Structural fixture bytes are external to the common adapter. */
    const uint8_t descriptor[] = {4U, 3U, 9U, 4U};
    const uint8_t configuration[] = {
        9U, 2U,   39U, 0U,  1U, 1U, 0U, 128U, 50U,  9U,  4U,  0U, 0U,
        3U, 255U, 0U,  0U,  0U, 7U, 5U, 129U, 2U,   64U, 0U,  0U, 7U,
        5U, 1U,   2U,  64U, 0U, 0U, 7U, 5U,   130U, 3U,  10U, 0U, 16U};
    const nx_usb_descriptor_view_t view = {
        interrupt_in != NULL ? configuration : descriptor,
        interrupt_in != NULL ? sizeof(configuration) : sizeof(descriptor)};
    if (nx_usb_descriptor_validate(profile, &view) != NX_SUCCESS) {
        return (int)NX_ERROR_INVALID;
    }
    size_t count = 0U;
    nx_usb_device_state_t state = {0};
    nx_request_initialize(&s_nx_request.base);
    nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 1000U);
    (void)nx_usb_in_prepare(&s_nx_request, s_nx_packet, sizeof(s_nx_packet),
                            NX_USB_DATA, 0U, deadline);
    (void)nx_usb_endpoint_submit(endpoint, &s_nx_request);
    (void)nx_usb_endpoint_cancel(endpoint, &s_nx_request);
    (void)nx_usb_device_service(device);
    (void)nx_usb_device_read_events(device, s_nx_events, 2U, &count);
    (void)nx_usb_device_state(device, &state);
    nx_request_initialize(&s_nx_reply.base);
    if (nx_usb_control_status_prepare(&s_nx_reply, state.epoch, deadline) ==
            NX_SUCCESS &&
        nx_usb_control_begin(&s_nx_control, &s_nx_reply) == NX_SUCCESS) {
        (void)nx_usb_control_step(&s_nx_control);
        (void)nx_usb_control_cancel(&s_nx_control, &s_nx_reply, state.epoch);
        (void)nx_usb_control_step(&s_nx_control);
    }
    nx_usb_control_snapshot_t control = {0};
    if (nx_usb_control_snapshot(&s_nx_control, &control) == NX_SUCCESS &&
        !control.outer_active) {
        /* Link coverage only; the class dispatcher and policy stay external. */
        (void)nx_usb_acm_begin(&s_nx_acm, state.epoch, deadline);
        (void)nx_usb_acm_step(&s_nx_acm);
        nx_usb_acm_pending_t pending = {0};
        if (nx_usb_acm_pending(&s_nx_acm, &pending) == NX_SUCCESS) {
            (void)nx_usb_acm_approve(&s_nx_acm, pending.epoch, false);
        }
        (void)nx_usb_acm_cancel(&s_nx_acm, state.epoch);
        (void)nx_usb_acm_step(&s_nx_acm);
    }
    nx_result_t stopped = nx_platform_stop();
    /* Global stop remains the explicit owner decision, never adapter policy. */
    (void)nx_usb_control_step(&s_nx_control);
    (void)nx_usb_acm_step(&s_nx_acm);
    nx_usb_acm_snapshot_t snapshot = {0};
    (void)nx_usb_acm_snapshot(&s_nx_acm, &snapshot);
    return (int)stopped;
}
