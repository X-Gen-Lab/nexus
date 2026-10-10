/**
 * \file            freertos_context_test.cpp
 * \brief           Real FreeRTOS adapter rejects unsafe CPU contexts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/os/freertos.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tuple>

namespace {
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::StrictMock;

/** \brief Mock kernel calls, never the adapter or its object state. */
class Kernel {
  public:
    MOCK_METHOD(QueueHandle_t, create_queue,
                (UBaseType_t, UBaseType_t, uint8_t*, StaticQueue_t*, uint8_t));
    MOCK_METHOD(TaskHandle_t, create_task,
                (TaskFunction_t, const char*, configSTACK_DEPTH_TYPE, void*,
                 UBaseType_t, StackType_t*, StaticTask_t*));
    MOCK_METHOD(BaseType_t, scheduler, ());
    MOCK_METHOD(TaskHandle_t, current_task, ());
    MOCK_METHOD(BaseType_t, send,
                (QueueHandle_t, const void*, TickType_t, BaseType_t));
    MOCK_METHOD(BaseType_t, receive, (QueueHandle_t, void*, TickType_t));
    MOCK_METHOD(BaseType_t, take, (QueueHandle_t, TickType_t));
    MOCK_METHOD(BaseType_t, give_isr, (QueueHandle_t, BaseType_t*));
    MOCK_METHOD(UBaseType_t, messages, (QueueHandle_t));
    MOCK_METHOD(void, delete_queue, (QueueHandle_t));
    MOCK_METHOD(void, delete_task, (TaskHandle_t));
    MOCK_METHOD(void, suspend, (TaskHandle_t));
    MOCK_METHOD(void, yield, ());
    MOCK_METHOD(nx_time_us_t, now, ());
};

/** \brief Inject only the CPU context that guards external kernel calls. */
class Cpu {
  public:
    MOCK_METHOD(bool, in_isr, ());
    MOCK_METHOD(bool, masked, ());
    MOCK_METHOD(bool, privileged, ());
    MOCK_METHOD(uint32_t, exception, ());
    MOCK_METHOD(nx_arch_irq_masks_t, masks, ());
    MOCK_METHOD(uint32_t, priority_group, ());
    MOCK_METHOD(uint8_t, irq_priority, (uint32_t));
};

Kernel* kernel;
Cpu* cpu;

enum class Context { Masked, Unprivileged, Isr };
enum class Operation {
    NotifyInit,
    NotifyDestroy,
    TaskStart,
    TaskJoin,
    QueueInit,
    QueueSend,
    QueueReceive,
    QueueDestroy,
    NotifyWait,
    NotifyWake
};

/** \brief Exact caller-owned storage and deterministic external handles. */
class FreeRTOSContextTest : public ::testing::Test {
  protected:
    StrictMock<Kernel> calls;
    StrictMock<Cpu> context;
    nx_freertos_notify_t notification{};
    nx_freertos_task_t task{};
    nx_freertos_queue_t queue{};
    StackType_t stack[16]{};
    uint8_t bytes[16]{};
    unsigned item = 23;
    QueueHandle_t latch = reinterpret_cast<QueueHandle_t>(uintptr_t{0x1000});
    QueueHandle_t queue_handle =
        reinterpret_cast<QueueHandle_t>(uintptr_t{0x2000});
    TaskHandle_t task_handle =
        reinterpret_cast<TaskHandle_t>(uintptr_t{0x3000});

    /** \brief Keep the bootstrap context privileged and unmasked by default. */
    void SetUp() override {
        kernel = &calls;
        cpu = &context;
        set_context(false, false, true);
    }

    /** \brief Avoid retaining fixture pointers after each complete execution.
     */
    void TearDown() override {
        kernel = nullptr;
        cpu = nullptr;
    }

