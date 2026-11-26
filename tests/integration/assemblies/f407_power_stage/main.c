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
#if defined(NX_POWER_STAGE_REF_DRIVE)
    const nx_power_stage_t* stages[] = {
        nx_factory_power_stage_ref(NX_POWER_STAGE_REF_DRIVE)};
#else
    const nx_power_stage_t* stages[] = {
        nx_factory_power_stage_ref(NX_POWER_STAGE_REF_LEFT),
        nx_factory_power_stage_ref(NX_POWER_STAGE_REF_RIGHT)};
#endif
    for (size_t i = 0; i < sizeof(stages) / sizeof(stages[0]); ++i) {
        nx_power_stage_state_t state;
        if (nx_power_stage_state(stages[i], &state) != NX_SUCCESS) {
            return 2;
        }
    }
    return nx_platform_stop() == NX_SUCCESS ? 0 : 3;
}
