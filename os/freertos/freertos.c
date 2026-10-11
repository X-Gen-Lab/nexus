/**
 * \file            freertos.c
 * \brief           Thin static-object FreeRTOS adapter
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
/* The adapter is a privileged kernel caller, never an MPU user gateway. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "nexus/os/freertos.h"
#include "nexus/arch/arch.h"
#include "nexus/arch/atomic.h"
#include <limits.h>
#include <string.h>

/* Native kernel fixtures have no hardware CPU facts; their context model is
 * explicit in the test target. MCU consumers must supply validated facts. */
#ifndef NEXUS_CPU_HAS_BASEPRI
#if defined(__arm__) || defined(__thumb__)
#error "FreeRTOS requires validated CPU interrupt-mask facts"
#else
#define NEXUS_CPU_HAS_BASEPRI 1
#endif
#endif
#ifndef NEXUS_CPU_EXTERNAL_IRQ_COUNT
#if defined(__arm__) || defined(__thumb__)
#error "FreeRTOS requires validated external IRQ bounds"
#else
#define NEXUS_CPU_EXTERNAL_IRQ_COUNT 240
#endif
#endif

/* Each maintained MCU port publishes its reviewed startup frame capacity.
 * The host fixture has its own explicit storage contract. */
#ifndef NEXUS_OS_MIN_STACK_WORDS
#if defined(__arm__) || defined(__thumb__)
#error "FreeRTOS requires a reviewed initial stack capacity"
#else
#define NEXUS_OS_MIN_STACK_WORDS 18U
#endif
#endif
#ifndef NEXUS_OS_STACK_ALIGNMENT_BYTES
#define NEXUS_OS_STACK_ALIGNMENT_BYTES portBYTE_ALIGNMENT
#endif

#if defined(__arm__) || defined(__thumb__) || defined(NEXUS_FREERTOS_MODEL)
#ifndef NX_FREERTOS_PRIORITY_GROUP
#define NX_FREERTOS_PRIORITY_GROUP()                                           \
    ((*(const volatile uint32_t*)0xe000ed0cu >> 8) & 7u)
#endif
#ifndef NX_FREERTOS_IRQ_PRIORITY
#define NX_FREERTOS_IRQ_PRIORITY(exception)                                    \
    (((const volatile uint8_t*)0xe000e400u)[(exception)-16])
#endif
#endif

/** \brief Reject unsafe CPU contexts before kernel entry or object mutation. */
static bool task_context_allowed(void) {
    if (nx_arch_in_isr() || !nx_arch_is_privileged()) {
        return false;
    }
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (masks.faultmask != 0) {
        return false;
    }
#if NEXUS_CPU_HAS_BASEPRI
    if (masks.primask != 0) {
        return false;
    }
    if (masks.basepri == 0) {
        return true;
    }
    /* Mainline ports keep their syscall mask while their pre-scheduler
     * critical nesting is nonzero. Accept that exact value only before start;
     * running or suspended tasks must restore every incoming manual mask. */
    return masks.basepri == configMAX_SYSCALL_INTERRUPT_PRIORITY &&
           xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED;
#else
    if (masks.basepri != 0 || masks.primask > 1) {
        return false;
    }
    /* Baseline ports retain PRIMASK, not BASEPRI, until scheduler start. The
     * value grants bootstrap permission; it cannot establish mask provenance.
     */
    return masks.primask == 0 ||
           xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED;
#endif
}

