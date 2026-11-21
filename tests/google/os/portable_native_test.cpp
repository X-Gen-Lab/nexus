/**
 * \file            portable_native_test.cpp
 * \brief           Real pthread behavior through the portable OS interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/storage.h"
#include "portable_consumer.h"
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <mutex>
#include <new>

namespace {
NX_OS_STACK_DEFINE(first_stack, 524288);
NX_OS_STACK_DEFINE(second_stack, 524288);

/** \brief A finite budget never silently becomes an infinite sentinel. */
nx_time_us_t after(uint64_t budget) {
    nx_time_us_t deadline = 0;
    if (nx_deadline_finite_after(nx_os_now_us(), budget, &deadline) !=
        NX_SUCCESS) {
        std::abort();
    }
    return deadline;
}

/** \brief Never return from a test while a task still borrows local storage. */
void require_join(nx_task_t* task) {
    const auto result = nx_task_join_until(task, after(2000000));
    EXPECT_EQ(result, NX_SUCCESS);
    if (result != NX_SUCCESS) {
        std::abort();
    }
}

/** \brief Observe only the external cond-wait boundary, without timing sleeps.
 */
class WaitBoundary {
  public:
    /** \brief Begin observing one channel worker's actual blocking call. */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex);
        entered = false;
    }
    /** \brief Publish arrival before the real atomic unlock-and-wait. */
    void observe() {
        std::lock_guard<std::mutex> lock(mutex);
        entered = true;
        condition.notify_all();
    }
    /** \brief Bound test coordination independently of the API under test. */
    bool await() {
        std::unique_lock<std::mutex> lock(mutex);
        return condition.wait_for(lock, std::chrono::seconds(2),
                                  [this] { return entered; });
    }

  private:
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false;
};
WaitBoundary boundary;
thread_local bool observe_channel_wait;

/** \brief Returning tasks retain their context until the public join succeeds.
 */
void increment(void* context) {
    ++*static_cast<unsigned*>(context);
}

/** \brief A gate prevents completion while finite join is exercised. */
struct Gate {
    nx_notify_t notification{};
    std::atomic<bool> ready{false};
    nx_result_t result = NX_ERROR_STATE;
};

/** \brief Read the authoritative predicate, independently of wake counts. */
bool gate_ready(void* context) {
    return static_cast<Gate*>(context)->ready.load(std::memory_order_acquire);
}

/** \brief Wait for cooperative release using only public notification ports. */
void gated_entry(void* context) {
    auto* gate = static_cast<Gate*>(context);
    const auto port = nx_notify_port(&gate->notification);
    gate->result = nx_wait_until(&port, gate_ready, gate, NX_DEADLINE_NEVER);
}

/** \brief Actual pthread teardown can outlive the application entry. */
struct ExitGate {
    pthread_key_t key{};
    Gate gate;
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false;
    unsigned payload = 0;
};
thread_local ExitGate* release_on_busy_join;

/** \brief Retain the real pthread during its thread-specific destructor. */
void block_thread_destructor(void* context) {
    auto* state = static_cast<ExitGate*>(context);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->entered = true;
        state->condition.notify_all();
    }
    gated_entry(&state->gate);
}

/** \brief Return before the destructor allows the kernel thread to exit. */
void attach_exit_gate(void* context) {
    auto* state = static_cast<ExitGate*>(context);
    if (pthread_setspecific(state->key, state) != 0) {
        std::abort();
    }
}

/** \brief Publish ordinary caller data only during real pthread teardown. */
void write_after_thread_destructor_gate(void* context) {
    block_thread_destructor(context);
    static_cast<ExitGate*>(context)->payload = 73U;
}

/** \brief A task can identify self-join before external start publication. */
struct SelfJoin {
    nx_task_t* task;
    nx_result_t result;
};

/** \brief Exercise the common self-join result from the actual child pthread.
 */
void join_self(void* context) {
    auto* state = static_cast<SelfJoin*>(context);
    state->result = nx_task_join_until(state->task, 0);
}

TEST(PortableOSTask, ReturnJoinAndReuseKeepOnePublicTaskType) {
    nx_task_t task{};
    unsigned calls = 0;
    for (unsigned iteration = 0; iteration != 2; ++iteration) {
        ASSERT_EQ(nx_task_start(&task, "portable", first_stack,
                                sizeof first_stack, 0, increment, &calls),
                  NX_SUCCESS);
        require_join(&task);
        EXPECT_EQ(calls, iteration + 1);
    }
}

