/**
 * \file            threadx_kernel_test.cpp
 * \brief           Real pinned ThreadX kernel contract execution
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/threadx.h"
#include "tx_api.h"
#include "tx_initialize.h"
#include "tx_thread.h"
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <sched.h>
#include <thread>
namespace {
std::atomic<unsigned> mutex_calls;
unsigned cold_observer_calls;
ULONG cold_observer_state;
ULONG cold_observer_flags;
TX_THREAD* cold_observer_current;
TX_THREAD runner;
alignas(8) unsigned char stack[8192];
void execute(ULONG) {
    std::exit(RUN_ALL_TESTS());
}
TEST(ThreadXOfficial, ReallyRunsKernelThread) {
    EXPECT_EQ(tx_thread_identify(), &runner);
    EXPECT_EQ(tx_thread_sleep(1), TX_SUCCESS);
    EXPECT_GE(tx_time_get(), 1U);
}
TEST(ThreadXOfficial, AsynchronousTicksPreserveTaskSignalPosture) {
    sigset_t before{}, after{};
    ASSERT_EQ(pthread_sigmask(SIG_SETMASK, nullptr, &before), 0);
    const auto until = nx_deadline_after(nx_time_now_us(), 10000);
    /* Keep this actual kernel task runnable across asynchronous timer ticks.
     * Read the host monotonic clock without taking a kernel scheduler lock. */
    while (nx_time_now_us() < until) {
    }
    ASSERT_EQ(pthread_sigmask(SIG_SETMASK, nullptr, &after), 0);
    EXPECT_EQ(sigismember(&before, SIGUSR1), 0);
    EXPECT_EQ(sigismember(&after, SIGUSR1), 0);
    for (int signal = 1; signal < NSIG; ++signal) {
        SCOPED_TRACE(signal);
        EXPECT_EQ(sigismember(&after, signal), sigismember(&before, signal));
    }
}
TEST(ThreadXOfficial, ContextObserversPreserveColdAndCriticalLockPosture) {
    EXPECT_EQ(cold_observer_calls, 0U);
    EXPECT_EQ(cold_observer_state, TX_INITIALIZE_IN_PROGRESS);
    EXPECT_EQ(cold_observer_flags, TX_INITIALIZE_IN_PROGRESS);
    EXPECT_EQ(cold_observer_current, nullptr);
    const auto previous = tx_interrupt_control(TX_INT_DISABLE);
    const auto depth = tx_linux_mutex_recursive_count;
    const auto calls = mutex_calls.load(std::memory_order_relaxed);
    const auto state = TX_THREAD_GET_SYSTEM_STATE();
    TX_THREAD* current;
    TX_THREAD_GET_CURRENT(current)
    const auto preempt = _tx_thread_preempt_disable;
    ULONG flags;
    TX_THREAD_SYSTEM_RETURN_CHECK(flags)
    const auto observed_depth = tx_linux_mutex_recursive_count;
    const auto observed_calls = mutex_calls.load(std::memory_order_relaxed);
    tx_interrupt_control(previous);
    EXPECT_EQ(state, 0U);
    EXPECT_EQ(current, &runner);
    EXPECT_EQ(flags, state | preempt);
    EXPECT_EQ(observed_depth, depth);
    EXPECT_EQ(observed_calls - calls, 3U);
}
TEST(ThreadXAdapter, LatchInitializesAndDestroys) {
    nx_threadx_notify_t notify{};
    ASSERT_EQ(nx_threadx_notify_init(&notify), NX_SUCCESS);
    EXPECT_NE(nx_threadx_notify_port(&notify).wake, nullptr);
    EXPECT_EQ(nx_threadx_notify_destroy(&notify), NX_SUCCESS);
}
TEST(ThreadXAdapter, ForeignPthreadCannotBorrowCurrentKernelTask) {
    nx_threadx_notify_t notify{};
    nx_result_t result = NX_SUCCESS;
    std::thread foreign([&] { result = nx_threadx_notify_init(&notify); });
    foreign.join();
    EXPECT_EQ(result, NX_ERROR_CONTEXT);
    if (notify.live) {
        EXPECT_EQ(nx_threadx_notify_destroy(&notify), NX_SUCCESS);
    }
    EXPECT_FALSE(notify.live);
}
TEST(ThreadXAdapter, ActualPortInterruptMaskRejectsWithoutChangingDepth) {
    nx_threadx_notify_t notify{};
    const auto previous = tx_interrupt_control(TX_INT_DISABLE);
    const auto before = tx_linux_mutex_recursive_count;
    const auto result = nx_threadx_notify_init(&notify);
    const auto after = tx_linux_mutex_recursive_count;
    tx_interrupt_control(previous);
    EXPECT_EQ(result, NX_ERROR_CONTEXT);
    EXPECT_EQ(after, before);
    if (notify.live) {
        EXPECT_EQ(nx_threadx_notify_destroy(&notify), NX_SUCCESS);
    }
    EXPECT_FALSE(notify.live);
}
TEST(ThreadXAdapter, OddByteItemsHaveExactFIFOAndNoOutputOnTimeout) {
    nx_threadx_queue_t queue{};
    unsigned char payload[6]{};
    ASSERT_EQ(nx_threadx_queue_init(&queue, payload, sizeof payload, 2, 3),
              NX_SUCCESS);
    unsigned char first[3]{1, 2, 3}, second[3]{4, 5, 6}, out[3]{9, 9, 9};
    EXPECT_EQ(nx_threadx_queue_send_until(&queue, first, 0), NX_SUCCESS);
    EXPECT_EQ(nx_threadx_queue_send_until(&queue, second, 0), NX_SUCCESS);
    EXPECT_EQ(nx_threadx_queue_send_until(&queue, first, 0), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_threadx_queue_receive_until(&queue, out, 0), NX_SUCCESS);
    EXPECT_EQ(std::memcmp(out, first, 3), 0);
    EXPECT_EQ(nx_threadx_queue_receive_until(&queue, out, 0), NX_SUCCESS);
    EXPECT_EQ(std::memcmp(out, second, 3), 0);
    EXPECT_EQ(nx_threadx_queue_receive_until(&queue, out, 0), NX_ERROR_TIMEOUT);
    EXPECT_EQ(std::memcmp(out, second, 3), 0);
    EXPECT_EQ(nx_threadx_queue_destroy(&queue), NX_SUCCESS);
}
TEST(ThreadXAdapter, InvalidCapacityIsRejectedBeforeKernelCreation) {
    nx_threadx_queue_t queue{};
    unsigned char payload[6]{};
    EXPECT_EQ(nx_threadx_queue_init(&queue, payload, sizeof payload, 3, 3),
              NX_ERROR_INVALID);
    EXPECT_FALSE(queue.live);
}