/** \brief Reject kernel API calls above the maintained syscall ceiling. */
bool nx_freertos_isr_allowed(void) {
    if (!nx_arch_is_privileged() || nx_arch_irq_is_masked()) {
        return false;
    }
#if defined(__arm__) || defined(__thumb__) || defined(NEXUS_FREERTOS_MODEL)
    uint32_t exception = nx_arch_exception_number();
    if (exception < 16 || exception - 16 >= NEXUS_CPU_EXTERNAL_IRQ_COUNT) {
        return false;
    }
#if NEXUS_CPU_HAS_BASEPRI
    uint32_t grouping = NX_FREERTOS_PRIORITY_GROUP();
    uint32_t maximum_grouping =
        configPRIO_BITS >= 7 ? 0u : 7u - configPRIO_BITS;
    return grouping <= maximum_grouping &&
           NX_FREERTOS_IRQ_PRIORITY(exception) >=
               configMAX_SYSCALL_INTERRUPT_PRIORITY;
#else
    /* PRIMASK excludes every configurable exception; Baseline kernels do not
     * own a BASEPRI syscall ceiling or a Mainline priority-group contract. */
    return true;
#endif
#else
    return false;
#endif
}

/** \brief Round a finite remaining deadline up to ticks without overflow. */
static TickType_t deadline_ticks(nx_time_us_t deadline) {
    nx_time_us_t now = nx_time_now_us();
    if (deadline != NX_DEADLINE_NEVER && now >= deadline) {
        return 0;
    }
    uint64_t remaining =
        deadline == NX_DEADLINE_NEVER ? UINT64_MAX : deadline - now;
    uint64_t seconds = remaining / 1000000u;
    uint64_t maximum = (uint64_t)portMAX_DELAY - 1u;
    if (seconds > maximum / configTICK_RATE_HZ) {
        return (TickType_t)maximum;
    }
    uint64_t ticks = seconds * configTICK_RATE_HZ;
    ticks += ((remaining % 1000000u) * configTICK_RATE_HZ + 999999u) / 1000000u;
    return (TickType_t)(ticks > maximum ? maximum : ticks);
}

/** \brief Snapshot the lock-free wake sequence. */
static uint32_t notify_arm(void* context) {
    nx_freertos_notify_t* notification = context;
    return nx_atomic_u32_load_acquire(&notification->sequence);
}

/** \brief Consume a binary latch while retaining sequence as the predicate. */
static nx_result_t notify_wait(void* context, uint32_t sequence,
                               uint64_t deadline_us) {
    nx_freertos_notify_t* notification = context;
    if (!task_context_allowed() ||
        xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t expected = 0;
    if (!nx_atomic_u32_compare_exchange_acq_rel(&notification->waiting,
                                                &expected, 1)) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = NX_SUCCESS;
    for (;;) {
        TickType_t ticks = deadline_ticks(deadline_us);
        if (ticks == 0) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        if (notify_arm(context) != sequence) {
            break;
        }
        (void)xSemaphoreTake(notification->handle, ticks);
    }
    nx_atomic_u32_store_release(&notification->waiting, 0);
    return result;
}

/** \brief Publish sequence then latch wake; a full binary latch is valid. */
static nx_result_t notify_wake(void* context) {
    nx_freertos_notify_t* notification = context;
    bool isr = nx_arch_in_isr();
    if ((isr && !nx_freertos_isr_allowed()) ||
        (!isr && !task_context_allowed())) {
        return NX_ERROR_CONTEXT;
    }
    (void)nx_atomic_u32_fetch_add_release(&notification->sequence, 1);
    if (isr) {
        BaseType_t switch_required = pdFALSE;
        (void)xSemaphoreGiveFromISR(notification->handle, &switch_required);
        portYIELD_FROM_ISR(switch_required);
    } else {
        (void)xSemaphoreGive(notification->handle);
    }
    return NX_SUCCESS;
}

/** \brief Create exactly one static notification latch. */
nx_result_t nx_freertos_notify_init(nx_freertos_notify_t* notification) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (notification == NULL || notification->handle != NULL) {
        return NX_ERROR_INVALID;
    }
    notification->sequence = 0;
    notification->waiting = 0;
    notification->handle = xSemaphoreCreateBinaryStatic(&notification->storage);
    return notification->handle != NULL ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief Bind optional wait without forcing drivers to depend on a kernel. */
nx_wait_port_t nx_freertos_notify_port(nx_freertos_notify_t* notification) {
    if (notification == NULL || notification->handle == NULL) {
        nx_wait_port_t invalid = {0};
        return invalid;
    }
    nx_wait_port_t port = {notification, notify_arm, notify_wait, notify_wake};
    return port;
}

/** \brief Require publisher quiescence before deleting the static latch. */
nx_result_t nx_freertos_notify_destroy(nx_freertos_notify_t* notification) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (notification == NULL || notification->handle == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_atomic_u32_load_acquire(&notification->waiting) != 0) {
        return NX_ERROR_BUSY;
    }
    vSemaphoreDelete(notification->handle);
    notification->handle = NULL;
    return NX_SUCCESS;
}

