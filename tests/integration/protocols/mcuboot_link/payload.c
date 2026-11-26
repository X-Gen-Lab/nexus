/**
 * \file            payload.c
 * \brief           Normal C11 payload linked in the primary execution window.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS ||
        started.remaining_effects != 0U) {
        return 1;
    }
    return nx_platform_stop() == NX_SUCCESS ? 0 : 2;
}
