/**
 * \file            main.c
 * \brief           Generated F103 PMA Device and EP0 withdrawal witness.
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
    nx_usb_device_state_t state;
    const bool observed = in != NULL && out != NULL &&
                          nx_usb_device_state(device, &state) == NX_SUCCESS &&
                          !state.closing;
#if defined(NX_USB_ENDPOINT_REF_DATA_IN)
    const bool bulk =
        nx_factory_usb_activation(NX_USB_DEVICE_ID_FS) != NULL &&
        nx_factory_usb_endpoint_ref(NX_USB_ENDPOINT_REF_DATA_IN) != NULL &&
        nx_factory_usb_endpoint_ref(NX_USB_ENDPOINT_REF_DATA_OUT) != NULL;
    if (!bulk) {
        (void)nx_platform_stop();
        return 3;
    }
#endif
    const nx_result_t stopped = nx_platform_stop();
    return observed && stopped == NX_SUCCESS ? 0 : 2;
}
