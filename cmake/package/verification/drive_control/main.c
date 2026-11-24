/**
 * \file            main.c
 * \brief           Public selected drive profile initialization and storage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/cia402.h"
#include "nexus/components/cia402_position.h"
#include "nexus_bindings.h"

#if __has_include("CO_driver.h") || __has_include("stm32f4xx_hal.h") ||        \
                                                  __has_include("stm32f4xx.h")
#error Private vendor/protocol headers reached the public drive consumer
#endif

static int profiles(void) {
    const nx_cia402_config_t velocity = {0.01F, 200, 400, 400, 800};
    const nx_cia402_position_config_t position = {
        .axis = {0.01F, 200, 400, 400, 800},
        .position_unit_rad = 0.001F,
        .minimum_position = -10000,
        .maximum_position = 10000,
        .profile_velocity = 100,
        .position_window = 5,
        .position_window_ms = 20,
        .velocity_window_rad_s = 0.02F,
        .epoch = 1};
    nx_cia402_t pv = {0};
    nx_cia402_position_t pp = {0};
    if (nx_cia402_initialize(&pv, &velocity) != NX_SUCCESS ||
        nx_cia402_position_initialize(&pp, &position) != NX_SUCCESS) {
        return 1;
    }
    if (pv.core.actual_mode != 0 || pp.core.actual_mode != 0 ||
        pv.core.state != NX_CIA402_SWITCH_ON_DISABLED ||
        pp.core.state != NX_CIA402_SWITCH_ON_DISABLED ||
        pv.core.supported_modes != 4 || pp.core.supported_modes != 1 ||
        nx_canopen_workspace_bytes(NX_CIA402_OBJECT_COUNT) == 0 ||
        nx_canopen_workspace_bytes(NX_CIA402_POSITION_OBJECT_COUNT) == 0 ||
        nx_canopen_workspace_alignment() == 0) {
        return 2;
    }
    if (nx_cia402_release(&pv) != NX_SUCCESS ||
        nx_cia402_position_release(&pp) != NX_SUCCESS) {
        return 3;
    }
    return 0;
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS ||
        started.remaining_effects != 0) {
        return 10;
    }
    const int result = profiles();
    const nx_result_t stopped = nx_platform_stop();
    return result != 0 ? result : stopped == NX_SUCCESS ? 0 : 20;
}