/** \brief Return completion, then park until the external joiner deletes us. */
static void task_entry(void* argument) {
    nx_freertos_task_t* task = argument;
    void (*entry)(void*) = task->entry;
    void* context = task->context;
    SemaphoreHandle_t finished = task->finished;
    entry(context);
    /* No task/control/context access after publication. Join deletes externally
     * so static TCB/stack are not waiting for self-delete idle reclamation. */
    (void)xSemaphoreGive(finished);
    for (;;) {
        vTaskSuspend(NULL);
    }
}

/** \brief Validate capacity before the kernel writes its initial frame. */
static bool task_storage_valid(const char* name, const StackType_t* stack,
                               size_t stack_words, UBaseType_t priority,
                               void (*entry)(void*)) {
    if (name == NULL || name[0] == '\0' || stack == NULL || entry == NULL ||
        stack_words < NEXUS_OS_MIN_STACK_WORDS || stack_words > UINT32_MAX ||
        (size_t)(configSTACK_DEPTH_TYPE)stack_words != stack_words ||
        stack_words > SIZE_MAX / sizeof(*stack) ||
        stack_words > (UINTPTR_MAX - (uintptr_t)stack) / sizeof(*stack) ||
        (uintptr_t)stack % NEXUS_OS_STACK_ALIGNMENT_BYTES != 0 ||
        priority >= configMAX_PRIORITIES) {
        return false;
    }
    return true;
}

/** \brief Create only caller-selected static task resources. */
nx_result_t nx_freertos_task_start(nx_freertos_task_t* task, const char* name,
                                   StackType_t* stack, size_t stack_words,
                                   UBaseType_t priority, void (*entry)(void*),
                                   void* context) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (task == NULL || (uintptr_t)task % _Alignof(nx_freertos_task_t) != 0 ||
        !task_storage_valid(name, stack, stack_words, priority, entry) ||
        task->handle != NULL || task->finished != NULL) {
        return NX_ERROR_INVALID;
    }
    task->finished = xSemaphoreCreateBinaryStatic(&task->finished_storage);
    if (task->finished == NULL) {
        return NX_ERROR_IO;
    }
    task->entry = entry;
    task->context = context;
    task->handle = xTaskCreateStatic(
        task_entry, name, (configSTACK_DEPTH_TYPE)stack_words, task,
        priority | portPRIVILEGE_BIT, stack, &task->control);
    if (task->handle == NULL) {
        vSemaphoreDelete(task->finished);
        task->finished = NULL;
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

/** \brief Delete a returned other task before returning reclaim permission. */
nx_result_t nx_freertos_task_join(nx_freertos_task_t* task,
                                  nx_time_us_t deadline) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (task == NULL || task->handle == NULL) {
        return NX_ERROR_INVALID;
    }
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return NX_ERROR_CONTEXT;
    }
    if (task->handle == xTaskGetCurrentTaskHandle()) {
        return NX_ERROR_CONTEXT;
    }
    for (;;) {
        TickType_t ticks = deadline_ticks(deadline);
        if (xSemaphoreTake(task->finished, ticks) == pdTRUE) {
            vTaskDelete(task->handle);
            task->handle = NULL;
            vSemaphoreDelete(task->finished);
            task->finished = NULL;
            return NX_SUCCESS;
        }
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            return NX_ERROR_TIMEOUT;
        }
    }
}

