/**
 * \file            freertos_permanent_return_test.cpp
 * \brief           Real permanent task return enters the pinned port fault
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-04
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "nexus/os/freertos.h"
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <unistd.h>

namespace {
bool entry_executed;

/** \brief Deliberately violate the permanent task's non-return contract. */
void invalid_permanent_entry(void*) {
    entry_executed = true;
}

/** \brief Kernel entry stays in the child; the parent has no RTOS threads. */
void run_returning_permanent_task() {
    (void)alarm(3U);
    nx_freertos_permanent_task_t task{};
    alignas(8) StackType_t stack[8192]{};
    if (nx_freertos_permanent_task_start(&task, "returning", stack, 8192U, 3U,
                                         invalid_permanent_entry,
                                         nullptr) != NX_SUCCESS) {
        std::_Exit(93);
    }
    vTaskStartScheduler();
    std::_Exit(94);
}

TEST(FreeRTOSPermanentReturn, RealPortAssertsInsteadOfCompletingSilently) {
    EXPECT_EXIT(run_returning_permanent_task(), ::testing::ExitedWithCode(91),
                "");
}
} /* namespace */

/** \brief Observe the real port assertion; weak hook parking is tested apart.
 */
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    const bool actual_return_fault =
        entry_executed && domain == NX_FAULT_FREERTOS_ASSERT &&
        reason == NX_ERROR_STATE && file != nullptr && line != 0U &&
        std::strstr(file, "/Posix/port.c") != nullptr;
    std::_Exit(actual_return_fault ? 91 : 92);
}