    /** \brief Permit read-only context checks with no kernel assumptions. */
    void set_context(bool isr, bool masked, bool privileged) {
        ::testing::Mock::VerifyAndClearExpectations(&context);
        EXPECT_CALL(context, in_isr())
            .Times(AnyNumber())
            .WillRepeatedly(Return(isr));
        EXPECT_CALL(context, masked())
            .Times(AnyNumber())
            .WillRepeatedly(Return(masked));
        EXPECT_CALL(context, privileged())
            .Times(AnyNumber())
            .WillRepeatedly(Return(privileged));
        EXPECT_CALL(context, exception())
            .Times(AnyNumber())
            .WillRepeatedly(Return(isr ? 3U : 0U));
        const nx_arch_irq_masks_t masks = {masked ? 1U : 0U, 0, 0};
        EXPECT_CALL(context, masks())
            .Times(AnyNumber())
            .WillRepeatedly(Return(masks));
    }

    /** \brief Entry remains caller-owned; mocked creation never executes it. */
    static void entry(void*) {
    }
};

/** \brief Exercise every task operation under each prohibited CPU context. */
class FreeRTOSRejectedContextTest
    : public FreeRTOSContextTest,
      public ::testing::WithParamInterface<std::tuple<Context, Operation>> {};

TEST_P(FreeRTOSRejectedContextTest, RejectsBeforeKernelOrStorageEffects) {
    const auto [condition, operation] = GetParam();
    set_context(condition == Context::Isr, condition == Context::Masked,
                condition != Context::Unprivileged);
    notification.handle = latch;
    notification.sequence = 42;
    task.handle = task_handle;
    task.finished = latch;
    queue.handle = queue_handle;
    if (operation == Operation::NotifyInit) {
        notification.handle = nullptr;
    }
    if (operation == Operation::TaskStart) {
        task.handle = nullptr;
        task.finished = nullptr;
    }
    if (operation == Operation::QueueInit) {
        queue.handle = nullptr;
    }
    uint8_t notification_before[sizeof notification];
    uint8_t task_before[sizeof task];
    uint8_t queue_before[sizeof queue];
    std::memcpy(notification_before, &notification, sizeof notification);
    std::memcpy(task_before, &task, sizeof task);
    std::memcpy(queue_before, &queue, sizeof queue);
    const auto port = nx_freertos_notify_port(&notification);
    nx_result_t result = NX_ERROR_INVALID;
    switch (operation) {
        case Operation::NotifyInit:
            result = nx_freertos_notify_init(&notification);
            break;
        case Operation::NotifyDestroy:
            result = nx_freertos_notify_destroy(&notification);
            break;
        case Operation::TaskStart:
            result = nx_freertos_task_start(&task, "test", stack, 16, 1, entry,
                                            &item);
            break;
        case Operation::TaskJoin:
            result = nx_freertos_task_join(&task, 1000);
            break;
        case Operation::QueueInit:
            result = nx_freertos_queue_init(&queue, bytes, sizeof bytes, 2,
                                            sizeof item);
            break;
        case Operation::QueueSend:
            result = nx_freertos_queue_send(&queue, &item, 0);
            break;
        case Operation::QueueReceive:
            result = nx_freertos_queue_receive(&queue, &item, 0);
            break;
        case Operation::QueueDestroy:
            result = nx_freertos_queue_destroy(&queue);
            break;
        case Operation::NotifyWait:
            result = port.wait(port.context, 42, 1000);
            break;
        case Operation::NotifyWake:
            result = port.wake(port.context);
            break;
    }
    EXPECT_EQ(result, NX_ERROR_CONTEXT);
    EXPECT_EQ(
        std::memcmp(notification_before, &notification, sizeof notification),
        0);
    EXPECT_EQ(std::memcmp(task_before, &task, sizeof task), 0);
    EXPECT_EQ(std::memcmp(queue_before, &queue, sizeof queue), 0);
    EXPECT_EQ(item, 23U);
}

INSTANTIATE_TEST_SUITE_P(
    UnsafeCpuContexts, FreeRTOSRejectedContextTest,
    ::testing::Combine(
        ::testing::Values(Context::Masked, Context::Unprivileged, Context::Isr),
        ::testing::Values(Operation::NotifyInit, Operation::NotifyDestroy,
                          Operation::TaskStart, Operation::TaskJoin,
                          Operation::QueueInit, Operation::QueueSend,
                          Operation::QueueReceive, Operation::QueueDestroy,
                          Operation::NotifyWait, Operation::NotifyWake)));

