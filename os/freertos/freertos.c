/**
 * \file            freertos.c
 * \brief           Thin static-object FreeRTOS adapter
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
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
    while (notify_arm(context) == sequence) {
        TickType_t ticks = deadline_ticks(deadline_us);
        if (ticks == 0) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        (void)xSemaphoreTake(notification->handle, ticks);
    }
    if (notify_arm(context) != sequence) {
        result = NX_SUCCESS;
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

/** \brief Create only caller-selected static task resources. */
nx_result_t nx_freertos_task_start(nx_freertos_task_t* task, const char* name,
                                   StackType_t* stack, size_t stack_words,
                                   UBaseType_t priority, void (*entry)(void*),
                                   void* context) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (task == NULL || name == NULL || stack == NULL || entry == NULL ||
        stack_words == 0 || stack_words > UINT32_MAX || task->handle != NULL ||
        priority >= configMAX_PRIORITIES) {
        return NX_ERROR_INVALID;
    }
    task->finished = xSemaphoreCreateBinaryStatic(&task->finished_storage);
    if (task->finished == NULL) {
        return NX_ERROR_IO;
    }
    task->entry = entry;
    task->context = context;
    task->handle =
        xTaskCreateStatic(task_entry, name, (configSTACK_DEPTH_TYPE)stack_words,
                          task, priority, stack, &task->control);
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
