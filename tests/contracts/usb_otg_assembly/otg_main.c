/**
 * \file            otg_main.c
 * \brief           C11 consumer of one managed role and current typed faces
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/time.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"

/** \brief Link explicit role controls without enabling an unknown PCB supply.
 * A nonzero value reports failed assembly or retained cleanup.
 */
int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS)
        return 1;
    const nx_usb_otg_port_t* port = nx_factory_usb_otg((nx_usb_device_id_t)0);
    const nx_usb_device_profile_t* profile =
        nx_factory_usb_profile((nx_usb_device_id_t)0);
    const nx_usb_device_profile_t* fallback = nx_factory_usb_profile_for_speed(
        (nx_usb_device_id_t)0, NX_USB_PROFILE_FULL_SPEED);
    if (nx_factory_usb_device((nx_usb_device_id_t)0) == NULL ||
        nx_factory_usb_endpoint((nx_usb_endpoint_id_t)0) == NULL ||
        nx_factory_usb_activation((nx_usb_device_id_t)0) == NULL ||
        nx_usb_profile_validate(profile) != NX_SUCCESS ||
        nx_usb_profile_validate(fallback) != NX_SUCCESS)
        return 7;
    nx_usb_otg_state_t state;
    if (nx_usb_otg_state(port, &state) != NX_SUCCESS)
        return 2;
    nx_time_us_t deadline;
    if (nx_deadline_finite_after(nx_time_now_us(), 1000000U, &deadline) !=
        NX_SUCCESS)
        return 3;
    if (nx_usb_otg_request(port, state.generation, NX_USB_OTG_OFF, deadline) !=
        NX_SUCCESS)
        return 4;
    if (nx_usb_otg_service(port) != NX_SUCCESS ||
        nx_usb_otg_device(port, state.generation) != NULL ||
        nx_usb_otg_host(port, state.generation) != NULL)
        return 5;
    return nx_platform_stop() == NX_SUCCESS ? 0 : 6;
}