TEST_F(FreeRTOSContextTest, BootstrapCreatesObjectsWithoutSchedulerQueries) {
    EXPECT_CALL(calls, create_queue(1, 0, nullptr, &notification.storage,
                                    queueQUEUE_TYPE_BINARY_SEMAPHORE))
        .WillOnce(Return(latch));
    EXPECT_EQ(nx_freertos_notify_init(&notification), NX_SUCCESS);
    EXPECT_CALL(calls, create_queue(1, 0, nullptr, &task.finished_storage,
                                    queueQUEUE_TYPE_BINARY_SEMAPHORE))
        .WillOnce(Return(latch));
    EXPECT_CALL(calls, create_task(_, ::testing::StrEq("test"), 16, &task, 1,
                                   stack, &task.control))
        .WillOnce(Return(task_handle));
    EXPECT_EQ(nx_freertos_task_start(&task, "test", stack, 16, 1, entry, &item),
              NX_SUCCESS);
    EXPECT_CALL(calls, create_queue(2, sizeof item, bytes, &queue.control,
                                    queueQUEUE_TYPE_BASE))
        .WillOnce(Return(queue_handle));
    EXPECT_EQ(
        nx_freertos_queue_init(&queue, bytes, sizeof bytes, 2, sizeof item),
        NX_SUCCESS);
}

TEST_F(FreeRTOSContextTest, BootstrapNoWaitQueueAndWakeRemainAllowed) {
    queue.handle = queue_handle;
    notification.handle = latch;
    EXPECT_CALL(calls, send(queue_handle, &item, 0, queueSEND_TO_BACK))
        .WillOnce(Return(pdTRUE));
    EXPECT_EQ(nx_freertos_queue_send(&queue, &item, 0), NX_SUCCESS);
    EXPECT_CALL(calls, receive(queue_handle, &item, 0))
        .WillOnce(Return(pdTRUE));
    EXPECT_EQ(nx_freertos_queue_receive(&queue, &item, 0), NX_SUCCESS);
    EXPECT_EQ(queue.users, 0U);
    const auto port = nx_freertos_notify_port(&notification);
    EXPECT_CALL(calls, send(latch, nullptr, 0, queueSEND_TO_BACK))
        .WillOnce(Return(pdFALSE));
    EXPECT_EQ(port.wake(port.context), NX_SUCCESS);
    EXPECT_EQ(notification.sequence, 1U);
}

TEST_F(FreeRTOSContextTest, BootstrapRejectsAllBlockingOperations) {
    queue.handle = queue_handle;
    notification.handle = latch;
    task.handle = task_handle;
    task.finished = latch;
    EXPECT_CALL(calls, scheduler())
        .Times(4)
        .WillRepeatedly(Return(taskSCHEDULER_NOT_STARTED));
    EXPECT_EQ(nx_freertos_queue_send(&queue, &item, 1), NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_freertos_queue_receive(&queue, &item, 1), NX_ERROR_CONTEXT);
    const auto port = nx_freertos_notify_port(&notification);
    EXPECT_EQ(port.wait(port.context, 0, 1000), NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_freertos_task_join(&task, 1000), NX_ERROR_CONTEXT);
    EXPECT_EQ(queue.users, 0U);
    EXPECT_EQ(notification.waiting, 0U);
    EXPECT_EQ(task.handle, task_handle);
}

TEST_F(FreeRTOSContextTest, RunningWaitRetainsSequenceAndReleasesWaiter) {
    notification.handle = latch;
    const auto port = nx_freertos_notify_port(&notification);
    EXPECT_CALL(calls, scheduler()).WillOnce(Return(taskSCHEDULER_RUNNING));
    EXPECT_CALL(calls, now()).WillOnce(Return(500));
    EXPECT_CALL(calls, take(latch, 1)).WillOnce([&](QueueHandle_t, TickType_t) {
        EXPECT_EQ(notification.waiting, 1U);
        notification.sequence = 1;
        return pdTRUE;
    });
    EXPECT_EQ(port.wait(port.context, 0, 1000), NX_SUCCESS);
    EXPECT_EQ(notification.waiting, 0U);
}