/** \brief Create queue storage at the actual configured depth and item size. */
nx_result_t nx_freertos_queue_init(nx_freertos_queue_t* queue, uint8_t* storage,
                                   size_t storage_bytes, size_t depth,
                                   size_t item_size) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (queue == NULL || storage == NULL || depth == 0 || item_size == 0 ||
        depth > storage_bytes / item_size || depth > UINT32_MAX ||
        item_size > UINT32_MAX || queue->handle != NULL) {
        return NX_ERROR_INVALID;
    }
    queue->users = 0;
    queue->handle = xQueueCreateStatic(
        (UBaseType_t)depth, (UBaseType_t)item_size, storage, &queue->control);
    return queue->handle != NULL ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief Send bounded queue data; explicit owner controls stop admission. */
nx_result_t nx_freertos_queue_send(nx_freertos_queue_t* queue, const void* item,
                                   TickType_t wait_ticks) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (queue == NULL || queue->handle == NULL || item == NULL ||
        wait_ticks == portMAX_DELAY) {
        return NX_ERROR_INVALID;
    }
    if (wait_ticks != 0 && xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return NX_ERROR_CONTEXT;
    }
    (void)nx_atomic_u32_fetch_add_acq_rel(&queue->users, 1);
    BaseType_t result = xQueueSend(queue->handle, item, wait_ticks);
    (void)nx_atomic_u32_fetch_sub_release(&queue->users, 1);
    return result == pdTRUE ? NX_SUCCESS : NX_ERROR_TIMEOUT;
}

/** \brief Receive bounded queue data. */
nx_result_t nx_freertos_queue_receive(nx_freertos_queue_t* queue, void* item,
                                      TickType_t wait_ticks) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (queue == NULL || queue->handle == NULL || item == NULL ||
        wait_ticks == portMAX_DELAY) {
        return NX_ERROR_INVALID;
    }
    if (wait_ticks != 0 && xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return NX_ERROR_CONTEXT;
    }
    (void)nx_atomic_u32_fetch_add_acq_rel(&queue->users, 1);
    BaseType_t result = xQueueReceive(queue->handle, item, wait_ticks);
    (void)nx_atomic_u32_fetch_sub_release(&queue->users, 1);
    return result == pdTRUE ? NX_SUCCESS : NX_ERROR_TIMEOUT;
}

/** \brief Delete only after external users quiesce and queued items drain. */
nx_result_t nx_freertos_queue_destroy(nx_freertos_queue_t* queue) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (queue == NULL || queue->handle == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_atomic_u32_load_acquire(&queue->users) != 0 ||
        uxQueueMessagesWaiting(queue->handle) != 0) {
        return NX_ERROR_BUSY;
    }
    vQueueDelete(queue->handle);
    queue->handle = NULL;
    return NX_SUCCESS;
}

/** \brief Reuse a single absolute budget across bounded kernel waits. */
static nx_result_t queue_until(nx_freertos_queue_t* queue, void* item,
                               nx_time_us_t deadline, bool send) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (queue == NULL || queue->handle == NULL || item == NULL) {
        return NX_ERROR_INVALID;
    }
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return NX_ERROR_CONTEXT;
    }
    (void)nx_atomic_u32_fetch_add_acq_rel(&queue->users, 1);
    TickType_t ticks = 0;
    nx_result_t result = NX_SUCCESS;
    for (;;) {
        BaseType_t ready = send ? xQueueSend(queue->handle, item, ticks)
                                : xQueueReceive(queue->handle, item, ticks);
        if (ready == pdTRUE) {
            break;
        }
        ticks = deadline_ticks(deadline);
        if (ticks == 0) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
    }
    (void)nx_atomic_u32_fetch_sub_release(&queue->users, 1);
    return result;
}

/** \brief Keep send payload borrowed only for this bounded call. */
nx_result_t nx_freertos_queue_send_until(nx_freertos_queue_t* queue,
                                         const void* item,
                                         nx_time_us_t deadline) {
    return queue_until(queue, (void*)item, deadline, true);
}

