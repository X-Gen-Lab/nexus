/**
 * \file            control_argument.c
 * \brief           Exercise an unknown request through the actual C11 API.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/mcuboot_control.h"

nx_result_t nx_test_mcuboot_unknown_request(void);

nx_result_t nx_test_mcuboot_unknown_request(void) {
    return nx_mcuboot_request((nx_mcuboot_request_t)99);
}
