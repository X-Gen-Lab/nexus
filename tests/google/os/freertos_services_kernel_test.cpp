/**
 * \file            freertos_services_kernel_test.cpp
 * \brief           Real pinned kernel exercises close, direct wake and joins
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/freertos.h"
#include <cstdlib>
#include <gtest/gtest.h>

namespace {
constexpr size_t stack_words = 8192;

/** \brief Borrowed worker state remains live until successful external join. */
struct Worker {
    nx_freertos_closable_queue_t* queue;
    nx_freertos_queue_waiter_t* waiter;
    uint32_t item;
    nx_result_t result;
    bool send;
};

/** \brief Exercise actual kernel blocking, without timing-based interleaving.
 */
void queue_worker(void* context) {
    auto* worker = static_cast<Worker*>(context);
    worker->result = worker->send ? nx_freertos_closable_queue_send_until(
                                        worker->queue, &worker->item,
                                        NX_DEADLINE_NEVER, worker->waiter)
                                  : nx_freertos_closable_queue_receive_until(
                                        worker->queue, &worker->item,
                                        NX_DEADLINE_NEVER, worker->waiter);
}

/** \brief Caller-owned controls, latches and task stacks use no kernel heap. */
class FreeRTOSServicesKernelTest : public ::testing::Test {
  protected:
    nx_freertos_closable_queue_t queue{};
    nx_freertos_queue_waiter_t waiters[3]{};
    nx_freertos_task_t tasks[2]{};
    alignas(8) StackType_t stacks[2][stack_words]{};
    Worker workers[2]{};
    uint32_t payload{};

    /** \brief Initialize exact depth and explicit maximum active caller count.
     */
    void SetUp() override {
        ASSERT_EQ(nx_freertos_closable_queue_init(
                      &queue, reinterpret_cast<uint8_t*>(&payload),
                      sizeof payload, 1, sizeof payload, 3),
                  NX_SUCCESS);
        for (auto& waiter : waiters) {
            ASSERT_EQ(nx_freertos_queue_waiter_init(&waiter), NX_SUCCESS);
        }
    }

    /** \brief Close, join and drain before any fixture storage is reclaimed. */
    void TearDown() override {
        if (queue.queue.handle != nullptr) {
            EXPECT_EQ(nx_freertos_closable_queue_close(&queue), NX_SUCCESS);
        }
        for (auto& task : tasks) {
            if (task.handle != nullptr) {
                const auto result = nx_freertos_task_join(
                    &task, nx_deadline_after(nx_time_now_us(), 1000000));
                EXPECT_EQ(result, NX_SUCCESS);
                if (result != NX_SUCCESS) {
                    std::abort();
                }
            }
        }
        if (queue.queue.handle != nullptr) {
            uint32_t item;
            while (nx_freertos_closable_queue_receive_until(
                       &queue, &item, 0, &waiters[2]) == NX_SUCCESS) {
            }
            EXPECT_EQ(nx_freertos_closable_queue_destroy(&queue), NX_SUCCESS);
        }
        for (auto& waiter : waiters) {
            if (waiter.handle != nullptr) {
                EXPECT_EQ(nx_freertos_queue_waiter_destroy(&waiter),
                          NX_SUCCESS);
            }
        }
    }

    /** \brief Higher priority makes registration occur before start returns. */
    void start(unsigned index, bool send, uint32_t item) {
        workers[index] = {&queue, &waiters[index], item, NX_ERROR_STATE, send};
        ASSERT_EQ(nx_freertos_task_start(&tasks[index], "queue", stacks[index],
                                         stack_words, 3, queue_worker,
                                         &workers[index]),
                  NX_SUCCESS);
    }

    /** \brief External deletion establishes permission to inspect/reuse state.
     */
    void join(unsigned index) {
        ASSERT_EQ(
            nx_freertos_task_join(&tasks[index],
                                  nx_deadline_after(nx_time_now_us(), 1000000)),
            NX_SUCCESS);
    }
};

