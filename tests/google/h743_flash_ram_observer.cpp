/**
 * \file            h743_flash_ram_observer.cpp
 * \brief           Reject Flash-resident fault observations in RAM tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_flash_ram_closure_support.h"
#include "nexus/core/fault.h"
#include <gtest/gtest.h>
using namespace nexus_test::h743_ram;

extern "C" void nx_fault_observe(nx_fault_domain_t, nx_result_t, const char*,
                                 unsigned) {
    ++cpu.flash_fault_observers;
    ADD_FAILURE() << "A busy-bank terminal path called the Flash observer";
}