/** \brief Copy receive payload only when the kernel reports readiness. */
nx_result_t nx_freertos_queue_receive_until(nx_freertos_queue_t* queue,
                                            void* item, nx_time_us_t deadline) {
    return queue_until(queue, item, deadline, false);
}

#if configUSE_TASK_NOTIFICATIONS == 1
/** \brief Snapshot only this adapter's predicate-hint sequence. */
static uint32_t direct_arm(void* context) {
    nx_freertos_direct_notify_t* notification = context;
    return nx_atomic_u32_load_acquire(&notification->sequence);
}

/** \brief Wait only in the explicitly bound receiver's exclusive slot. */
static nx_result_t direct_wait(void* context, uint32_t sequence,
                               uint64_t deadline) {
    nx_freertos_direct_notify_t* notification = context;
    if (!task_context_allowed() ||
        xTaskGetSchedulerState() != taskSCHEDULER_RUNNING ||
        xTaskGetCurrentTaskHandle() != notification->receiver) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t expected = 0;
    if (!nx_atomic_u32_compare_exchange_acq_rel(&notification->waiting,
                                                &expected, 1)) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = NX_SUCCESS;
    for (;;) {
        TickType_t ticks = deadline_ticks(deadline);
        if (ticks == 0) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        if (direct_arm(context) != sequence) {
            break;
        }
        (void)ulTaskNotifyTakeIndexed(notification->index, pdTRUE, ticks);
    }
    nx_atomic_u32_store_release(&notification->waiting, 0);
    return result;
}

/** \brief Release-publish a hint before notifying the bound receiver. */
static nx_result_t direct_wake(void* context) {
    nx_freertos_direct_notify_t* notification = context;
    bool isr = nx_arch_in_isr();
    if ((isr && !nx_freertos_isr_allowed()) ||
        (!isr && !task_context_allowed())) {
        return NX_ERROR_CONTEXT;
    }
    (void)nx_atomic_u32_fetch_add_release(&notification->sequence, 1);
    if (isr) {
        BaseType_t switch_required = pdFALSE;
        (void)xTaskNotifyIndexedFromISR(notification->receiver,
                                        notification->index, 0, eIncrement,
                                        &switch_required);
        portYIELD_FROM_ISR(switch_required);
    } else {
        (void)xTaskNotifyIndexed(notification->receiver, notification->index, 0,
                                 eIncrement);
    }
    return NX_SUCCESS;
}
#endif

/** \brief Bind explicit receiver storage without allocating a semaphore. */
nx_result_t
nx_freertos_direct_notify_init(nx_freertos_direct_notify_t* notification,
                               TaskHandle_t receiver, UBaseType_t index) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
#if configUSE_TASK_NOTIFICATIONS == 1
    if (notification == NULL || receiver == NULL ||
        notification->receiver != NULL ||
        index >= configTASK_NOTIFICATION_ARRAY_ENTRIES) {
        return NX_ERROR_INVALID;
    }
    notification->receiver = receiver;
    notification->index = index;
    notification->sequence = 0;
    notification->waiting = 0;
    return NX_SUCCESS;
#else
    (void)notification;
    (void)receiver;
    (void)index;
    return NX_ERROR_UNSUPPORTED;
#endif
}

/** \brief Unsupported profiles expose an invalid port, never a kernel call. */
nx_wait_port_t
nx_freertos_direct_notify_port(nx_freertos_direct_notify_t* notification) {
#if configUSE_TASK_NOTIFICATIONS == 1
    if (notification != NULL && notification->receiver != NULL) {
        nx_wait_port_t port = {notification, direct_arm, direct_wait,
                               direct_wake};
        return port;
    }
#else
    (void)notification;
#endif
    nx_wait_port_t invalid = {0};
    return invalid;
}

/** \brief Quiescence is external; the active-wait check is only a guard. */
nx_result_t
nx_freertos_direct_notify_destroy(nx_freertos_direct_notify_t* notification) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
#if configUSE_TASK_NOTIFICATIONS == 1
    if (notification == NULL || notification->receiver == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_atomic_u32_load_acquire(&notification->waiting) != 0) {
        return NX_ERROR_BUSY;
    }
    notification->receiver = NULL;
    return NX_SUCCESS;
