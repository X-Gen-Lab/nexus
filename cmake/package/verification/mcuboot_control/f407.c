/**
 * \file            f407.c
 * \brief           Unexecuted control link without persistent operation
 * admission \author          Nexus Team \version         1.0.0 \date 2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "application.h"
#include "flash_map_backend/flash_map_backend.h"
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
    nx_result_t result = control_api_run(false);
    nx_result_t stopped = nx_platform_stop();
    /* NULL operation prevents persistent effects even if this probe is run. */
    return result == NX_ERROR_IO && stopped == NX_SUCCESS ? 0 : 2;
}
