/**
 * \file            common_health_freertos_test.cpp
 * \brief           Real static-task resource consumer preserves binding
 * lifetime \author          Nexus Team \version         1.0.0 \date 2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/health_freertos.h"
#include "nexus/os/freertos.h"
#include "nexus/os/storage.h"
#include <array>
#include <cstdlib>
#include <gtest/gtest.h>

namespace {
/** \brief Returning adapter tasks remain suspended until explicit join. */
void resource_task(void* context) {
    __atomic_store_n(static_cast<uint32_t*>(context), 1U, __ATOMIC_RELEASE);
}

class CommonHealthFreeRTOS : public testing::Test {
  protected:
    nx_task_t task{};
    alignas(8) StackType_t stack[8192]{};
    uint32_t finished = 0;
    void SetUp() override {
        ASSERT_EQ(nx_task_start(&task, "resource", stack, sizeof(stack), 3U,
                                resource_task, &finished),
                  NX_SUCCESS);
    }
    void TearDown() override {
        if (task.handle != nullptr) {
            const auto result = nx_task_join_until(
                &task, nx_deadline_after(nx_time_now_us(), 1000000U));
            EXPECT_EQ(result, NX_SUCCESS);
            if (result != NX_SUCCESS)
                std::abort();
        }
    }
};

TEST_F(CommonHealthFreeRTOS,
       UsesActualTaskIdentityWithoutEnumerationOrRepaint) {
    ASSERT_EQ(__atomic_load_n(&finished, __ATOMIC_ACQUIRE), 1U);
    nx_freertos_stack_binding_t binding{task.handle, &task.control, stack,
                                        std::size(stack)};
    nx_health_resources_t output{};
    ASSERT_EQ(nx_health_freertos_resources(&binding, nullptr, &output),
              NX_SUCCESS);
    EXPECT_EQ(output.known, NX_HEALTH_STACK_KNOWN);
    EXPECT_EQ(output.stack.capacity_bytes, sizeof(stack));
    EXPECT_EQ(output.stack.untouched_bytes + output.stack.touched_bytes,
              sizeof(stack));
    const auto original = output;
    const auto first_byte = stack[0];
    binding.stack = stack + 1;
    --binding.words;
    EXPECT_EQ(nx_health_freertos_resources(&binding, nullptr, &output),
              NX_ERROR_STATE);
    EXPECT_EQ(output.stack.capacity_bytes, original.stack.capacity_bytes);
    EXPECT_EQ(stack[0], first_byte);
    binding = {};
}
} /* namespace */