TEST_F(FreeRTOSContextTest, SuccessfulJoinDeletesBeforeReclaimPermission) {
    task.handle = task_handle;
    task.finished = latch;
    EXPECT_CALL(calls, scheduler()).WillOnce(Return(taskSCHEDULER_RUNNING));
    EXPECT_CALL(calls, current_task()).WillOnce(Return(nullptr));
    EXPECT_CALL(calls, now()).WillOnce(Return(500));
    EXPECT_CALL(calls, take(latch, 1)).WillOnce(Return(pdTRUE));
    EXPECT_CALL(calls, delete_task(task_handle));
    EXPECT_CALL(calls, delete_queue(latch));
    EXPECT_EQ(nx_freertos_task_join(&task, 1000), NX_SUCCESS);
    EXPECT_EQ(task.handle, nullptr);
    EXPECT_EQ(task.finished, nullptr);
}

TEST_F(FreeRTOSContextTest,
       KernelBootstrapCeilingAllowsFollowingStaticObjects) {
    EXPECT_CALL(calls, create_queue(1, 0, nullptr, &notification.storage,
                                    queueQUEUE_TYPE_BINARY_SEMAPHORE))
        .WillOnce(
            [&](UBaseType_t, UBaseType_t, uint8_t*, StaticQueue_t*, uint8_t) {
                /* The pinned CM4F port keeps its syscall BASEPRI before start.
                 */
                set_context(false, true, true);
                const nx_arch_irq_masks_t masks = {
                    0, configMAX_SYSCALL_INTERRUPT_PRIORITY, 0};
                EXPECT_CALL(context, masks()).WillRepeatedly(Return(masks));
                return latch;
            });
    EXPECT_EQ(nx_freertos_notify_init(&notification), NX_SUCCESS);
    EXPECT_CALL(calls, scheduler())
        .Times(5)
        .WillRepeatedly(Return(taskSCHEDULER_NOT_STARTED));
    EXPECT_CALL(calls, create_queue(1, 0, nullptr, &task.finished_storage,
                                    queueQUEUE_TYPE_BINARY_SEMAPHORE))
        .WillOnce(Return(latch));
    EXPECT_CALL(calls, create_task(_, ::testing::StrEq("test"), 16, &task, 1,
                                   stack, &task.control))
        .WillOnce(Return(task_handle));
    EXPECT_EQ(nx_freertos_task_start(&task, "test", stack, 16, 1, entry, &item),
              NX_SUCCESS);
    EXPECT_CALL(calls, create_queue(2, sizeof item, bytes, &queue.control,
                                    queueQUEUE_TYPE_BASE))
        .WillOnce(Return(queue_handle));
    EXPECT_EQ(
        nx_freertos_queue_init(&queue, bytes, sizeof bytes, 2, sizeof item),
        NX_SUCCESS);
    EXPECT_CALL(calls, send(queue_handle, &item, 0, queueSEND_TO_BACK))
        .WillOnce(Return(pdTRUE));
    EXPECT_EQ(nx_freertos_queue_send(&queue, &item, 0), NX_SUCCESS);
    EXPECT_CALL(calls, receive(queue_handle, &item, 0))
        .WillOnce(Return(pdTRUE));
    EXPECT_EQ(nx_freertos_queue_receive(&queue, &item, 0), NX_SUCCESS);
    const auto port = nx_freertos_notify_port(&notification);
    EXPECT_CALL(calls, send(latch, nullptr, 0, queueSEND_TO_BACK))
        .WillOnce(Return(pdTRUE));
    EXPECT_EQ(port.wake(port.context), NX_SUCCESS);
    EXPECT_EQ(notification.sequence, 1U);
}