void increment(void* context) {
    ++*static_cast<unsigned*>(context);
}
struct Suspended {
    TX_SEMAPHORE gate{};
    unsigned entered{};
};
void wait_on_gate(void* context) {
    auto* state = static_cast<Suspended*>(context);
    ++state->entered;
    if (tx_semaphore_get(&state->gate, TX_WAIT_FOREVER) != TX_SUCCESS)
        std::abort();
}
TEST(ThreadXTasks, ReturnJoinDeletesBeforeStorageReuse) {
    nx_threadx_task_t task{};
    alignas(8) unsigned char task_stack[1024];
    unsigned calls = 0;
    ASSERT_EQ(nx_threadx_task_start(&task, "return", task_stack,
                                    sizeof task_stack, 8, increment, &calls),
              NX_SUCCESS);
    EXPECT_EQ(nx_threadx_task_join(
                  &task, nx_deadline_after(nx_time_now_us(), 1000000)),
              NX_SUCCESS);
    EXPECT_EQ(calls, 1U);
    EXPECT_FALSE(task.live);
    ASSERT_EQ(nx_threadx_task_start(&task, "reuse", task_stack,
                                    sizeof task_stack, 8, increment, &calls),
              NX_SUCCESS);
    EXPECT_EQ(nx_threadx_task_join(
                  &task, nx_deadline_after(nx_time_now_us(), 1000000)),
              NX_SUCCESS);
    EXPECT_EQ(calls, 2U);
}
TEST(ThreadXTasks, JoinTimeoutRetainsActualBlockedThread) {
    nx_threadx_task_t task{};
    alignas(8) unsigned char task_stack[1024];
    Suspended state;
    ASSERT_EQ(tx_semaphore_create(&state.gate, const_cast<char*>("gate"), 0),
              TX_SUCCESS);
    const auto started =
        nx_threadx_task_start(&task, "blocked", task_stack, sizeof task_stack,
                              8, wait_on_gate, &state);
    EXPECT_EQ(started, NX_SUCCESS);
    if (started != NX_SUCCESS) {
        tx_semaphore_delete(&state.gate);
        return;
    }
    EXPECT_EQ(state.entered, 1U);
    EXPECT_EQ(nx_threadx_task_join(&task, 0), NX_ERROR_TIMEOUT);
    EXPECT_TRUE(task.live);
    EXPECT_EQ(task.context, &state);
    EXPECT_EQ(nx_threadx_task_discard_before_scheduler(&task),
              NX_ERROR_CONTEXT);
    ASSERT_EQ(tx_semaphore_put(&state.gate), TX_SUCCESS);
    EXPECT_EQ(nx_threadx_task_join(
                  &task, nx_deadline_after(nx_time_now_us(), 1000000)),
              NX_SUCCESS);
    EXPECT_EQ(tx_semaphore_delete(&state.gate), TX_SUCCESS);
}
TEST(ThreadXTasks, InvalidStackAndPriorityHaveNoLiveObjects) {
    nx_threadx_task_t task{};
    unsigned calls = 0;
    alignas(8) unsigned char task_stack[1024];
    EXPECT_EQ(nx_threadx_task_start(&task, "short", task_stack, 8, 8, increment,
                                    &calls),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_threadx_task_start(&task, "alignment", task_stack + 1,
                                    sizeof task_stack - 1, 8, increment,
                                    &calls),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_threadx_task_start(&task, "priority", task_stack,
                                    sizeof task_stack, TX_MAX_PRIORITIES,
                                    increment, &calls),
              NX_ERROR_INVALID);
    EXPECT_FALSE(task.live);
    EXPECT_EQ(calls, 0U);
}
nx_result_t bootstrap_start, bootstrap_discard, before_entry;
nx_result_t foreign_bootstrap;
bool foreign_bootstrap_live;
unsigned bootstrap_calls;
nx_threadx_task_t bootstrap_task{};
alignas(8) unsigned char bootstrap_stack[1024];
TEST(ThreadXTasks, PrestartDiscardIsProvenActualKernelBootstrap) {
    EXPECT_EQ(before_entry, NX_ERROR_CONTEXT);
    EXPECT_EQ(bootstrap_start, NX_SUCCESS);
    EXPECT_EQ(bootstrap_discard, NX_SUCCESS);
    EXPECT_EQ(bootstrap_calls, 0U);
    EXPECT_FALSE(bootstrap_task.live);
}
TEST(ThreadXTasks, ForeignPthreadCannotBorrowBootstrapCallback) {
    EXPECT_EQ(foreign_bootstrap, NX_ERROR_CONTEXT);
    EXPECT_FALSE(foreign_bootstrap_live);
}

} /* namespace */
extern "C" int __real_pthread_mutex_timedlock(pthread_mutex_t*,
                                              const struct timespec*);
