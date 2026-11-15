/**
 * \file            threadx_close_test.cpp
 * \brief           Real pinned ThreadX kernel contract execution
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/threadx.h"
#include <cstdlib>
#include <gtest/gtest.h>
namespace {
struct Worker {
    nx_threadx_closable_queue_t* queue;
    nx_threadx_queue_waiter_t* waiter;
    unsigned item;
    nx_result_t result;
    bool send;
};
void worker_entry(void* context) {
    auto* worker = static_cast<Worker*>(context);
    worker->result = worker->send ? nx_threadx_closable_queue_send_until(
                                        worker->queue, &worker->item,
                                        NX_DEADLINE_NEVER, worker->waiter)
                                  : nx_threadx_closable_queue_receive_until(
                                        worker->queue, &worker->item,
                                        NX_DEADLINE_NEVER, worker->waiter);
}
class ThreadXClose : public ::testing::Test {
  protected:
    nx_threadx_closable_queue_t queue{};
    nx_threadx_queue_waiter_t waiters[3]{};
    nx_threadx_task_t tasks[2]{};
    alignas(8) unsigned char stacks[2][1024]{};
    Worker workers[2]{};
    unsigned payload{};
    void SetUp() override {
        ASSERT_EQ(nx_threadx_closable_queue_init(
                      &queue, reinterpret_cast<uint8_t*>(&payload),
                      sizeof payload, 1, sizeof payload, 3),
                  NX_SUCCESS);
        for (auto& waiter : waiters)
            ASSERT_EQ(nx_threadx_queue_waiter_init(&waiter), NX_SUCCESS);
    }
    void TearDown() override {
        if (queue.queue.live) {
            EXPECT_EQ(nx_threadx_closable_queue_close(&queue), NX_SUCCESS);
        }
        for (auto& task : tasks)
            if (task.live) {
                auto result = nx_threadx_task_join(
                    &task, nx_deadline_after(nx_time_now_us(), 1000000));
                EXPECT_EQ(result, NX_SUCCESS);
                if (result != NX_SUCCESS)
                    std::abort();
            }
        if (queue.queue.live) {
            unsigned item;
            while (nx_threadx_closable_queue_receive_until(
                       &queue, &item, 0, &waiters[2]) == NX_SUCCESS) {
            }
            EXPECT_EQ(nx_threadx_closable_queue_destroy(&queue), NX_SUCCESS);
        }
        for (auto& waiter : waiters) {
            if (waiter.notification.live) {
                EXPECT_EQ(nx_threadx_queue_waiter_destroy(&waiter), NX_SUCCESS);
            }
        }
    }
    void start(unsigned index, bool send, unsigned item) {
        workers[index] = {&queue, &waiters[index], item, NX_ERROR_STATE, send};
        ASSERT_EQ(nx_threadx_task_start(&tasks[index], "queue", stacks[index],
                                        sizeof stacks[index], 8, worker_entry,
                                        &workers[index]),
                  NX_SUCCESS);
    }
    void join(unsigned index) {
        ASSERT_EQ(
            nx_threadx_task_join(&tasks[index],
                                 nx_deadline_after(nx_time_now_us(), 1000000)),
            NX_SUCCESS);
    }
};
TEST_F(ThreadXClose, CloseBroadcastsEveryBlockedSenderAndDrainsOriginal) {
    unsigned original = 7;
    ASSERT_EQ(
        nx_threadx_closable_queue_send_until(&queue, &original, 0, &waiters[2]),
        NX_SUCCESS);
    start(0, true, 11);
    start(1, true, 13);
    EXPECT_EQ(queue.queue.users, 2U);
    EXPECT_EQ(nx_threadx_queue_waiter_destroy(&waiters[0]), NX_ERROR_BUSY);
    EXPECT_EQ(nx_threadx_closable_queue_destroy(&queue), NX_ERROR_BUSY);
    ASSERT_EQ(nx_threadx_closable_queue_close(&queue), NX_SUCCESS);
    join(0);
    join(1);
    EXPECT_EQ(workers[0].result, NX_ERROR_CANCELLED);
    EXPECT_EQ(workers[1].result, NX_ERROR_CANCELLED);
    unsigned value = 99;
    EXPECT_EQ(
        nx_threadx_closable_queue_receive_until(&queue, &value, 0, &waiters[2]),
        NX_SUCCESS);
    EXPECT_EQ(value, original);
    EXPECT_EQ(
        nx_threadx_closable_queue_receive_until(&queue, &value, 0, &waiters[2]),
        NX_ERROR_CANCELLED);
    EXPECT_EQ(value, original);
}
TEST_F(ThreadXClose, CloseBroadcastsEveryBlockedReceiver) {
    start(0, false, 11);
    start(1, false, 13);
    EXPECT_EQ(queue.queue.users, 2U);
    ASSERT_EQ(nx_threadx_closable_queue_close(&queue), NX_SUCCESS);
    join(0);
    join(1);
    EXPECT_EQ(workers[0].result, NX_ERROR_CANCELLED);
    EXPECT_EQ(workers[1].result, NX_ERROR_CANCELLED);
    EXPECT_EQ(workers[0].item, 11U);
    EXPECT_EQ(workers[1].item, 13U);
}
TEST_F(ThreadXClose, OneWaiterCannotJoinTwoOperations) {
    start(0, false, 11);
    unsigned value = 9;
    EXPECT_EQ(
        nx_threadx_closable_queue_receive_until(&queue, &value, 0, &waiters[0]),
        NX_ERROR_BUSY);
    EXPECT_EQ(value, 9U);
}
TEST_F(ThreadXClose, ExplicitConcurrentBudgetRejectsWithoutBorrow) {
    queue.maximum_waiters = 1;
    start(0, false, 11);
    start(1, false, 13);
    join(1);
    EXPECT_EQ(workers[1].result, NX_ERROR_BUSY);
    EXPECT_EQ(queue.queue.users, 1U);
    EXPECT_EQ(waiters[1].active, 0U);
}
TEST_F(ThreadXClose, PayloadArrivalWakesSleepingReceiver) {
    start(0, false, 11);
    unsigned value = 42;
    ASSERT_EQ(
        nx_threadx_closable_queue_send_until(&queue, &value, 0, &waiters[2]),
        NX_SUCCESS);
    join(0);
    EXPECT_EQ(workers[0].result, NX_SUCCESS);
    EXPECT_EQ(workers[0].item, 42U);
}
TEST_F(ThreadXClose, FreedSpaceWakesSleepingSender) {
    unsigned original = 7, value = 0;
    ASSERT_EQ(
        nx_threadx_closable_queue_send_until(&queue, &original, 0, &waiters[2]),
        NX_SUCCESS);
    start(0, true, 11);
    ASSERT_EQ(
        nx_threadx_closable_queue_receive_until(&queue, &value, 0, &waiters[2]),
        NX_SUCCESS);
    join(0);
    EXPECT_EQ(workers[0].result, NX_SUCCESS);
    ASSERT_EQ(
        nx_threadx_closable_queue_receive_until(&queue, &value, 0, &waiters[2]),
        NX_SUCCESS);
    EXPECT_EQ(value, 11U);
}
TEST_F(ThreadXClose, DeadlineUnregistersWithoutChangingOutput) {
    unsigned value = 99;
    EXPECT_EQ(
        nx_threadx_closable_queue_receive_until(&queue, &value, 0, &waiters[2]),
        NX_ERROR_TIMEOUT);
    EXPECT_EQ(value, 99U);
    EXPECT_EQ(queue.queue.users, 0U);
    EXPECT_EQ(queue.receive_waiters, nullptr);
    EXPECT_EQ(waiters[2].active, 0U);
}
TEST_F(ThreadXClose, CloseIsIdempotentAndRejectsFurtherSends) {
    ASSERT_EQ(nx_threadx_closable_queue_close(&queue), NX_SUCCESS);
    EXPECT_EQ(nx_threadx_closable_queue_close(&queue), NX_SUCCESS);
    unsigned value = 99;
    EXPECT_EQ(nx_threadx_closable_queue_send_until(
                  &queue, &value, NX_DEADLINE_NEVER, &waiters[2]),
              NX_ERROR_CANCELLED);
    EXPECT_EQ(queue.queue.count, 0U);
}
} /* namespace */