TEST_F(FreeRTOSServicesKernelTest, CloseBroadcastsAllBlockedSendersAndDrains) {
    uint32_t original = 7;
    ASSERT_EQ(nx_freertos_closable_queue_send_until(&queue, &original, 0,
                                                    &waiters[2]),
              NX_SUCCESS);
    start(0, true, 11);
    start(1, true, 13);
    EXPECT_EQ(queue.queue.users, 2U);
    EXPECT_NE(queue.send_waiters, nullptr);
    EXPECT_EQ(nx_freertos_closable_queue_destroy(&queue), NX_ERROR_BUSY);
    ASSERT_EQ(nx_freertos_closable_queue_close(&queue), NX_SUCCESS);
    join(0);
    join(1);
    EXPECT_EQ(workers[0].result, NX_ERROR_CANCELLED);
    EXPECT_EQ(workers[1].result, NX_ERROR_CANCELLED);
    uint32_t value = 99;
    EXPECT_EQ(nx_freertos_closable_queue_receive_until(&queue, &value, 0,
                                                       &waiters[2]),
              NX_SUCCESS);
    EXPECT_EQ(value, original);
    EXPECT_EQ(nx_freertos_closable_queue_receive_until(&queue, &value, 0,
                                                       &waiters[2]),
              NX_ERROR_CANCELLED);
    EXPECT_EQ(
        nx_freertos_closable_queue_send_until(&queue, &value, 0, &waiters[2]),
        NX_ERROR_CANCELLED);
}

TEST_F(FreeRTOSServicesKernelTest, CloseBroadcastsAllBlockedReceivers) {
    start(0, false, 91);
    start(1, false, 92);
    EXPECT_EQ(queue.queue.users, 2U);
    ASSERT_EQ(nx_freertos_closable_queue_close(&queue), NX_SUCCESS);
    join(0);
    join(1);
    EXPECT_EQ(workers[0].result, NX_ERROR_CANCELLED);
    EXPECT_EQ(workers[1].result, NX_ERROR_CANCELLED);
    EXPECT_EQ(workers[0].item, 91U);
    EXPECT_EQ(workers[1].item, 92U);
}

TEST_F(FreeRTOSServicesKernelTest, ProducerWakesRegisteredReceiver) {
    start(0, false, 0);
    EXPECT_EQ(queue.queue.users, 1U);
    uint32_t value = 123;
    ASSERT_EQ(
        nx_freertos_closable_queue_send_until(&queue, &value, 0, &waiters[2]),
        NX_SUCCESS);
    join(0);
    EXPECT_EQ(workers[0].result, NX_SUCCESS);
    EXPECT_EQ(workers[0].item, value);
    EXPECT_EQ(queue.queue.users, 0U);
    EXPECT_EQ(queue.receive_waiters, nullptr);
}

TEST_F(FreeRTOSServicesKernelTest, AbsoluteTimeoutReleasesWaiterForReuse) {
    uint32_t value = 0;
    const auto deadline = nx_deadline_after(nx_time_now_us(), 2000);
    EXPECT_EQ(nx_freertos_closable_queue_receive_until(&queue, &value, deadline,
                                                       &waiters[2]),
              NX_ERROR_TIMEOUT);
    EXPECT_GE(nx_time_now_us(), deadline);
    EXPECT_EQ(waiters[2].active, 0U);
    EXPECT_EQ(queue.queue.users, 0U);
    EXPECT_EQ(queue.receive_waiters, nullptr);
    value = 42;
    ASSERT_EQ(
        nx_freertos_closable_queue_send_until(&queue, &value, 0, &waiters[2]),
        NX_SUCCESS);
    value = 0;
    EXPECT_EQ(nx_freertos_closable_queue_receive_until(&queue, &value, 0,
                                                       &waiters[2]),
              NX_SUCCESS);
    EXPECT_EQ(value, 42U);
}

/** \brief Acquire/release protects the predicate independently of wake counts.
 */
struct DirectPublisher {
    nx_wait_port_t port;
    uint32_t ready;
};

/** \brief A lower-priority producer runs only once the receiver blocks. */
void direct_publisher(void* context) {
    auto* publisher = static_cast<DirectPublisher*>(context);
    __atomic_store_n(&publisher->ready, 1, __ATOMIC_RELEASE);
    if (publisher->port.wake(publisher->port.context) != NX_SUCCESS) {
        std::abort();
    }
}

/** \brief The real predicate remains authoritative after a direct wake. */
bool publisher_ready(void* context) {
    auto* publisher = static_cast<DirectPublisher*>(context);
    return __atomic_load_n(&publisher->ready, __ATOMIC_ACQUIRE) != 0;
}

