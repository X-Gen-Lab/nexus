/**
 * \file            threadx_wait_test.cpp
 * \brief           Real pinned ThreadX kernel contract execution
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/os/threadx.h"
#include <array>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
namespace {
struct WakeContext {
    nx_wait_port_t port;
    unsigned calls{};
};
void wake_entry(void* context) {
    auto* c = static_cast<WakeContext*>(context);
    c->calls = 1;
    if (c->port.wake(c->port.context) != NX_SUCCESS)
        std::abort();
}
TEST(ThreadXWait, WakeBeforeWaitIsLatchedAndNeverIsRealInfinity) {
    nx_threadx_notify_t notify{};
    ASSERT_EQ(nx_threadx_notify_init(&notify), NX_SUCCESS);
    auto port = nx_threadx_notify_port(&notify);
    auto sequence = port.arm(port.context);
    EXPECT_EQ(port.wake(port.context), NX_SUCCESS);
    EXPECT_EQ(port.wait(port.context, sequence, NX_DEADLINE_NEVER), NX_SUCCESS);
    EXPECT_EQ(nx_threadx_notify_destroy(&notify), NX_SUCCESS);
}
TEST(ThreadXWait, ExpiredBudgetWinsOverSaturatedUnrelatedHints) {
    nx_threadx_notify_t notify{};
    ASSERT_EQ(nx_threadx_notify_init(&notify), NX_SUCCESS);
    auto port = nx_threadx_notify_port(&notify);
    auto sequence = port.arm(port.context);
    for (unsigned i = 0; i < 12; ++i)
        EXPECT_EQ(port.wake(port.context), NX_SUCCESS);
    EXPECT_EQ(port.wait(port.context, sequence, 0), NX_ERROR_TIMEOUT);
    EXPECT_EQ(notify.control.tx_semaphore_count, 1U);
    EXPECT_EQ(nx_threadx_notify_destroy(&notify), NX_SUCCESS);
}
TEST(ThreadXWait, RealKernelBlockedWaitIsWokenByScheduledPublisher) {
    nx_threadx_notify_t notify{};
    nx_threadx_task_t publisher{};
    alignas(8) unsigned char stack[1024];
    ASSERT_EQ(nx_threadx_notify_init(&notify), NX_SUCCESS);
    auto port = nx_threadx_notify_port(&notify);
    auto sequence = port.arm(port.context);
    WakeContext context{port};
    ASSERT_EQ(nx_threadx_task_start(&publisher, "publisher", stack,
                                    sizeof stack, 20, wake_entry, &context),
              NX_SUCCESS);
    EXPECT_EQ(context.calls, 0U);
    EXPECT_EQ(port.wait(port.context, sequence, NX_DEADLINE_NEVER), NX_SUCCESS);
    EXPECT_EQ(nx_threadx_task_join(
                  &publisher, nx_deadline_after(nx_time_now_us(), 1000000)),
              NX_SUCCESS);
    EXPECT_EQ(context.calls, 1U);
    EXPECT_EQ(nx_threadx_notify_destroy(&notify), NX_SUCCESS);
}
TEST(ThreadXWait, ActualManualMaskRejectsBeforeMutation) {
    nx_threadx_notify_t notify{};
    const auto mask = nx_arch_irq_save();
    EXPECT_EQ(nx_threadx_notify_init(&notify), NX_ERROR_CONTEXT);
    EXPECT_FALSE(notify.live);
    EXPECT_EQ(nx_threadx_kernel_enter(nullptr, nullptr), NX_ERROR_INVALID);
    nx_arch_irq_restore(mask);
    ASSERT_EQ(nx_threadx_notify_init(&notify), NX_SUCCESS);
    auto port = nx_threadx_notify_port(&notify);
    const auto sequence = port.arm(port.context);
    const auto second = nx_arch_irq_save();
    EXPECT_EQ(port.wake(port.context), NX_ERROR_CONTEXT);
    EXPECT_EQ(port.arm(port.context), sequence);
    nx_arch_irq_restore(second);
    EXPECT_EQ(nx_threadx_notify_destroy(&notify), NX_SUCCESS);
    EXPECT_FALSE(nx_threadx_isr_allowed());
}
TEST(ThreadXQueue, LargeNonWordPayloadRetainsExactBytes) {
    nx_threadx_queue_t queue{};
    std::array<unsigned char, 513> payload{}, value{}, output{};
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<unsigned char>(i);
    ASSERT_EQ(nx_threadx_queue_init(&queue, payload.data(), payload.size(), 1,
                                    value.size()),
              NX_SUCCESS);
    EXPECT_EQ(nx_threadx_queue_send_until(&queue, value.data(), 0), NX_SUCCESS);
    EXPECT_EQ(nx_threadx_queue_destroy(&queue), NX_ERROR_BUSY);
    EXPECT_EQ(nx_threadx_queue_receive_until(&queue, output.data(), 0),
              NX_SUCCESS);
    EXPECT_EQ(output, value);
    EXPECT_EQ(nx_threadx_queue_destroy(&queue), NX_SUCCESS);
}
TEST(ThreadXTasks, GoogleMockObservesOnlyExternalApplicationEntry) {
    ::testing::StrictMock<::testing::MockFunction<void(unsigned)>> application;
    EXPECT_CALL(application, Call(42));
    nx_threadx_task_t task{};
    alignas(8) unsigned char stack[1024];
    auto entry = [](void* context) {
        static_cast<::testing::MockFunction<void(unsigned)>*>(context)->Call(
            42);
    };
    ASSERT_EQ(nx_threadx_task_start(&task, "mock-client", stack, sizeof stack,
                                    8, entry, &application),
              NX_SUCCESS);
    ASSERT_EQ(nx_threadx_task_join(
                  &task, nx_deadline_after(nx_time_now_us(), 1000000)),
              NX_SUCCESS);
}
struct SelfJoin {
    nx_threadx_task_t* task;
    nx_result_t result{};
};
void self_entry(void* context) {
    auto* c = static_cast<SelfJoin*>(context);
    c->result = nx_threadx_task_join(c->task, NX_DEADLINE_NEVER);
}
TEST(ThreadXTasks, SelfJoinRejectsWithoutDeletingLiveThread) {
    nx_threadx_task_t task{};
    SelfJoin context{&task};
    alignas(8) unsigned char stack[1024];
    ASSERT_EQ(nx_threadx_task_start(&task, "self", stack, sizeof stack, 8,
                                    self_entry, &context),
              NX_SUCCESS);
    ASSERT_EQ(nx_threadx_task_join(
                  &task, nx_deadline_after(nx_time_now_us(), 1000000)),
              NX_SUCCESS);
    EXPECT_EQ(context.result, NX_ERROR_CONTEXT);
}
TEST(ThreadXQueue, PriorityZeroTimeslicedForeignCallerCannotBreakTaskGuard) {
    nx_threadx_queue_t queue{};
    unsigned payload = 0, value = 42;
    ASSERT_EQ(nx_threadx_queue_init(&queue,
                                    reinterpret_cast<uint8_t*>(&payload),
                                    sizeof payload, 1, sizeof payload),
              NX_SUCCESS);
    UINT old_priority;
    ULONG old_slice;
    auto* current = tx_thread_identify();
    ASSERT_EQ(tx_thread_priority_change(current, 0, &old_priority), TX_SUCCESS);
    ASSERT_EQ(tx_thread_time_slice_change(current, 1, &old_slice), TX_SUCCESS);
    EXPECT_EQ(nx_threadx_queue_send_until(&queue, &value, 0), NX_ERROR_CONTEXT);
    EXPECT_EQ(queue.count, 0U);
    EXPECT_EQ(queue.users, 0U);
    ULONG ignored_slice;
    UINT ignored_priority;
    ASSERT_EQ(tx_thread_time_slice_change(current, old_slice, &ignored_slice),
              TX_SUCCESS);
    ASSERT_EQ(
        tx_thread_priority_change(current, old_priority, &ignored_priority),
        TX_SUCCESS);
    while (nx_threadx_queue_receive_until(&queue, &value, 0) == NX_SUCCESS) {
    }
    EXPECT_EQ(nx_threadx_queue_destroy(&queue), NX_SUCCESS);
}

TX_SEMAPHORE permanent_gate;
nx_threadx_permanent_task_t permanent;
alignas(8) unsigned char permanent_stack[1024];
unsigned permanent_calls;
void permanent_worker(void*) {
    ++permanent_calls;
    for (;;)
        if (tx_semaphore_get(&permanent_gate, TX_WAIT_FOREVER) != TX_SUCCESS)
            std::abort();
}
TEST(ThreadXTasks, PermanentUsesNoCompletionLatchAndRunsRealKernel) {
    ASSERT_EQ(
        tx_semaphore_create(&permanent_gate, const_cast<char*>("permanent"), 0),
        TX_SUCCESS);
    ASSERT_EQ(nx_threadx_permanent_task_start(
                  &permanent, "permanent", permanent_stack,
                  sizeof permanent_stack, 8, permanent_worker, nullptr),
              NX_SUCCESS);
    EXPECT_EQ(permanent_calls, 1U);
    EXPECT_TRUE(permanent.live);
    EXPECT_LT(sizeof(nx_threadx_permanent_task_t), sizeof(nx_threadx_task_t));
}
} /* namespace */
