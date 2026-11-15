/**
 * \file            freertos_prestart_test.cpp
 * \brief           Actual kernel prestart discard removes all static references
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/freertos.h"
#include <atomic>
#include <cstring>
#include <gtest/gtest.h>

namespace {
/* Host scheduler and sanitizer storage is not a recommended MCU stack budget.
 */
constexpr size_t kStackWords = 32768U;
nx_freertos_task_t s_tasks[3] = {};
alignas(8) StackType_t s_stacks[3][kStackWords] = {};
std::atomic<unsigned> s_runs{0U};
std::atomic<unsigned> s_errors{0U};
std::atomic<bool> s_done{false};

/** \brief Record real scheduled execution after the prestart objects are gone.
 */
void child(void*) {
    s_runs.fetch_add(1U, std::memory_order_release);
}

/** \brief Join actual returning tasks and reject running-scheduler discard. */
void coordinator(void*) {
    for (size_t index = 0U; index < 2U; ++index) {
        if (nx_freertos_task_join(
                &s_tasks[index],
                nx_deadline_after(nx_time_now_us(), 1000000U)) != NX_SUCCESS) {
            s_errors.fetch_add(1U, std::memory_order_release);
        }
    }
    if (nx_freertos_task_discard_before_scheduler(&s_tasks[2]) !=
            NX_ERROR_CONTEXT ||
        s_tasks[2].handle == nullptr || s_tasks[2].finished == nullptr) {
        s_errors.fetch_add(1U, std::memory_order_release);
    }
    s_done.store(true, std::memory_order_release);
    vTaskEndScheduler();
}
} /* namespace */

TEST(FreeRTOSPrestart,
     ReverseDiscardReclaimsCandidateThenSchedulerRunsReusedTCBs) {
    ASSERT_EQ(xTaskGetSchedulerState(), taskSCHEDULER_NOT_STARTED);
    ASSERT_EQ(uxTaskGetNumberOfTasks(), 0U);
    ASSERT_EQ(xTaskGetCurrentTaskHandle(), nullptr);
    for (size_t index = 0U; index < 3U; ++index) {
        ASSERT_EQ(nx_freertos_task_start(
                      &s_tasks[index], "discard", s_stacks[index], kStackWords,
                      static_cast<UBaseType_t>(index + 1U), child, nullptr),
                  NX_SUCCESS);
    }
    ASSERT_EQ(xTaskGetCurrentTaskHandle(), s_tasks[2].handle);
    for (size_t remaining = 3U; remaining > 0U; --remaining) {
        const size_t index = remaining - 1U;
        TaskHandle_t old = s_tasks[index].handle;
        ASSERT_EQ(nx_freertos_task_discard_before_scheduler(&s_tasks[index]),
                  NX_SUCCESS);
        ASSERT_NE(xTaskGetCurrentTaskHandle(), old);
        EXPECT_EQ(uxTaskGetNumberOfTasks(), remaining - 1U);
        EXPECT_EQ(s_tasks[index].handle, nullptr);
        EXPECT_EQ(s_tasks[index].finished, nullptr);
        EXPECT_EQ(s_tasks[index].entry, nullptr);
        EXPECT_EQ(s_tasks[index].context, nullptr);
        /* Caller reuse is legal only after the actual kernel detach succeeded.
         */
        std::memset(&s_tasks[index].control, 0xA5,
                    sizeof(s_tasks[index].control));
        std::memset(s_stacks[index], 0xA5, sizeof(s_stacks[index]));
    }
    ASSERT_EQ(xTaskGetCurrentTaskHandle(), nullptr);
    EXPECT_EQ(s_runs.load(std::memory_order_acquire), 0U);
    for (size_t index = 0U; index < 2U; ++index) {
        ASSERT_EQ(nx_freertos_task_start(
                      &s_tasks[index], "child", s_stacks[index], kStackWords,
                      static_cast<UBaseType_t>(index + 2U), child, nullptr),
                  NX_SUCCESS);
    }
    ASSERT_EQ(nx_freertos_task_start(&s_tasks[2], "owner", s_stacks[2],
                                     kStackWords, 1U, coordinator, nullptr),
              NX_SUCCESS);
    vTaskStartScheduler();
    EXPECT_TRUE(s_done.load(std::memory_order_acquire));
    EXPECT_EQ(s_errors.load(std::memory_order_acquire), 0U);
    EXPECT_EQ(s_runs.load(std::memory_order_acquire), 2U);
    EXPECT_EQ(s_tasks[0].handle, nullptr);
    EXPECT_EQ(s_tasks[1].handle, nullptr);
    EXPECT_EQ(s_tasks[2].entry, nullptr);
    ASSERT_EQ(xTaskGetSchedulerState(), taskSCHEDULER_NOT_STARTED);
    TaskHandle_t owner = s_tasks[2].handle;
    EXPECT_EQ(nx_freertos_task_discard_before_scheduler(&s_tasks[2]),
              NX_ERROR_INVALID);
    EXPECT_EQ(s_tasks[2].handle, owner);
}
