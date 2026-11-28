/**
 * \file            main.c
 * \brief           Generated EP0 factory and owned Device withdrawal witness.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_platform_start_result_t platform = nx_platform_start();
    if (platform.primary != NX_SUCCESS || platform.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_usb_device_port_t* device =
        nx_factory_usb_device_ref(NX_USB_DEVICE_REF_FS);
    const nx_usb_endpoint_port_t* in =
        nx_factory_usb_endpoint_ref(NX_USB_ENDPOINT_REF_CONTROL_IN);
    const nx_usb_endpoint_port_t* out =
        nx_factory_usb_endpoint_ref(NX_USB_ENDPOINT_REF_CONTROL_OUT);
    const nx_usb_device_profile_t* profile =
        nx_factory_usb_profile(NX_USB_DEVICE_ID_FS);
    const nx_usb_activation_port_t* activation =
        nx_factory_usb_activation(NX_USB_DEVICE_ID_FS);
    nx_usb_device_state_t state;
    const bool observed = in != NULL && out != NULL && activation != NULL &&
                          nx_usb_profile_validate(profile) == NX_SUCCESS &&
                          nx_usb_device_state(device, &state) == NX_SUCCESS &&
                          state.started;
    const nx_result_t stopped = nx_platform_stop();
    return observed && stopped == NX_SUCCESS ? 0 : 2;
}
