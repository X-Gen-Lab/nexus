/**
 * \file            composition.c
 * \brief           Explicit platform lifetime for public software admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "application.h"
#include "nexus_bindings.h"

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS ||
        started.remaining_effects != 0U) {
        return 10;
    }
    const int application = smart_component_application();
    const nx_result_t cleanup = nx_platform_stop();
    return application != 0 ? application : cleanup == NX_SUCCESS ? 0 : 20;
}
