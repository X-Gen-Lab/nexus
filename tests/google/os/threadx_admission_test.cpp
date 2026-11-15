/**
 * \file            threadx_admission_test.cpp
 * \brief           Real kernel cold storage admission boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/threadx.h"
#include <cstring>
#include <gtest/gtest.h>
namespace {
TEST(ThreadXAdmission, QueuePayloadCannotOverlapControl) {
    nx_threadx_queue_t queue{};
    auto result = nx_threadx_queue_init(
        &queue, reinterpret_cast<uint8_t*>(&queue), 4, 1, 4);
    if (result == NX_SUCCESS) {
        EXPECT_EQ(nx_threadx_queue_destroy(&queue), NX_SUCCESS);
    }
    EXPECT_EQ(result, NX_ERROR_INVALID);
    EXPECT_FALSE(queue.live);
}
TEST(ThreadXAdmission, ClosablePayloadCannotOverlapOuterRegistry) {
    nx_threadx_closable_queue_t queue{};
    auto result = nx_threadx_closable_queue_init(
        &queue, reinterpret_cast<uint8_t*>(&queue.maximum_waiters), 4, 1, 4, 1);
    if (result == NX_SUCCESS) {
        EXPECT_EQ(nx_threadx_closable_queue_close(&queue), NX_SUCCESS);
        EXPECT_EQ(nx_threadx_closable_queue_destroy(&queue), NX_SUCCESS);
    }
    EXPECT_EQ(result, NX_ERROR_INVALID);
}
TEST(ThreadXAdmission, WrappedPayloadRangeHasNoKernelEffects) {
    nx_threadx_queue_t queue{};
    auto result = nx_threadx_queue_init(
        &queue, reinterpret_cast<uint8_t*>(UINTPTR_MAX - 1), 4, 1, 4);
    if (result == NX_SUCCESS) {
        EXPECT_EQ(nx_threadx_queue_destroy(&queue), NX_SUCCESS);
    }
    EXPECT_EQ(result, NX_ERROR_INVALID);
    EXPECT_FALSE(queue.live);
}
void noop(void*) {
}
void reclaim_if_live(nx_threadx_task_t* task) {
    if (!task->live)
        return;
    ASSERT_EQ(tx_thread_terminate(&task->control), TX_SUCCESS);
    ASSERT_EQ(tx_thread_delete(&task->control), TX_SUCCESS);
    ASSERT_EQ(tx_semaphore_delete(&task->finished), TX_SUCCESS);
    task->live = false;
}
TEST(ThreadXAdmission, BorrowedNameCannotBeStackFillVictim) {
    nx_threadx_task_t task{};
    alignas(8) unsigned char stack[1024]{};
    std::memcpy(stack, "name", 5);
    auto result = nx_threadx_task_start(&task, reinterpret_cast<char*>(stack),
                                        stack, sizeof stack, 31, noop, nullptr);
    reclaim_if_live(&task);
    EXPECT_EQ(result, NX_ERROR_INVALID);
}
TEST(ThreadXAdmission, ContextCannotAliasKernelControl) {
    nx_threadx_task_t task{};
    alignas(8) unsigned char stack[1024]{};
    auto result = nx_threadx_task_start(&task, "name", stack, sizeof stack, 31,
                                        noop, &task.finished);
    reclaim_if_live(&task);
    EXPECT_EQ(result, NX_ERROR_INVALID);
}
} /* namespace */
