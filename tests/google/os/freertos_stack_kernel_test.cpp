/**
 * \file            freertos_stack_kernel_test.cpp
 * \brief           Real static kernel task identity and bounded stack snapshots
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "nexus/os/freertos.h"
#include "nexus/os/freertos_stack.h"
#include <cstdlib>
#include <gtest/gtest.h>

namespace {
constexpr size_t words = 8192U;

/** \brief Returning entry leaves the real adapter task suspended until join. */
void finished_entry(void* context) {
    __atomic_store_n(static_cast<uint32_t*>(context), 1U, __ATOMIC_RELEASE);
}

/** \brief Test failures never let a real task retain expired fixture storage.
 */
class FreeRTOSStackKernel : public testing::Test {
  protected:
    nx_freertos_task_t task{};
    alignas(8) StackType_t stack[words]{};
    uint32_t finished{};

    void SetUp() override {
        ASSERT_EQ(nx_freertos_task_start(&task, "snapshot", stack, words, 3U,
                                         finished_entry, &finished),
                  NX_SUCCESS);
    }
    void TearDown() override {
        if (task.handle != nullptr) {
            const nx_result_t result = nx_freertos_task_join(
                &task, nx_deadline_after(nx_time_now_us(), 1000000U));
            EXPECT_EQ(result, NX_SUCCESS);
            if (result != NX_SUCCESS) {
                std::abort();
            }
        }
    }
};

TEST_F(FreeRTOSStackKernel, QueriesActualUnjoinedStaticTaskWithoutRepainting) {
    ASSERT_EQ(__atomic_load_n(&finished, __ATOMIC_ACQUIRE), 1U);
    nx_freertos_stack_binding_t binding{task.handle, &task.control, stack,
                                        words};
    nx_stack_observation_t first{};
    ASSERT_EQ(nx_freertos_stack_observe(&binding, &first), NX_SUCCESS);
    EXPECT_EQ(first.capacity_bytes, sizeof stack);
    EXPECT_EQ(first.untouched_bytes + first.touched_bytes, sizeof stack);
    EXPECT_GT(first.touched_bytes, 0U);
    nx_stack_observation_t second{};
    ASSERT_EQ(nx_freertos_stack_observe(&binding, &second), NX_SUCCESS);
    EXPECT_EQ(first.untouched_bytes, second.untouched_bytes);
    EXPECT_EQ(first.touched_bytes, second.touched_bytes);
    /* Finish snapshot readers before deletion; stale handles are not queried.
     */
    binding = {};
    ASSERT_EQ(nx_freertos_task_join(
                  &task, nx_deadline_after(nx_time_now_us(), 1000000U)),
              NX_SUCCESS);
    EXPECT_EQ(task.handle, nullptr);
    EXPECT_EQ(nx_freertos_stack_observe(&binding, &second), NX_ERROR_INVALID);
}

TEST_F(FreeRTOSStackKernel,
       MismatchedOriginalStackIdentityNeverRepaintsTheTask) {
    const StackType_t before = stack[0];
    nx_freertos_stack_binding_t binding{task.handle, &task.control, stack + 1U,
                                        words - 1U};
    nx_stack_observation_t out{77U, 78U, 79U};
    EXPECT_EQ(nx_freertos_stack_observe(&binding, &out), NX_ERROR_STATE);
    EXPECT_EQ(out.capacity_bytes, 77U);
    EXPECT_EQ(stack[0], before);
    binding = {};
    ASSERT_EQ(nx_freertos_task_join(
                  &task, nx_deadline_after(nx_time_now_us(), 1000000U)),
              NX_SUCCESS);
}
} /* namespace */
