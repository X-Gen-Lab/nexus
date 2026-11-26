/**
 * \file            portable_freertos_test.cpp
 * \brief           Portable consumers execute inside the official kernel
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/storage.h"
#include "portable_consumer.h"
#include <atomic>
#include <cstdlib>
#include <gtest/gtest.h>

namespace {
nx_task_t runner;
NX_OS_STACK_DEFINE(runner_stack, 65536);

/** \brief The actual task may finish before public start returns. */
void increment(void* context) {
    ++*static_cast<unsigned*>(context);
}

/** \brief Never reclaim a local stack or context while the kernel owns it. */
void join(nx_task_t* task) {
    nx_time_us_t deadline = 0;
    if (nx_deadline_finite_after(nx_os_now_us(), 2000000, &deadline) !=
            NX_SUCCESS ||
        nx_task_join_until(task, deadline) != NX_SUCCESS) {
        std::abort();
    }
}

/** \brief Entry can execute before start publishes its native task handle. */
struct SelfJoin {
    nx_task_t* task;
    std::atomic<bool>* start_returned;
    nx_result_t result;
    bool observed_start_returned;
};

/** \brief Observe the actual creation window through independent context. */
void join_self(void* context) {
    auto* state = static_cast<SelfJoin*>(context);
    state->observed_start_returned =
        state->start_returned->load(std::memory_order_acquire);
    state->result = nx_task_join_until(state->task, 0);
}

TEST(PortableFreeRTOSCommon, StackCapacityUsesBytesAndTaskStorageCanBeReused) {
    nx_task_t task{};
    NX_OS_STACK_DEFINE(stack, 65536);
    unsigned calls = 0;
    EXPECT_EQ(nx_task_start(&task, "fraction", stack, sizeof stack - 1, 3,
                            increment, &calls),
              NX_ERROR_INVALID);
    for (unsigned iteration = 0; iteration != 2; ++iteration) {
        ASSERT_EQ(nx_task_start(&task, "return", stack, sizeof stack, 3,
                                increment, &calls),
                  NX_SUCCESS);
        join(&task);
        EXPECT_EQ(calls, iteration + 1);
    }
}

TEST(PortableFreeRTOSCommon, SelfJoinIsContextBeforeStartReturns) {
    nx_task_t task{};
    NX_OS_STACK_DEFINE(stack, 65536);
    std::atomic<bool> start_returned{false};
    SelfJoin state{&task, &start_returned, NX_ERROR_STATE, true};
    ASSERT_EQ(
        nx_task_start(&task, "self", stack, sizeof stack, 3, join_self, &state),
        NX_SUCCESS);
    start_returned.store(true, std::memory_order_release);
    join(&task);
    EXPECT_FALSE(state.observed_start_returned);
    EXPECT_EQ(state.result, NX_ERROR_CONTEXT);
}

TEST(PortableFreeRTOSCommon, SameC11ConsumerDrainsAndReturnsOnClose) {
    nx_channel_t channel{};
    nx_channel_waiter_t producer{}, receiver{};
    nx_task_t task{};
    NX_OS_STACK_DEFINE(stack, 65536);
    uint32_t storage[4]{};
    ASSERT_EQ(nx_channel_init(&channel, storage, sizeof storage, 3,
                              sizeof(uint32_t), 2),
              NX_SUCCESS);
    ASSERT_EQ(nx_channel_waiter_init(&producer), NX_SUCCESS);
    ASSERT_EQ(nx_channel_waiter_init(&receiver), NX_SUCCESS);
    for (uint32_t value : {2U, 3U, 5U}) {
        ASSERT_EQ(nx_channel_send_until(&channel, &value, 0, &producer),
                  NX_SUCCESS);
    }
    ASSERT_EQ(nx_channel_close(&channel), NX_SUCCESS);
    nx_portable_consumer_t consumer{&channel, &receiver, 0, 0, NX_ERROR_STATE};
    ASSERT_EQ(nx_task_start(&task, "consumer", stack, sizeof stack, 3,
                            nx_portable_consumer_run, &consumer),
              NX_SUCCESS);
    join(&task);
    EXPECT_EQ(consumer.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(consumer.count, 3U);
    EXPECT_EQ(consumer.sum, 10U);
    EXPECT_EQ(nx_channel_destroy(&channel), NX_SUCCESS);
    EXPECT_EQ(nx_channel_waiter_destroy(&producer), NX_SUCCESS);
    EXPECT_EQ(nx_channel_waiter_destroy(&receiver), NX_SUCCESS);
}

TEST(PortableFreeRTOSCommon, ExtraQueueCapacityAndPublicC11ExchangeAgree) {
    nx_queue_t queue{};
    uint32_t storage[2]{}, input = 42, output = 0;
    ASSERT_EQ(
        nx_queue_init(&queue, storage, sizeof storage, 1, sizeof(uint32_t)),
        NX_SUCCESS);
    EXPECT_EQ(nx_portable_queue_exchange(&queue, &input, &output, 0),
              NX_SUCCESS);
    EXPECT_EQ(output, input);
    EXPECT_EQ(nx_queue_destroy(&queue), NX_SUCCESS);
}

TEST(PortableFreeRTOSCommon, SleepYieldAndISRRejectionKeepDeadlineDomain) {
    EXPECT_EQ(nx_sleep_until(0), NX_SUCCESS);
    EXPECT_EQ(nx_sleep_until(NX_DEADLINE_NEVER), NX_ERROR_INVALID);
    nx_time_us_t deadline = 0;
    ASSERT_EQ(nx_deadline_finite_after(nx_os_now_us(), 2000, &deadline),
              NX_SUCCESS);
    EXPECT_EQ(nx_sleep_until(deadline), NX_SUCCESS);
    EXPECT_GE(nx_os_now_us(), deadline);
    EXPECT_EQ(nx_task_yield(), NX_SUCCESS);
    nx_notify_t notification{};
    ASSERT_EQ(nx_notify_init(&notification), NX_SUCCESS);
    const auto port = nx_notify_port(&notification);
    const auto sequence = port.arm(port.context);
    EXPECT_EQ(nx_notify_wake_from_isr(&notification), NX_ERROR_CONTEXT);
    EXPECT_EQ(port.arm(port.context), sequence);
    EXPECT_EQ(nx_notify_destroy(&notification), NX_SUCCESS);
}

/** \brief Host process exit is fixture policy, not generic scheduler shutdown.
 */
void execute(void*) {
    std::exit(RUN_ALL_TESTS());
}

/** \brief Public bootstrap creates the actual coordinator before scheduling. */
void startup(void*) {
    if (nx_task_start(&runner, "portable-test", runner_stack,
                      sizeof runner_stack, 2, execute, nullptr) != NX_SUCCESS) {
        std::abort();
    }
}
} /* namespace */

/** \brief Enter the locked POSIX port through the common bootstrap API. */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return static_cast<int>(nx_os_run(startup, nullptr));
}
