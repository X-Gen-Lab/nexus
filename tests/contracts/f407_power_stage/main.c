/**
 * \file            main.c
 * \brief           Power-stage strong factory and cold lifecycle link consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    if (nx_platform_start().primary != NX_SUCCESS) {
        return 1;
    }
    const nx_power_stage_t* drive =
        nx_factory_power_stage_ref(NX_POWER_STAGE_REF_DRIVE);
    nx_power_stage_state_t state;
    if (nx_power_stage_state(drive, &state) != NX_SUCCESS) {
        return 2;
    }
    return nx_platform_stop() == NX_SUCCESS ? 0 : 3;
}