TEST_F(FreeRTOSContextTest, RunningAndSuspendedCanonicalMasksRejectMutation) {
    set_context(false, true, true);
    const nx_arch_irq_masks_t masks = {0, configMAX_SYSCALL_INTERRUPT_PRIORITY,
                                       0};
    EXPECT_CALL(context, masks()).WillRepeatedly(Return(masks));
    EXPECT_CALL(calls, scheduler())
        .WillOnce(Return(taskSCHEDULER_RUNNING))
        .WillOnce(Return(taskSCHEDULER_RUNNING))
        .WillOnce(Return(taskSCHEDULER_SUSPENDED))
        .WillOnce(Return(taskSCHEDULER_SUSPENDED));
    queue.handle = queue_handle;
    notification.handle = latch;
    const auto port = nx_freertos_notify_port(&notification);
    for (unsigned i = 0; i < 2; ++i) {
        EXPECT_EQ(nx_freertos_queue_send(&queue, &item, 0), NX_ERROR_CONTEXT);
        EXPECT_EQ(port.wake(port.context), NX_ERROR_CONTEXT);
        EXPECT_EQ(queue.users, 0U);
        EXPECT_EQ(notification.sequence, 0U);
    }
}

TEST_F(FreeRTOSContextTest, NoncanonicalBasepriRejectsWithoutKernelQueries) {
    set_context(false, true, true);
    const nx_arch_irq_masks_t masks = {
        0, configMAX_SYSCALL_INTERRUPT_PRIORITY + 0x10U, 0};
    EXPECT_CALL(context, masks()).WillRepeatedly(Return(masks));
    queue.handle = queue_handle;
    EXPECT_EQ(nx_freertos_queue_send(&queue, &item, 0), NX_ERROR_CONTEXT);
    EXPECT_EQ(queue.users, 0U);
}

TEST_F(FreeRTOSContextTest, AllowedIrqPublishesBeforeGivingAndYielding) {
    set_context(true, false, true);
    EXPECT_CALL(context, exception()).WillRepeatedly(Return(16U));
    EXPECT_CALL(context, priority_group()).WillOnce(Return(3));
    EXPECT_CALL(context, irq_priority(16)).WillOnce(Return(0x50));
    notification.handle = latch;
    const auto port = nx_freertos_notify_port(&notification);
    EXPECT_CALL(calls, give_isr(latch, _))
        .WillOnce([&](QueueHandle_t, BaseType_t* required) {
            EXPECT_EQ(notification.sequence, 1U);
            *required = pdTRUE;
            return pdTRUE;
        });
    EXPECT_CALL(calls, yield());
    EXPECT_EQ(port.wake(port.context), NX_SUCCESS);
}

TEST_F(FreeRTOSContextTest, UnsafeGroupingRejectsBeforePriorityOrKernelReads) {
    set_context(true, false, true);
    EXPECT_CALL(context, exception()).WillRepeatedly(Return(16U));
    EXPECT_CALL(context, priority_group()).WillOnce(Return(4));
    notification.handle = latch;
    const auto port = nx_freertos_notify_port(&notification);
    EXPECT_EQ(port.wake(port.context), NX_ERROR_CONTEXT);
    EXPECT_EQ(notification.sequence, 0U);
}

TEST_F(FreeRTOSContextTest, AboveCeilingIrqRejectsBeforeSequencePublication) {
    set_context(true, false, true);
    EXPECT_CALL(context, exception()).WillRepeatedly(Return(16U));
    EXPECT_CALL(context, priority_group()).WillOnce(Return(3));
    EXPECT_CALL(context, irq_priority(16)).WillOnce(Return(0x40));
    notification.handle = latch;
    const auto port = nx_freertos_notify_port(&notification);
    EXPECT_EQ(port.wake(port.context), NX_ERROR_CONTEXT);
    EXPECT_EQ(notification.sequence, 0U);
}

TEST_F(FreeRTOSContextTest, SystemAndOutOfRangeExceptionsNeverReadNvic) {
    set_context(true, false, true);
    EXPECT_CALL(context, exception())
        .WillOnce(Return(0U))
        .WillOnce(Return(2U))
        .WillOnce(Return(3U))
        .WillOnce(Return(11U))
        .WillOnce(Return(14U))
        .WillOnce(Return(15U))
        .WillOnce(Return(256U));
    notification.handle = latch;
    const auto port = nx_freertos_notify_port(&notification);
    for (unsigned i = 0; i < 7; ++i) {
        EXPECT_EQ(port.wake(port.context), NX_ERROR_CONTEXT);
        EXPECT_EQ(notification.sequence, 0U);
    }
}