extern "C" int __wrap_pthread_mutex_timedlock(pthread_mutex_t* mutex,
                                              const struct timespec* until) {
    mutex_calls.fetch_add(1, std::memory_order_relaxed);
    return __real_pthread_mutex_timedlock(mutex, until);
}
void startup(void*) {
    nx_threadx_notify_t notification{};
    std::thread foreign([&] {
        foreign_bootstrap = nx_threadx_notify_init(&notification);
        foreign_bootstrap_live = notification.live;
    });
    foreign.join();
    if (notification.live &&
        nx_threadx_notify_destroy(&notification) != NX_SUCCESS) {
        std::abort();
    }
    bootstrap_start = nx_threadx_task_start(
        &bootstrap_task, "prestart", bootstrap_stack, sizeof bootstrap_stack, 8,
        increment, &bootstrap_calls);
    bootstrap_discard =
        nx_threadx_task_discard_before_scheduler(&bootstrap_task);
    if (tx_thread_create(&runner, const_cast<char*>("google"), execute, 0,
                         stack, sizeof stack, 16, 16, TX_NO_TIME_SLICE,
                         TX_AUTO_START) != TX_SUCCESS) {
        std::abort();
    }
}
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    const auto cold_before = mutex_calls.load(std::memory_order_relaxed);
    cold_observer_state = TX_THREAD_GET_SYSTEM_STATE();
    TX_THREAD_SYSTEM_RETURN_CHECK(cold_observer_flags)
    TX_THREAD_GET_CURRENT(cold_observer_current)
    cold_observer_calls =
        mutex_calls.load(std::memory_order_relaxed) - cold_before;
    before_entry = nx_threadx_task_discard_before_scheduler(&bootstrap_task);
    cpu_set_t allowed;
    if (sched_getaffinity(0, sizeof allowed, &allowed) != 0)
        return 70;
    for (unsigned i = 0; i < CPU_SETSIZE; ++i) {
        if (CPU_ISSET(i, &allowed)) {
            CPU_ZERO(&allowed);
            CPU_SET(i, &allowed);
            if (sched_setaffinity(0, sizeof allowed, &allowed) != 0)
                return 71;
            return static_cast<int>(nx_threadx_kernel_enter(startup, nullptr));
        }
    }
    return 73;
}
