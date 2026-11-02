/**
 * \file            empty.c
 * \brief           Reference baseline with linked but unreferenced TMP117
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
int main(void) {
    nx_platform_start_result_t boot = nx_platform_start();
    if (boot.primary != NX_SUCCESS || boot.cleanup != NX_SUCCESS ||
        boot.remaining_effects != 0U) {
        return 1;
    }
    return nx_platform_stop() == NX_SUCCESS ? 0 : 2;
}