TEST_F(FreeRTOSServicesKernelTest, DirectSlotWakesActualBoundReceiver) {
    nx_freertos_direct_notify_t notification{};
    ASSERT_EQ(nx_freertos_direct_notify_init(&notification,
                                             xTaskGetCurrentTaskHandle(), 0),
              NX_SUCCESS);
    DirectPublisher publisher{nx_freertos_direct_notify_port(&notification), 0};
    ASSERT_EQ(nx_freertos_task_start(&tasks[0], "publish", stacks[0],
                                     stack_words, 1, direct_publisher,
                                     &publisher),
              NX_SUCCESS);
    EXPECT_EQ(nx_wait_until(&publisher.port, publisher_ready, &publisher,
                            nx_deadline_after(nx_time_now_us(), 1000000)),
              NX_SUCCESS);
    join(0);
    EXPECT_EQ(notification.sequence, 1U);
    EXPECT_EQ(nx_freertos_direct_notify_destroy(&notification), NX_SUCCESS);
}

/** \brief A gate holds a returning task without depending on sleep timing. */
void gated_child(void* context) {
    auto gate = static_cast<SemaphoreHandle_t>(context);
    (void)xSemaphoreTake(gate, portMAX_DELAY);
}

TEST_F(FreeRTOSServicesKernelTest, JoinTimeoutRetainsStackUntilGateAndJoin) {
    StaticSemaphore_t gate_storage{};
    const auto gate = xSemaphoreCreateBinaryStatic(&gate_storage);
    ASSERT_NE(gate, nullptr);
    for (unsigned iteration = 0; iteration < 3; ++iteration) {
        ASSERT_EQ(nx_freertos_task_start(&tasks[0], "gated", stacks[0],
                                         stack_words, 3, gated_child, gate),
                  NX_SUCCESS);
        const auto retained = tasks[0].handle;
        ASSERT_EQ(nx_freertos_task_join(&tasks[0], 0), NX_ERROR_TIMEOUT);
        EXPECT_EQ(tasks[0].handle, retained);
        EXPECT_EQ(tasks[0].context, gate);
        ASSERT_EQ(xSemaphoreGive(gate), pdTRUE);
        join(0);
        EXPECT_EQ(tasks[0].handle, nullptr);
        EXPECT_EQ(tasks[0].finished, nullptr);
    }
    vSemaphoreDelete(gate);
}

/** \brief Permanent resources intentionally survive every test until reset. */
nx_freertos_permanent_task_t permanent_task;
alignas(8) StackType_t permanent_stack[stack_words];
uint32_t permanent_started;

/** \brief A permanent task never returns and never owns a join latch. */
void permanent_entry(void*) {
    __atomic_store_n(&permanent_started, 1, __ATOMIC_RELEASE);
    for (;;) {
        vTaskSuspend(nullptr);
    }
}

TEST(FreeRTOSPermanentKernelTest, CreatesOnlyCallerTCBAndStackUntilReset) {
    const auto task_count = uxTaskGetNumberOfTasks();
    EXPECT_EQ(nx_freertos_permanent_task_start(&permanent_task, "small",
                                               permanent_stack, 1, 3,
                                               permanent_entry, nullptr),
              NX_ERROR_INVALID);
    EXPECT_EQ(uxTaskGetNumberOfTasks(), task_count);
    ASSERT_EQ(nx_freertos_permanent_task_start(&permanent_task, "permanent",
                                               permanent_stack, stack_words, 3,
                                               permanent_entry, nullptr),
              NX_SUCCESS);
    EXPECT_EQ(__atomic_load_n(&permanent_started, __ATOMIC_ACQUIRE), 1U);
    EXPECT_EQ(uxTaskGetNumberOfTasks(), task_count + 1);
    EXPECT_LT(sizeof permanent_task, sizeof(nx_freertos_task_t));
    EXPECT_EQ(nx_freertos_permanent_task_start(&permanent_task, "duplicate",
                                               permanent_stack, stack_words, 3,
                                               permanent_entry, nullptr),
              NX_ERROR_INVALID);
}

/** \brief Static coordinator stack survives until the real scheduler exits. */
StaticTask_t coordinator_control;
alignas(8) StackType_t coordinator_stack[stack_words];
uint32_t done;
int result = 1;

/** \brief Execute every discovered GoogleTest case inside the actual kernel.
 */
void coordinator(void*) {
    result = RUN_ALL_TESTS();
    __atomic_store_n(&done, 1, __ATOMIC_RELEASE);
    vTaskEndScheduler();
    std::abort();
}
} /* namespace */

/** \brief Start the pinned POSIX port once; this is not a kernel mock. */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if (xTaskCreateStatic(coordinator, "google", stack_words, nullptr, 2,
                          coordinator_stack, &coordinator_control) == nullptr) {
        return 1;
    }
    vTaskStartScheduler();
    return __atomic_load_n(&done, __ATOMIC_ACQUIRE) == 1 ? result : 1;
}