TEST(PortableOSTask, FiniteJoinTimeoutRetainsTaskAndBorrowedContext) {
    nx_task_t task{};
    Gate gate;
    ASSERT_EQ(nx_notify_init(&gate.notification), NX_SUCCESS);
    ASSERT_EQ(nx_task_start(&task, "gated", first_stack, sizeof first_stack, 0,
                            gated_entry, &gate),
              NX_SUCCESS);
    const auto deadline = after(2000);
    EXPECT_EQ(nx_task_join_until(&task, deadline), NX_ERROR_TIMEOUT);
    EXPECT_GE(nx_os_now_us(), deadline);
    gate.ready.store(true, std::memory_order_release);
    EXPECT_EQ(nx_notify_wake(&gate.notification), NX_SUCCESS);
    const auto joined = nx_task_join_until(&task, after(2000000));
    EXPECT_EQ(joined, NX_SUCCESS);
    if (joined != NX_SUCCESS) {
        std::abort();
    }
    EXPECT_EQ(gate.result, NX_SUCCESS);
    EXPECT_EQ(nx_notify_destroy(&gate.notification), NX_SUCCESS);
}

TEST(PortableOSTask, EntryReturnDoesNotReleaseAStillRunningPthreadDestructor) {
    nx_task_t task{};
    ExitGate state;
    ASSERT_EQ(pthread_key_create(&state.key, block_thread_destructor), 0);
    ASSERT_EQ(nx_notify_init(&state.gate.notification), NX_SUCCESS);
    ASSERT_EQ(nx_task_start(&task, "teardown", first_stack, sizeof first_stack,
                            0, attach_exit_gate, &state),
              NX_SUCCESS);
    {
        std::unique_lock<std::mutex> lock(state.mutex);
        EXPECT_TRUE(state.condition.wait_for(
            lock, std::chrono::seconds(2), [&state] { return state.entered; }));
    }
    const auto deadline = after(2000);
    EXPECT_EQ(nx_task_join_until(&task, deadline), NX_ERROR_TIMEOUT);
    EXPECT_GE(nx_os_now_us(), deadline);
    state.gate.ready.store(true, std::memory_order_release);
    EXPECT_EQ(nx_notify_wake(&state.gate.notification), NX_SUCCESS);
    require_join(&task);
    EXPECT_EQ(state.gate.result, NX_SUCCESS);
    EXPECT_EQ(nx_notify_destroy(&state.gate.notification), NX_SUCCESS);
    EXPECT_EQ(pthread_key_delete(state.key), 0);
}

TEST(PortableOSTask, SuccessfulFiniteJoinObservesPlainTLSDestructorPayload) {
    nx_task_t task{};
    ExitGate state;
    ASSERT_EQ(
        pthread_key_create(&state.key, write_after_thread_destructor_gate), 0);
    ASSERT_EQ(nx_notify_init(&state.gate.notification), NX_SUCCESS);
    ASSERT_EQ(nx_task_start(&task, "teardown-data", first_stack,
                            sizeof first_stack, 0, attach_exit_gate, &state),
              NX_SUCCESS);
    {
        std::unique_lock<std::mutex> lock(state.mutex);
        EXPECT_TRUE(state.condition.wait_for(
            lock, std::chrono::seconds(2), [&state] { return state.entered; }));
    }
    /* The wrapper releases the child only after a real busy join. Its gate
     * orders parent to child; only successful join publishes the payload. */
    release_on_busy_join = &state;
    require_join(&task);
    EXPECT_EQ(release_on_busy_join, nullptr);
    EXPECT_EQ(state.payload, 73U);
    EXPECT_EQ(state.gate.result, NX_SUCCESS);
    EXPECT_EQ(nx_notify_destroy(&state.gate.notification), NX_SUCCESS);
    EXPECT_EQ(pthread_key_delete(state.key), 0);
}

TEST(PortableOSTask, SelfJoinRejectsWithoutReclaimingTheRunningTask) {
    nx_task_t task{};
    SelfJoin state{&task, NX_ERROR_STATE};
    ASSERT_EQ(nx_task_start(&task, "self", first_stack, sizeof first_stack, 0,
                            join_self, &state),
              NX_SUCCESS);
    require_join(&task);
    EXPECT_EQ(state.result, NX_ERROR_CONTEXT);
}

TEST(PortableOSTask, InvalidPriorityDoesNotExecuteEntry) {
    nx_task_t task{};
    unsigned calls = 0;
    EXPECT_EQ(nx_task_start(&task, "priority", first_stack, sizeof first_stack,
                            NX_OS_PRIORITY_LEVELS, increment, &calls),
              NX_ERROR_INVALID);
    EXPECT_EQ(calls, 0U);
}