#else
    (void)notification;
    return NX_ERROR_UNSUPPORTED;
#endif
}

/** \brief Create an explicit permanent task without any join-only storage. */
nx_result_t
nx_freertos_permanent_task_start(nx_freertos_permanent_task_t* task,
                                 const char* name, StackType_t* stack,
                                 size_t stack_words, UBaseType_t priority,
                                 void (*entry)(void*), void* context) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (task == NULL ||
        (uintptr_t)task % _Alignof(nx_freertos_permanent_task_t) != 0 ||
        !task_storage_valid(name, stack, stack_words, priority, entry) ||
        task->handle != NULL) {
        return NX_ERROR_INVALID;
    }
    task->handle = xTaskCreateStatic(
        entry, name, (configSTACK_DEPTH_TYPE)stack_words, context,
        priority | portPRIVILEGE_BIT, stack, &task->control);
    return task->handle != NULL ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief Create one caller-owned binary latch, not a universal waiter pool. */
nx_result_t nx_freertos_queue_waiter_init(nx_freertos_queue_waiter_t* waiter) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (waiter == NULL || waiter->handle != NULL) {
        return NX_ERROR_INVALID;
    }
    waiter->active = 0;
    waiter->next = NULL;
    waiter->handle = xSemaphoreCreateBinaryStatic(&waiter->storage);
    return waiter->handle != NULL ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief Reject reclamation while the waiter remains linked or borrowed. */
nx_result_t
nx_freertos_queue_waiter_destroy(nx_freertos_queue_waiter_t* waiter) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (waiter == NULL || waiter->handle == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_atomic_u32_load_acquire(&waiter->active) != 0) {
        return NX_ERROR_BUSY;
    }
    vSemaphoreDelete(waiter->handle);
    waiter->handle = NULL;
    return NX_SUCCESS;
}

/** \brief Couple an exact raw queue with explicitly budgeted waiters. */
nx_result_t nx_freertos_closable_queue_init(nx_freertos_closable_queue_t* queue,
                                            uint8_t* storage,
                                            size_t storage_bytes, size_t depth,
                                            size_t item_size,
                                            size_t maximum_waiters) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (queue == NULL || maximum_waiters == 0 || maximum_waiters > UINT32_MAX) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_freertos_queue_init(
        &queue->queue, storage, storage_bytes, depth, item_size);
    if (result == NX_SUCCESS) {
        queue->send_waiters = NULL;
        queue->receive_waiters = NULL;
        queue->maximum_waiters = (uint32_t)maximum_waiters;
        queue->closed = false;
    }
    return result;
}

/** \brief Call under the task scheduling guard; registration is bounded. */
static void queue_broadcast(nx_freertos_queue_waiter_t* waiter) {
    for (; waiter != NULL; waiter = waiter->next) {
        /* A full binary latch still covers wake-before-sleep. */
        (void)xSemaphoreGive(waiter->handle);
    }
}

/** \brief Unlink before releasing active storage; close holds the same guard.
 */
static void queue_unregister(nx_freertos_queue_waiter_t** head,
                             nx_freertos_queue_waiter_t* waiter) {
    while (*head != NULL) {
        if (*head == waiter) {
            *head = waiter->next;
            waiter->next = NULL;
            break;
        }
        head = &(*head)->next;
    }
}

/** \brief Serialize task-only state under a nested scheduler-suspension guard.
 */
