/**
 * \file            st_main.c
 * \brief           Exact F103RB consumer of the selected ST Device Core.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "st_bulk.h"

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS) {
        return (int)started.primary;
    }
    nx_result_t result =
        nx_test_pma_st_start(nx_factory_usb_profile(NX_USB_DEVICE_ID_FS),
                             nx_factory_usb_activation(NX_USB_DEVICE_ID_FS));
    while (result == NX_SUCCESS) {
        result = nx_test_pma_st_step();
    }
    const nx_result_t stopped = nx_test_pma_st_stop();
    if (stopped != NX_SUCCESS) {
        return (int)stopped;
    }
    const nx_result_t closed = nx_platform_stop();
    return (int)(closed == NX_SUCCESS ? result : closed);
}
