/**
 * \file            portable_threadx_test.cpp
 * \brief           Real kernel regressions for portable ThreadX admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/storage.h"
#include "portable_consumer.h"
#include <cstdlib>
#include <gtest/gtest.h>
#include <sched.h>

namespace {
nx_task_t runner;
NX_OS_STACK_DEFINE(runner_stack, 8192);

/** \brief Run the complete small contract executable inside the real kernel. */
void execute(void*) {
    std::exit(RUN_ALL_TESTS());
}

/** \brief Count entry only after actual kernel execution. */
void increment(void* context) {
    ++*static_cast<unsigned*>(context);
}

TEST(PortableThreadXTransition, QueueAcceptsCapacityBeyondTheUsedPayload) {
    nx_queue_t queue{};
    unsigned char storage[9]{};
    const auto result = nx_queue_init(&queue, storage, sizeof storage, 2, 3);
    EXPECT_EQ(result, NX_SUCCESS);
    if (result == NX_SUCCESS) {
        EXPECT_EQ(nx_queue_destroy(&queue), NX_SUCCESS);
    }
}

TEST(PortableThreadXTransition, EmptyTaskNameIsRejectedBeforeCreation) {
    nx_task_t task{};
    NX_OS_STACK_DEFINE(stack, 1024);
    unsigned calls = 0;
    const auto result =
        nx_task_start(&task, "", stack, sizeof stack, 1, increment, &calls);
    EXPECT_EQ(result, NX_ERROR_INVALID);
    if (result == NX_SUCCESS) {
        nx_time_us_t deadline = 0;
        if (nx_deadline_finite_after(nx_os_now_us(), 2000000, &deadline) !=
                NX_SUCCESS ||
            nx_task_join_until(&task, deadline) != NX_SUCCESS) {
            std::abort();
        }
    }
    EXPECT_EQ(calls, 0U);
}

TEST(PortableThreadXCommon, LogicalPriorityPreservesHigherReadyTaskOrder) {
    nx_task_t task{};
    NX_OS_STACK_DEFINE(stack, 1024);
    unsigned calls = 0;
    ASSERT_EQ(nx_task_start(&task, "higher", stack, sizeof stack, 1, increment,
                            &calls),
              NX_SUCCESS);
    EXPECT_EQ(calls, 1U);
    nx_time_us_t deadline = 0;
    ASSERT_EQ(nx_deadline_finite_after(nx_os_now_us(), 2000000, &deadline),
              NX_SUCCESS);
    const auto joined = nx_task_join_until(&task, deadline);
    EXPECT_EQ(joined, NX_SUCCESS);
    if (joined != NX_SUCCESS) {
        std::abort();
    }
}

TEST(PortableThreadXCommon, SameC11ConsumerDrainsAndReturnsOnClose) {
    nx_channel_t channel{};
    nx_channel_waiter_t producer{}, receiver{};
    nx_task_t task{};
    NX_OS_STACK_DEFINE(stack, 1024);
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
    ASSERT_EQ(nx_task_start(&task, "consumer", stack, sizeof stack, 1,
                            nx_portable_consumer_run, &consumer),
              NX_SUCCESS);
    nx_time_us_t deadline = 0;
    if (nx_deadline_finite_after(nx_os_now_us(), 2000000, &deadline) !=
            NX_SUCCESS ||
        nx_task_join_until(&task, deadline) != NX_SUCCESS) {
        std::abort();
    }
    EXPECT_EQ(consumer.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(consumer.count, 3U);
    EXPECT_EQ(consumer.sum, 10U);
    EXPECT_EQ(nx_channel_destroy(&channel), NX_SUCCESS);
    EXPECT_EQ(nx_channel_waiter_destroy(&producer), NX_SUCCESS);
    EXPECT_EQ(nx_channel_waiter_destroy(&receiver), NX_SUCCESS);
}

TEST(PortableThreadXCommon, SleepAndISRAdmissionUsePublicContracts) {
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

/** \brief The adapter establishes the actual ThreadX bootstrap scope. */
void startup(void*) {
    if (nx_task_start(&runner, "portable-test", runner_stack,
                      sizeof runner_stack, 0, execute, nullptr) != NX_SUCCESS) {
        std::abort();
    }
}
} /* namespace */

/** \brief Match the maintained real Linux-kernel affinity contract. */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    cpu_set_t allowed;
    if (sched_getaffinity(0, sizeof allowed, &allowed) != 0) {
        return 70;
    }
    for (unsigned i = 0; i < CPU_SETSIZE; ++i) {
        if (CPU_ISSET(i, &allowed)) {
            CPU_ZERO(&allowed);
            CPU_SET(i, &allowed);
            if (sched_setaffinity(0, sizeof allowed, &allowed) != 0) {
                return 71;
            }
            return static_cast<int>(nx_os_run(startup, nullptr));
        }
    }
    return 73;
}