static nx_result_t closable_transfer(nx_freertos_closable_queue_t* queue,
                                     void* item, nx_time_us_t deadline,
                                     nx_freertos_queue_waiter_t* waiter,
                                     bool send) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (queue == NULL || queue->queue.handle == NULL || item == NULL ||
        waiter == NULL || waiter->handle == NULL ||
        queue->maximum_waiters == 0) {
        return NX_ERROR_INVALID;
    }
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t expected = 0;
    if (!nx_atomic_u32_compare_exchange_acq_rel(&waiter->active, &expected,
                                                1)) {
        return NX_ERROR_BUSY;
    }
    (void)xSemaphoreTake(waiter->handle, 0);
    /* Single-core task-only registry: suspension prevents a give from
     * switching to an awakened task before broadcasting/unlinking finishes.
     * Kernel queue calls own their individual short interrupt guards. */
    nx_freertos_queue_waiter_t** head =
        send ? &queue->send_waiters : &queue->receive_waiters;
    bool registered = false;
    bool expired = false;
    nx_result_t result = NX_ERROR_BUSY;
    vTaskSuspendAll();
    if (queue->queue.users >= queue->maximum_waiters) {
        (void)xTaskResumeAll();
        nx_atomic_u32_store_release(&waiter->active, 0);
        return NX_ERROR_BUSY;
    }
    ++queue->queue.users;
    for (;;) {
        if (send && queue->closed) {
            result = NX_ERROR_CANCELLED;
            break;
        }
        BaseType_t ready = send ? xQueueSend(queue->queue.handle, item, 0)
                                : xQueueReceive(queue->queue.handle, item, 0);
        if (ready == pdTRUE) {
            queue_broadcast(send ? queue->receive_waiters
                                 : queue->send_waiters);
            result = NX_SUCCESS;
            break;
        }
        if (queue->closed) {
            result = NX_ERROR_CANCELLED;
            break;
        }
        if (expired) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        if (!registered) {
            waiter->next = *head;
            *head = waiter;
            registered = true;
        }
        (void)xTaskResumeAll();
        TickType_t ticks = deadline_ticks(deadline);
        expired = ticks == 0;
        if (!expired) {
            (void)xSemaphoreTake(waiter->handle, ticks);
        }
        vTaskSuspendAll();
    }
    if (registered) {
        queue_unregister(head, waiter);
    }
    --queue->queue.users;
    (void)xTaskResumeAll();
    nx_atomic_u32_store_release(&waiter->active, 0);
    return result;
}

/** \brief Every send shares admission serialization with close. */
nx_result_t
nx_freertos_closable_queue_send_until(nx_freertos_closable_queue_t* queue,
                                      const void* item, nx_time_us_t deadline,
                                      nx_freertos_queue_waiter_t* waiter) {
    return closable_transfer(queue, (void*)item, deadline, waiter, true);
}

/** \brief Closed queues retain admitted items until consumers drain them. */
nx_result_t
nx_freertos_closable_queue_receive_until(nx_freertos_closable_queue_t* queue,
                                         void* item, nx_time_us_t deadline,
                                         nx_freertos_queue_waiter_t* waiter) {
    return closable_transfer(queue, item, deadline, waiter, false);
}

/** \brief Suspend task switching while close latches every registered wake. */
nx_result_t
nx_freertos_closable_queue_close(nx_freertos_closable_queue_t* queue) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (queue == NULL || queue->queue.handle == NULL) {
        return NX_ERROR_INVALID;
    }
    vTaskSuspendAll();
    queue->closed = true;
    queue_broadcast(queue->send_waiters);
    queue_broadcast(queue->receive_waiters);
    (void)xTaskResumeAll();
    return NX_SUCCESS;
}

/** \brief Quiescence, close and drain are all required before deletion. */
nx_result_t
nx_freertos_closable_queue_destroy(nx_freertos_closable_queue_t* queue) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (queue == NULL || queue->queue.handle == NULL) {
        return NX_ERROR_INVALID;
    }
    vTaskSuspendAll();
    bool busy = !queue->closed || queue->queue.users != 0 ||
                queue->send_waiters != NULL || queue->receive_waiters != NULL ||
                uxQueueMessagesWaiting(queue->queue.handle) != 0;
    (void)xTaskResumeAll();
    if (busy) {
        return NX_ERROR_BUSY;
    }
    vQueueDelete(queue->queue.handle);
    queue->queue.handle = NULL;
    return NX_SUCCESS;
}