TEST(PortableOSTask, OverlappingControlAndValidHostStackAreRejected) {
    alignas(64) unsigned char storage[65536]{};
    auto* task = new (storage) nx_task_t{};
    unsigned calls = 0;
    EXPECT_EQ(nx_task_start(task, "overlap", storage, sizeof storage, 0,
                            increment, &calls),
              NX_ERROR_INVALID);
    EXPECT_EQ(calls, 0U);
}

TEST(PortableOSQueue, ArbitraryByteItemsKeepFIFOAndImmediateTrySemantics) {
    nx_queue_t queue{};
    unsigned char bytes[6]{};
    const unsigned char first[3]{1, 2, 3}, second[3]{4, 5, 6};
    unsigned char output[3]{};
    ASSERT_EQ(nx_queue_init(&queue, bytes, sizeof bytes, 2, 3), NX_SUCCESS);
    EXPECT_EQ(nx_queue_send_until(&queue, first, 0), NX_SUCCESS);
    EXPECT_EQ(nx_queue_send_until(&queue, second, 0), NX_SUCCESS);
    EXPECT_EQ(nx_queue_receive_until(&queue, output, 0), NX_SUCCESS);
    EXPECT_EQ(std::memcmp(output, first, sizeof output), 0);
    EXPECT_EQ(nx_queue_receive_until(&queue, output, 0), NX_SUCCESS);
    EXPECT_EQ(std::memcmp(output, second, sizeof output), 0);
    EXPECT_EQ(nx_queue_destroy(&queue), NX_SUCCESS);
}

TEST(PortableOSQueue, TimeoutPreservesOutputAndDoesNotConsumeAnItem) {
    nx_queue_t queue{};
    uint32_t bytes{}, first = 7, second = 9, output = 91;
    ASSERT_EQ(nx_queue_init(&queue, &bytes, sizeof bytes, 1, sizeof first),
              NX_SUCCESS);
    EXPECT_EQ(nx_queue_receive_until(&queue, &output, 0), NX_ERROR_TIMEOUT);
    EXPECT_EQ(output, 91U);
    EXPECT_EQ(nx_queue_send_until(&queue, &first, 0), NX_SUCCESS);
    EXPECT_EQ(nx_queue_send_until(&queue, &second, 0), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_queue_receive_until(&queue, &output, 0), NX_SUCCESS);
    EXPECT_EQ(output, first);
    EXPECT_EQ(nx_queue_destroy(&queue), NX_SUCCESS);
}

TEST(PortableOSQueue, InsufficientCapacityRejectsBeforeResourcesAreOwned) {
    nx_queue_t queue{};
    unsigned char bytes[9]{};
    EXPECT_EQ(nx_queue_init(&queue, bytes, sizeof bytes, 4, 3),
              NX_ERROR_INVALID);
    ASSERT_EQ(nx_queue_init(&queue, bytes, sizeof bytes, 2, 3), NX_SUCCESS);
    EXPECT_EQ(nx_queue_destroy(&queue), NX_SUCCESS);
}

TEST(PortableOSQueue, C11BusinessSourceUsesOnlyPublicQueueReferences) {
    nx_queue_t queue{};
    uint32_t bytes{}, input = 42, output = 0;
    ASSERT_EQ(nx_queue_init(&queue, &bytes, sizeof bytes, 1, sizeof input),
              NX_SUCCESS);
    EXPECT_EQ(nx_portable_queue_exchange(&queue, &input, &output, 0),
              NX_SUCCESS);
    EXPECT_EQ(output, input);
    EXPECT_EQ(nx_queue_destroy(&queue), NX_SUCCESS);
}

TEST(PortableOSNotify, CommonPortLatchesWakeAndStillChecksFiniteExpiry) {
    nx_notify_t notification{};
    ASSERT_EQ(nx_notify_init(&notification), NX_SUCCESS);
    const auto port = nx_notify_port(&notification);
    const auto sequence = port.arm(port.context);
    EXPECT_EQ(nx_notify_wake(&notification), NX_SUCCESS);
    EXPECT_EQ(port.wait(port.context, sequence, after(2000000)), NX_SUCCESS);
    EXPECT_EQ(port.wait(port.context, sequence, 0), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_notify_destroy(&notification), NX_SUCCESS);
}

