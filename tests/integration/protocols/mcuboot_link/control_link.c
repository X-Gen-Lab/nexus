/**
 * \file            control_link.c
 * \brief           Unexecuted control-only link probe without image crypto.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "flash_map_backend/flash_map_backend.h"
#include "mcuboot_config/mcuboot_config.h"
#include "nexus/components/mcuboot_control.h"
#include "nexus_bindings.h"

const nx_mcuboot_flash_operation_t* nx_mcuboot_flash_operation(void) {
    return NULL;
}

void nx_mcuboot_assert_fail(void) {
    for (;;) {
    }
}

int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS ||
        started.remaining_effects != 0U) {
        return 1;
    }
    nx_mcuboot_action_t action = NX_MCUBOOT_ACTION_NONE;
    nx_result_t queried = nx_mcuboot_query(&action);
    nx_result_t requested = nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT);
    nx_result_t stopped = nx_platform_stop();
    /* NULL operation intentionally prevents any persistent side effect. */
    return queried == NX_ERROR_IO && requested == NX_ERROR_IO &&
                   stopped == NX_SUCCESS
               ? 0
               : 2;
}
