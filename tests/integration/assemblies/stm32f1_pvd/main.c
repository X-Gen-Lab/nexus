/**
 * \file            main.c
 * \brief           Consume the F103 polling comparator through the EXTI face.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    const nx_exti_port_t* supply = nx_factory_exti_ref(NX_EXTI_REF_SUPPLY);
    bool below = false;
    const nx_result_t sampled = nx_exti_port_level(supply, &below);
    const nx_result_t stopped = nx_platform_stop();
    return sampled == NX_SUCCESS && stopped == NX_SUCCESS ? 0 : 2;
}
