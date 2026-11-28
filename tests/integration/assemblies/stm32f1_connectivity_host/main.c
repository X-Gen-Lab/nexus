/**
 * \file            main.c
 * \brief           Generated F105/F107 Host and logical pipe witness.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_platform_start_result_t began = nx_platform_start();
    if (began.primary != NX_SUCCESS || began.cleanup != NX_SUCCESS)
        return 1;
    const nx_usb_host_port_t* host =
        nx_factory_usb_host_ref(NX_USB_HOST_REF_LINK);
    nx_usb_host_state_t state;
    nx_result_t result = nx_usb_host_state(host, &state);
    if (result == NX_SUCCESS && !state.started)
        result = NX_ERROR_STATE;
    return nx_platform_stop() == NX_SUCCESS && result == NX_SUCCESS ? 0 : 2;
}