TEST_F(FreeRTOSContextTest, MaskedIrqNeverReadsNvicOrPublishesSequence) {
    set_context(true, true, true);
    notification.handle = latch;
    const auto port = nx_freertos_notify_port(&notification);
    EXPECT_EQ(port.wake(port.context), NX_ERROR_CONTEXT);
    EXPECT_EQ(notification.sequence, 0U);
}
} /* namespace */

extern "C" {
/** \brief Forward architectural context snapshots to the injected boundary. */
bool nx_arch_in_isr(void) {
    return cpu->in_isr();
}

/** \brief Preserve the injected incoming mask without modifying it. */
bool nx_arch_irq_is_masked(void) {
    return cpu->masked();
}

/** \brief Observe privilege independently of kernel state. */
bool nx_arch_is_privileged(void) {
    return cpu->privileged();
}

/** \brief Supply exception identity without replacing adapter logic. */
uint32_t nx_arch_exception_number(void) {
    return cpu->exception();
}

/** \brief Observe individual masks without restoring or modifying them. */
nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return cpu->masks();
}

/** \brief Supply the priority-grouping field at the external MMIO boundary. */
uint32_t nx_freertos_test_priority_group(void) {
    return cpu->priority_group();
}

/** \brief Supply only the selected external exception's priority register. */
uint8_t nx_freertos_test_irq_priority(uint32_t exception) {
    return cpu->irq_priority(exception);
}

/** \brief Pin external queue creation calls to real kernel declarations. */
QueueHandle_t xQueueGenericCreateStatic(UBaseType_t depth, UBaseType_t size,
                                        uint8_t* storage,
                                        StaticQueue_t* control, uint8_t type) {
    return kernel->create_queue(depth, size, storage, control, type);
}

/** \brief Pin external task creation calls to real kernel declarations. */
TaskHandle_t xTaskCreateStatic(TaskFunction_t entry, const char* name,
                               configSTACK_DEPTH_TYPE depth, void* argument,
                               UBaseType_t priority, StackType_t* stack,
                               StaticTask_t* control) {
    return kernel->create_task(entry, name, depth, argument, priority, stack,
                               control);
}

/** \brief Supply scheduler state without running a fake scheduler. */
BaseType_t xTaskGetSchedulerState(void) {
    return kernel->scheduler();
}

/** \brief Supply external task identity for self-join rejection. */
TaskHandle_t xTaskGetCurrentTaskHandle(void) {
    return kernel->current_task();
}

/** \brief Observe kernel queue and semaphore sends. */
BaseType_t xQueueGenericSend(QueueHandle_t handle, const void* item,
                             TickType_t ticks, BaseType_t position) {
    return kernel->send(handle, item, ticks, position);
}

/** \brief Observe the kernel receive boundary. */
BaseType_t xQueueReceive(QueueHandle_t handle, void* item, TickType_t ticks) {
    return kernel->receive(handle, item, ticks);
}

/** \brief Observe binary-latch waits at the real kernel boundary. */
BaseType_t xQueueSemaphoreTake(QueueHandle_t handle, TickType_t ticks) {
    return kernel->take(handle, ticks);
}

/** \brief Observe IRQ wake operations without simulating physical NVIC. */
BaseType_t xQueueGiveFromISR(QueueHandle_t handle, BaseType_t* required) {
    return kernel->give_isr(handle, required);
}

/** \brief Observe queue drain checks. */
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t handle) {
    return kernel->messages(handle);
}

/** \brief Observe caller-owned kernel object deletion. */
void vQueueDelete(QueueHandle_t handle) {
    kernel->delete_queue(handle);
}

/** \brief Observe task deletion before storage is reclaimed. */
void vTaskDelete(TaskHandle_t handle) {
    kernel->delete_task(handle);
}

/** \brief Task wrappers are not scheduled by this mocked kernel boundary. */
void vTaskSuspend(TaskHandle_t handle) {
    kernel->suspend(handle);
}

/** \brief Observe an explicit ISR scheduler request. */
void vPortYield(void) {
    kernel->yield();
}

/** \brief Supply only the external absolute deadline clock. */
nx_time_us_t nx_time_now_us(void) {
    return kernel->now();
}
} /* extern "C" */