TEST(PortableOSNotify, NativeISRRejectionDoesNotPublishASequence) {
    nx_notify_t notification{};
    ASSERT_EQ(nx_notify_init(&notification), NX_SUCCESS);
    const auto port = nx_notify_port(&notification);
    const auto sequence = port.arm(port.context);
    EXPECT_EQ(nx_notify_wake_from_isr(&notification), NX_ERROR_CONTEXT);
    EXPECT_EQ(port.arm(port.context), sequence);
    EXPECT_EQ(nx_notify_destroy(&notification), NX_SUCCESS);
}

/** \brief Every fixture task uses only the common channel and task contracts.
 */
class PortableOSChannel : public ::testing::Test {
  protected:
    nx_channel_t channel{};
    nx_channel_waiter_t waiters[3]{};
    nx_task_t tasks[2]{};
    uint32_t payload[5]{};
    bool live = false;
    bool waiter_live[3]{};
    bool task_live[2]{};

    /** \brief Choose bounded caller-owned capacities, without native handles.
     */
    void SetUp() override {
        ASSERT_EQ(nx_channel_init(&channel, payload, sizeof payload, 4,
                                  sizeof(uint32_t), 3),
                  NX_SUCCESS);
        live = true;
        for (unsigned i = 0; i != 3; ++i) {
            ASSERT_EQ(nx_channel_waiter_init(&waiters[i]), NX_SUCCESS);
            waiter_live[i] = true;
        }
    }

    /** \brief Close and really join before reclaiming any borrowed fixture. */
    void TearDown() override {
        if (live) {
            EXPECT_EQ(nx_channel_close(&channel), NX_SUCCESS);
        }
        for (unsigned i = 0; i != 2; ++i) {
            if (task_live[i]) {
                const auto result =
                    nx_task_join_until(&tasks[i], after(2000000));
                EXPECT_EQ(result, NX_SUCCESS);
                if (result != NX_SUCCESS) {
                    std::abort();
                }
            }
        }
        if (live && waiter_live[2]) {
            uint32_t item = 0;
            while (nx_channel_receive_until(&channel, &item, 0, &waiters[2]) ==
                   NX_SUCCESS) {
            }
            EXPECT_EQ(nx_channel_destroy(&channel), NX_SUCCESS);
        }
        for (unsigned i = 0; i != 3; ++i) {
            if (waiter_live[i]) {
                EXPECT_EQ(nx_channel_waiter_destroy(&waiters[i]), NX_SUCCESS);
            }
        }
    }

    /** \brief Persist the task ownership receipt before assertions can return.
     */
    nx_result_t start(unsigned index, void (*entry)(void*), void* context) {
        void* stack = index == 0 ? static_cast<void*>(first_stack)
                                 : static_cast<void*>(second_stack);
        const auto result =
            nx_task_start(&tasks[index], "channel", stack, sizeof first_stack,
                          0, entry, context);
        task_live[index] = result == NX_SUCCESS;
        return result;
    }
};

/** \brief Each simultaneous operation owns a distinct explicit waiter. */
struct ChannelCall {
    nx_channel_t* channel;
    nx_channel_waiter_t* waiter;
    uint32_t value;
    nx_result_t result;
    bool send;
};

/** \brief Observe actual blocking at the libc boundary without private fields.
 */
void channel_entry(void* context) {
    auto* call = static_cast<ChannelCall*>(context);
    observe_channel_wait = true;
    call->result =
        call->send ? nx_channel_send_until(call->channel, &call->value,
                                           NX_DEADLINE_NEVER, call->waiter)
                   : nx_channel_receive_until(call->channel, &call->value,
                                              NX_DEADLINE_NEVER, call->waiter);
    observe_channel_wait = false;
}

TEST_F(PortableOSChannel, CloseIsIdempotentAndRetainsQueuedItemsForDrain) {
    uint32_t input = 7, output = 91;
    ASSERT_EQ(nx_channel_send_until(&channel, &input, 0, &waiters[2]),
              NX_SUCCESS);
    EXPECT_EQ(nx_channel_close(&channel), NX_SUCCESS);
    EXPECT_EQ(nx_channel_close(&channel), NX_SUCCESS);
    EXPECT_EQ(nx_channel_send_until(&channel, &input, 0, &waiters[2]),
              NX_ERROR_CANCELLED);
    EXPECT_EQ(nx_channel_receive_until(&channel, &output, 0, &waiters[2]),
              NX_SUCCESS);
    EXPECT_EQ(output, input);
    output = 91;
    EXPECT_EQ(nx_channel_receive_until(&channel, &output, 0, &waiters[2]),
              NX_ERROR_CANCELLED);
    EXPECT_EQ(output, 91U);
}

