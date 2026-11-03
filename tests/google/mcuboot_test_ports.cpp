/**
 * \file            mcuboot_test_ports.cpp
 * \brief           Product-owned hooks for the normal in-memory Flash model.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "mcuboot_memory_model.h"
#include <cstdlib>
extern "C" const nx_mcuboot_flash_operation_t*
nx_mcuboot_flash_operation(void) {
    return nexus_mcuboot_test::active;
}
extern "C" void nx_mcuboot_assert_fail(void) {
    ADD_FAILURE() << "Unexpected upstream assertion in ordinary boot control";
    std::abort();
}
extern "C" void nx_mcuboot_watchdog_progress(void) {
}
