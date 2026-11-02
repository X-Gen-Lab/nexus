/**
 * \file main.c
 * \brief Ordinary linked-image reference; no bootloader or header generation.
 * \author Nexus Team
 * \version 1.0.0
 * \date 2026-11-02
 *
 * \copyright Copyright (c) 2026 Nexus Project
 */

#include "nexus_bindings.h"
#include <stdint.h>

static volatile uint32_t image_data = UINT32_C(0x12481632);

int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    image_data ^= UINT32_C(0x01010101);
    return nx_platform_stop() == NX_SUCCESS ? 0 : 2;
}