TEST_F(PortableOSChannel, CloseWakesAnActuallyBlockedReceiver) {
    ChannelCall call{&channel, &waiters[0], 91, NX_ERROR_STATE, false};
    boundary.reset();
    ASSERT_EQ(start(0, channel_entry, &call), NX_SUCCESS);
    EXPECT_TRUE(boundary.await());
    EXPECT_EQ(nx_channel_close(&channel), NX_SUCCESS);
    require_join(&tasks[0]);
    task_live[0] = false;
    EXPECT_EQ(call.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(call.value, 91U);
}

TEST_F(PortableOSChannel, CloseWakesAnActuallyBlockedSender) {
    uint32_t input = 7;
    for (unsigned i = 0; i != 4; ++i) {
        ASSERT_EQ(nx_channel_send_until(&channel, &input, 0, &waiters[2]),
                  NX_SUCCESS);
    }
    ChannelCall call{&channel, &waiters[0], 91, NX_ERROR_STATE, true};
    boundary.reset();
    ASSERT_EQ(start(0, channel_entry, &call), NX_SUCCESS);
    EXPECT_TRUE(boundary.await());
    EXPECT_EQ(nx_channel_close(&channel), NX_SUCCESS);
    require_join(&tasks[0]);
    task_live[0] = false;
    EXPECT_EQ(call.result, NX_ERROR_CANCELLED);
}

TEST_F(PortableOSChannel, C11ConsumerDrainsAndExitsWithoutKernelNames) {
    for (uint32_t value : {2U, 3U, 5U}) {
        ASSERT_EQ(nx_channel_send_until(&channel, &value, 0, &waiters[2]),
                  NX_SUCCESS);
    }
    ASSERT_EQ(nx_channel_close(&channel), NX_SUCCESS);
    nx_portable_consumer_t consumer{&channel, &waiters[0], 0, 0,
                                    NX_ERROR_STATE};
    ASSERT_EQ(start(0, nx_portable_consumer_run, &consumer), NX_SUCCESS);
    require_join(&tasks[0]);
    task_live[0] = false;
    EXPECT_EQ(consumer.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(consumer.count, 3U);
    EXPECT_EQ(consumer.sum, 10U);
}

TEST(PortableOSTime, FiniteSleepUsesTheSameMonotonicDeadlineDomain) {
    EXPECT_EQ(nx_sleep_until(0), NX_SUCCESS);
    const auto deadline = after(2000);
    EXPECT_EQ(nx_sleep_until(deadline), NX_SUCCESS);
    EXPECT_GE(nx_os_now_us(), deadline);
    EXPECT_EQ(nx_sleep_until(NX_DEADLINE_NEVER), NX_ERROR_INVALID);
    EXPECT_EQ(nx_task_yield(), NX_SUCCESS);
}
} /* namespace */

extern "C" int __real_pthread_cond_wait(pthread_cond_t*, pthread_mutex_t*);
extern "C" int __real_pthread_cond_timedwait(pthread_cond_t*, pthread_mutex_t*,
                                             const struct timespec*);
extern "C" int __real_pthread_tryjoin_np(pthread_t, void**);

/** \brief Release teardown only after the real join reports a live pthread. */
extern "C" int __wrap_pthread_tryjoin_np(pthread_t thread, void** result) {
    const int error = __real_pthread_tryjoin_np(thread, result);
    if (error == EBUSY && release_on_busy_join != nullptr) {
        auto* state = release_on_busy_join;
        release_on_busy_join = nullptr;
        state->gate.ready.store(true, std::memory_order_release);
        if (nx_notify_wake(&state->gate.notification) != NX_SUCCESS) {
            std::abort();
        }
    }
    return error;
}

/** \brief Interpose only the external wait boundary and call real pthread. */
extern "C" int __wrap_pthread_cond_wait(pthread_cond_t* condition,
                                        pthread_mutex_t* mutex) {
    if (observe_channel_wait) {
        boundary.observe();
    }
    return __real_pthread_cond_wait(condition, mutex);
}

/** \brief Finite waits retain the same real external operation. */
extern "C" int __wrap_pthread_cond_timedwait(pthread_cond_t* condition,
                                             pthread_mutex_t* mutex,
                                             const struct timespec* deadline) {
    if (observe_channel_wait) {
        boundary.observe();
    }
    return __real_pthread_cond_timedwait(condition, mutex, deadline);
}
