/**
 * \file            native.c
 * \brief           POSIX backend without global pools or hidden workers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "nexus/os/storage.h"
#include <errno.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(__SANITIZE_THREAD__)
#define NATIVE_THREAD_SANITIZED
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define NATIVE_THREAD_SANITIZED
#endif
#endif

enum {
    NATIVE_MUTEX = 1u,
    NATIVE_FIRST_CONDITION = 2u,
    NATIVE_SECOND_CONDITION = 4u
};

/* Only the executing pthread reads its identity; no startup handshake is
 * needed. */
static _Thread_local void* current_task;
static uint32_t kernel_entered;

/** \brief Preserve resource exhaustion and permission failures at admission. */
static nx_result_t creation_result(int error) {
    if (error == EAGAIN || error == ENOMEM) {
        return NX_ERROR_EXHAUSTED;
    }
    if (error == EINVAL) {
        return NX_ERROR_INVALID;
    }
    if (error == EPERM || error == EACCES) {
        return NX_ERROR_PERMISSION;
    }
    return NX_ERROR_IO;
}

/** \brief Reject overlapping or wrapping known caller storage spans. */
static bool storage_disjoint(const void* control, size_t control_bytes,
                             const void* storage, size_t storage_bytes) {
    uintptr_t control_begin = (uintptr_t)control;
    uintptr_t storage_begin = (uintptr_t)storage;
    if (control_bytes > UINTPTR_MAX - control_begin ||
        storage_bytes > UINTPTR_MAX - storage_begin) {
        return false;
    }
    return control_begin + control_bytes <= storage_begin ||
           storage_begin + storage_bytes <= control_begin;
}

/** \brief Reject known borrowed pointers inside control or execution storage.
 */
static bool points_into(const void* pointer, const void* storage,
                        size_t bytes) {
    uintptr_t point = (uintptr_t)pointer;
    uintptr_t begin = (uintptr_t)storage;
    return point >= begin && point - begin < bytes;
}

/** \brief Release an owned condition once, retaining failed cleanup for retry.
 */
static nx_result_t condition_release(pthread_cond_t* condition,
                                     uint8_t* resources, uint8_t bit) {
    if ((*resources & bit) == 0u) {
        return NX_SUCCESS;
    }
    if (pthread_cond_destroy(condition) != 0) {
        return NX_ERROR_BUSY;
    }
    *resources &= (uint8_t)~bit;
    return NX_SUCCESS;
}

/** \brief Release the mutex only after every owned condition is released. */
static nx_result_t mutex_release(pthread_mutex_t* mutex, uint8_t* resources) {
    if ((*resources & NATIVE_MUTEX) == 0u) {
        return NX_SUCCESS;
    }
    if (pthread_mutex_destroy(mutex) != 0) {
        return NX_ERROR_BUSY;
    }
    *resources &= (uint8_t)~NATIVE_MUTEX;
    return NX_SUCCESS;
}

/** \brief Continue notification cleanup from its last successful release. */
static nx_result_t notification_release(nx_notify_t* notification) {
    if (condition_release(&notification->condition, &notification->resources,
                          NATIVE_FIRST_CONDITION) != NX_SUCCESS) {
        return NX_ERROR_BUSY;
    }
    return mutex_release(&notification->mutex, &notification->resources);
}

/** \brief Preserve queue cleanup ownership through intermediate failures. */
static nx_result_t queue_release(nx_queue_t* queue) {
    if (condition_release(&queue->readable, &queue->resources,
                          NATIVE_FIRST_CONDITION) != NX_SUCCESS ||
        condition_release(&queue->writable, &queue->resources,
                          NATIVE_SECOND_CONDITION) != NX_SUCCESS) {
        return NX_ERROR_BUSY;
    }
    return mutex_release(&queue->mutex, &queue->resources);
}

/** \brief Convert an absolute microsecond deadline without tick rounding. */
static struct timespec absolute_time(uint64_t us) {
    struct timespec value = {(time_t)(us / 1000000u),
                             (long)((us % 1000000u) * 1000u)};
    return value;
}

/** \brief Create an absolute-monotonic condition variable. */
static int condition_init(pthread_cond_t* condition) {
    pthread_condattr_t attributes;
    int result = pthread_condattr_init(&attributes);
    if (result != 0) {
        return result;
    }
    result = pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
    if (result == 0) {
        result = pthread_cond_init(condition, &attributes);
    }
    pthread_condattr_destroy(&attributes);
    return result;
}

/** \brief Read the actual host monotonic clock. */
uint64_t nx_os_now_us(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
        abort();
    }
    return (uint64_t)value.tv_sec * 1000000u + (uint64_t)value.tv_nsec / 1000u;
}

/** \brief Never convert the infinite sentinel to an OS absolute time. */
static int condition_wait(pthread_cond_t* condition, pthread_mutex_t* mutex,
                          uint64_t deadline_us) {
    if (deadline_us == NX_DEADLINE_NEVER) {
        return pthread_cond_wait(condition, mutex);
    }
    if (nx_os_now_us() >= deadline_us) {
        return ETIMEDOUT;
    }
    struct timespec deadline = absolute_time(deadline_us);
    return pthread_cond_timedwait(condition, mutex, &deadline);
}

/** \brief Read wake sequence under the publisher mutex. */
static uint32_t notify_arm(void* context) {
    nx_notify_t* notification = context;
    pthread_mutex_lock(&notification->mutex);
    uint32_t sequence = notification->sequence;
    pthread_mutex_unlock(&notification->mutex);
    return sequence;
}

/** \brief Atomically recheck and sleep with a latched wake predicate. */
static nx_result_t notify_wait(void* context, uint32_t sequence,
                               uint64_t deadline_us) {
    nx_notify_t* notification = context;
    pthread_mutex_lock(&notification->mutex);
    if (notification->waiting) {
        pthread_mutex_unlock(&notification->mutex);
        return NX_ERROR_BUSY;
    }
    notification->waiting = true;
    nx_result_t result = NX_SUCCESS;
    for (;;) {
        /* A wake is a hint, so it cannot extend the caller's finite budget. */
        if (deadline_us != NX_DEADLINE_NEVER && nx_os_now_us() >= deadline_us) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        if (notification->sequence != sequence) {
            break;
        }
        int error = condition_wait(&notification->condition,
                                   &notification->mutex, deadline_us);
        if (error != 0) {
            result = error == ETIMEDOUT ? NX_ERROR_TIMEOUT : NX_ERROR_IO;
            break;
        }
    }
    notification->waiting = false;
    pthread_mutex_unlock(&notification->mutex);
    return result;
}

/** \brief Publish before signalling while holding the same waiter mutex. */
static nx_result_t notify_wake(void* context) {
    nx_notify_t* notification = context;
    pthread_mutex_lock(&notification->mutex);
    ++notification->sequence;
    pthread_cond_signal(&notification->condition);
    pthread_mutex_unlock(&notification->mutex);
    return NX_SUCCESS;
}

/** \brief Initialize actual caller-owned host synchronization. */
nx_result_t nx_notify_init(nx_notify_t* notification) {
    if (notification == NULL) {
        return NX_ERROR_INVALID;
    }
    if (notification->initialized || notification->resources != 0U) {
        return NX_ERROR_BUSY;
    }
    memset(notification, 0, sizeof(*notification));
    int error = pthread_mutex_init(&notification->mutex, NULL);
    if (error != 0) {
        return creation_result(error);
    }
    notification->resources = NATIVE_MUTEX;
    error = condition_init(&notification->condition);
    if (error != 0) {
        return notification_release(notification) == NX_SUCCESS
                   ? creation_result(error)
                   : NX_ERROR_BUSY;
    }
    notification->resources |= NATIVE_FIRST_CONDITION;
    notification->initialized = true;
    return NX_SUCCESS;
}

/** \brief Bind the caller-owned notification. */
nx_wait_port_t nx_notify_port(nx_notify_t* notification) {
    if (notification == NULL || !notification->initialized) {
        nx_wait_port_t invalid = {0};
        return invalid;
    }
    nx_wait_port_t port = {notification, notify_arm, notify_wait, notify_wake};
    return port;
}

/** \brief Publish a hint only through a live task-domain notification. */
nx_result_t nx_notify_wake(nx_notify_t* notification) {
    if (notification == NULL || !notification->initialized) {
        return NX_ERROR_INVALID;
    }
    return notify_wake(notification);
}

/** \brief Host signals are not admitted as hardware interrupt publishers. */
nx_result_t nx_notify_wake_from_isr(nx_notify_t* notification) {
    (void)notification;
    return NX_ERROR_CONTEXT;
}

/** \brief Release synchronization only after all publishers are quiesced. */
nx_result_t nx_notify_destroy(nx_notify_t* notification) {
    if (notification == NULL ||
        (!notification->initialized && notification->resources == 0u)) {
        return NX_ERROR_INVALID;
    }
    if (notification->initialized) {
        pthread_mutex_lock(&notification->mutex);
        bool waiting = notification->waiting;
        pthread_mutex_unlock(&notification->mutex);
        if (waiting) {
            return NX_ERROR_BUSY;
        }
        notification->initialized = false;
    }
    return notification_release(notification);
}

/** \brief Allocate only per-instance queue metadata supplied by the caller. */
nx_result_t nx_queue_init(nx_queue_t* queue, void* storage,
                          size_t storage_bytes, size_t depth,
                          size_t item_size) {
    if (queue == NULL || storage == NULL || depth == 0 || item_size == 0 ||
        depth > storage_bytes / item_size ||
        !storage_disjoint(queue, sizeof *queue, storage, storage_bytes)) {
        return NX_ERROR_INVALID;
    }
    if (queue->initialized || queue->resources != 0U) {
        return NX_ERROR_BUSY;
    }
    memset(queue, 0, sizeof(*queue));
    int error = pthread_mutex_init(&queue->mutex, NULL);
    if (error != 0) {
        return creation_result(error);
    }
    queue->resources = NATIVE_MUTEX;
    error = condition_init(&queue->readable);
    if (error == 0) {
        queue->resources |= NATIVE_FIRST_CONDITION;
        error = condition_init(&queue->writable);
    }
    if (error != 0) {
        return queue_release(queue) == NX_SUCCESS ? creation_result(error)
                                                  : NX_ERROR_BUSY;
    }
    queue->resources |= NATIVE_SECOND_CONDITION;
    queue->bytes = storage;
    queue->depth = depth;
    queue->item_size = item_size;
    queue->initialized = true;
    return NX_SUCCESS;
}

/**
 * \brief           Copy one item while keeping blocking outside the metadata
 *                  lifetime.
 */
static nx_result_t queue_transfer(nx_queue_t* queue, void* item,
                                  uint64_t deadline_us, bool sending,
                                  nx_channel_t* channel) {
    if (queue == NULL || !queue->initialized || item == NULL) {
        return NX_ERROR_INVALID;
    }
    pthread_mutex_lock(&queue->mutex);
    if (queue->users == SIZE_MAX ||
        (channel != NULL && queue->users >= channel->maximum_waiters)) {
        pthread_mutex_unlock(&queue->mutex);
        return NX_ERROR_BUSY;
    }
    ++queue->users;
    nx_result_t result = NX_SUCCESS;
    while (sending ? queue->count == queue->depth : queue->count == 0) {
        if (channel != NULL && channel->closed) {
            result = NX_ERROR_CANCELLED;
            break;
        }
        int error =
            condition_wait(sending ? &queue->writable : &queue->readable,
                           &queue->mutex, deadline_us);
        if (error != 0) {
            result = error == ETIMEDOUT ? NX_ERROR_TIMEOUT : NX_ERROR_IO;
            break;
        }
    }
    if (sending && channel != NULL && channel->closed) {
        result = NX_ERROR_CANCELLED;
    }
    if (result == NX_SUCCESS) {
        size_t index = queue->head;
        if (sending) {
            index = queue->count >= queue->depth - queue->head
                        ? queue->count - (queue->depth - queue->head)
                        : queue->head + queue->count;
        }
        uint8_t* slot = queue->bytes + index * queue->item_size;
        if (sending) {
            memcpy(slot, item, queue->item_size);
            ++queue->count;
            pthread_cond_signal(&queue->readable);
        } else {
            memcpy(item, slot, queue->item_size);
            queue->head = queue->head + 1 == queue->depth ? 0 : queue->head + 1;
            --queue->count;
            pthread_cond_signal(&queue->writable);
        }
    }
    --queue->users;
    pthread_mutex_unlock(&queue->mutex);
    return result;
}

/** \brief Copy one producer item. */
nx_result_t nx_queue_send_until(nx_queue_t* queue, const void* item,
                                uint64_t deadline_us) {
    return queue_transfer(queue, (void*)item, deadline_us, true, NULL);
}

/** \brief Drain one consumer item. */
nx_result_t nx_queue_receive_until(nx_queue_t* queue, void* item,
                                   uint64_t deadline_us) {
    return queue_transfer(queue, item, deadline_us, false, NULL);
}

/** \brief Close admission without destroying the ability to drain. */
nx_result_t nx_channel_close(nx_channel_t* channel) {
    if (channel == NULL || !channel->queue.initialized) {
        return NX_ERROR_INVALID;
    }
    nx_queue_t* queue = &channel->queue;
    pthread_mutex_lock(&queue->mutex);
    channel->closed = true;
    pthread_cond_broadcast(&queue->readable);
    pthread_cond_broadcast(&queue->writable);
    pthread_mutex_unlock(&queue->mutex);
    return NX_SUCCESS;
}

/** \brief Require quiesced callers and drained items before reclamation. */
static nx_result_t destroy_queue(nx_queue_t* queue,
                                 const nx_channel_t* channel) {
    if (queue == NULL || (!queue->initialized && queue->resources == 0u)) {
        return NX_ERROR_INVALID;
    }
    if (queue->initialized) {
        pthread_mutex_lock(&queue->mutex);
        bool busy = (channel != NULL && !channel->closed) ||
                    queue->users != 0 || queue->count != 0;
        pthread_mutex_unlock(&queue->mutex);
        if (busy) {
            return NX_ERROR_BUSY;
        }
        queue->initialized = false;
    }
    return queue_release(queue);
}

/** \brief Reclaim a raw queue only after callers and queued items are gone. */
nx_result_t nx_queue_destroy(nx_queue_t* queue) {
    return destroy_queue(queue, NULL);
}

/** \brief Preserve partial initialization resources for channel destruction. */
nx_result_t nx_channel_init(nx_channel_t* channel, void* storage,
                            size_t storage_bytes, size_t depth,
                            size_t item_size, size_t maximum_waiters) {
    if (channel == NULL || storage == NULL || maximum_waiters == 0 ||
        depth == 0 || item_size == 0 || depth > storage_bytes / item_size ||
        !storage_disjoint(channel, sizeof *channel, storage, storage_bytes)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_queue_init(&channel->queue, storage, storage_bytes,
                                       depth, item_size);
    if (result == NX_SUCCESS) {
        channel->maximum_waiters = maximum_waiters;
        channel->closed = false;
    }
    return result;
}

/** \brief Initialize an operation-local guard without a kernel object. */
nx_result_t nx_channel_waiter_init(nx_channel_waiter_t* waiter) {
    if (waiter == NULL) {
        return NX_ERROR_INVALID;
    }
    if (waiter->initialized ||
        __atomic_load_n(&waiter->active, __ATOMIC_ACQUIRE)) {
        return NX_ERROR_BUSY;
    }
    waiter->initialized = true;
    return NX_SUCCESS;
}

/** \brief Require the exclusive operation to end before waiter reuse. */
nx_result_t nx_channel_waiter_destroy(nx_channel_waiter_t* waiter) {
    if (waiter == NULL || !waiter->initialized) {
        return NX_ERROR_INVALID;
    }
    if (__atomic_load_n(&waiter->active, __ATOMIC_ACQUIRE) != 0U) {
        return NX_ERROR_BUSY;
    }
    waiter->initialized = false;
    return NX_SUCCESS;
}

/** \brief Claim one explicit waiter across the complete blocking operation. */
static nx_result_t channel_transfer(nx_channel_t* channel, void* item,
                                    nx_time_us_t deadline,
                                    nx_channel_waiter_t* waiter, bool sending) {
    if (channel == NULL || !channel->queue.initialized || item == NULL ||
        waiter == NULL || !waiter->initialized ||
        !storage_disjoint(channel, sizeof *channel, waiter, sizeof *waiter) ||
        !storage_disjoint(waiter, sizeof *waiter, channel->queue.bytes,
                          channel->queue.depth * channel->queue.item_size)) {
        return NX_ERROR_INVALID;
    }
    uint32_t unused = 0U;
    if (!__atomic_compare_exchange_n(&waiter->active, &unused, 1U, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result =
        queue_transfer(&channel->queue, item, deadline, sending, channel);
    __atomic_store_n(&waiter->active, 0U, __ATOMIC_RELEASE);
    return result;
}

/** \brief Copy one item while the channel admits new sends. */
nx_result_t nx_channel_send_until(nx_channel_t* channel, const void* item,
                                  nx_time_us_t deadline,
                                  nx_channel_waiter_t* waiter) {
    return channel_transfer(channel, (void*)item, deadline, waiter, true);
}

/** \brief Retain queued items after close and report cancellation when empty.
 */
nx_result_t nx_channel_receive_until(nx_channel_t* channel, void* item,
                                     nx_time_us_t deadline,
                                     nx_channel_waiter_t* waiter) {
    return channel_transfer(channel, item, deadline, waiter, false);
}

/** \brief Reclaim closed channels or continue failed-init resource cleanup. */
nx_result_t nx_channel_destroy(nx_channel_t* channel) {
    if (channel == NULL) {
        return NX_ERROR_INVALID;
    }
    return destroy_queue(&channel->queue, channel);
}

/** \brief Execute caller entry without retaining storage after thread exit. */
static void* task_entry(void* argument) {
    nx_task_t* task = argument;
    current_task = task;
    task->entry(task->context);
    return NULL;
}

/** \brief A permanent entry returning cannot authorize lifecycle cleanup. */
static void* permanent_entry(void* argument) {
    nx_permanent_task_t* task = argument;
    current_task = task;
    task->entry(task->context);
    abort();
}

/** \brief Admit only disjoint caller storage and the host scheduling level. */
static bool task_admitted(const void* task, size_t control_bytes,
                          const char* name, void* stack, size_t stack_bytes,
                          uint32_t priority, void (*entry)(void*),
                          const void* context) {
    return task != NULL && name != NULL && stack != NULL && entry != NULL &&
           stack_bytes != 0 && priority < NX_OS_PRIORITY_LEVELS &&
           (uintptr_t)stack % NX_OS_STACK_ALIGNMENT == 0U &&
           storage_disjoint(task, control_bytes, stack, stack_bytes) &&
           !points_into(name, task, control_bytes) &&
           !points_into(name, stack, stack_bytes) &&
           !points_into(context, task, control_bytes) &&
           !points_into(context, stack, stack_bytes) && name[0] != '\0';
}

/** \brief Honor the exact caller stack before creating a real pthread. */
static nx_result_t create_thread(pthread_t* thread, void* stack,
                                 size_t stack_bytes, void* (*trampoline)(void*),
                                 void* task) {
    pthread_attr_t attributes;
    int error = pthread_attr_init(&attributes);
    if (error != 0) {
        return creation_result(error);
    }
    error = pthread_attr_setstack(&attributes, stack, stack_bytes);
    if (error == 0) {
        error = pthread_create(thread, &attributes, trampoline, task);
    }
    pthread_attr_destroy(&attributes);
    return error == 0 ? NX_SUCCESS : creation_result(error);
}

/** \brief Prepare all borrowed state before a child can execute immediately. */
nx_result_t nx_task_start(nx_task_t* task, const char* name, void* stack,
                          size_t stack_bytes, uint32_t priority,
                          void (*entry)(void*), void* context) {
    if (!task_admitted(task, sizeof *task, name, stack, stack_bytes, priority,
                       entry, context) ||
        task->started) {
        return NX_ERROR_INVALID;
    }
    task->entry = entry;
    task->context = context;
    nx_result_t result =
        create_thread(&task->thread, stack, stack_bytes, task_entry, task);
    if (result != NX_SUCCESS) {
        task->entry = NULL;
        task->context = NULL;
        return result;
    }
    task->started = true;
    return NX_SUCCESS;
}

/** \brief Permanent tasks have no completion object or detach side effect. */
nx_result_t nx_permanent_task_start(nx_permanent_task_t* task, const char* name,
                                    void* stack, size_t stack_bytes,
                                    uint32_t priority, void (*entry)(void*),
                                    void* context) {
    if (!task_admitted(task, sizeof *task, name, stack, stack_bytes, priority,
                       entry, context) ||
        task->started) {
        return NX_ERROR_INVALID;
    }
    task->entry = entry;
    task->context = context;
    nx_result_t result =
        create_thread(&task->thread, stack, stack_bytes, permanent_entry, task);
    if (result != NX_SUCCESS) {
        task->entry = NULL;
        task->context = NULL;
        return result;
    }
    task->started = true;
    return NX_SUCCESS;
}

#if defined(NATIVE_THREAD_SANITIZED)
/** \brief Retain real thread-exit ordering in runtimes without clockjoin hooks.
 */
static int sanitized_join_before(pthread_t thread, nx_time_us_t deadline) {
    for (;;) {
        int error = pthread_tryjoin_np(thread, NULL);
        if (error != EBUSY) {
            return error;
        }
        nx_time_us_t now = nx_os_now_us();
        if (now >= deadline) {
            return ETIMEDOUT;
        }
        nx_time_us_t wake = deadline - now > 1000U ? now + 1000U : deadline;
        struct timespec absolute = absolute_time(wake);
        error =
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &absolute, NULL);
        if (error != 0 && error != EINTR) {
            return error;
        }
    }
}
#endif

/** \brief Actual pthread exit, not an entry flag, ends the retained loan. */
nx_result_t nx_task_join_until(nx_task_t* task, nx_time_us_t deadline) {
    if (task == NULL) {
        return NX_ERROR_INVALID;
    }
    if (task == current_task) {
        return NX_ERROR_CONTEXT;
    }
    if (!task->started) {
        return NX_ERROR_INVALID;
    }
    int error = pthread_tryjoin_np(task->thread, NULL);
    if (error == EBUSY && deadline == NX_DEADLINE_NEVER) {
        error = pthread_join(task->thread, NULL);
    } else if (error == EBUSY) {
        if (nx_os_now_us() >= deadline) {
            return NX_ERROR_TIMEOUT;
        }
#if defined(NATIVE_THREAD_SANITIZED)
        error = sanitized_join_before(task->thread, deadline);
#else
        struct timespec absolute = absolute_time(deadline);
        error = pthread_clockjoin_np(task->thread, NULL, CLOCK_MONOTONIC,
                                     &absolute);
#endif
    }
    if (error != 0) {
        return error == ETIMEDOUT ? NX_ERROR_TIMEOUT : NX_ERROR_IO;
    }
    task->started = false;
    task->entry = NULL;
    task->context = NULL;
    return NX_SUCCESS;
}

/** \brief Native execution has no pre-scheduler discard phase. */
nx_result_t nx_task_discard_before_scheduler(nx_task_t* task) {
    (void)task;
    return NX_ERROR_CONTEXT;
}

/** \brief Enter once and retain the calling bootstrap stack without a worker.
 */
nx_result_t nx_os_run(void (*startup)(void*), void* context) {
    if (startup == NULL) {
        return NX_ERROR_INVALID;
    }
    uint32_t cold = 0U;
    if (current_task != NULL ||
        !__atomic_compare_exchange_n(&kernel_entered, &cold, 1U, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        return NX_ERROR_CONTEXT;
    }
    startup(context);
    for (;;) {
        (void)pause();
    }
}

/** \brief EINTR retries retain the original absolute sleep deadline. */
nx_result_t nx_sleep_until(nx_time_us_t deadline) {
    if (deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (nx_os_now_us() >= deadline) {
        return NX_SUCCESS;
    }
    struct timespec absolute = absolute_time(deadline);
    int error;
    do {
        error =
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &absolute, NULL);
    } while (error == EINTR);
    return error == 0 ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief Offer an execution opportunity without promising scheduling order. */
nx_result_t nx_task_yield(void) {
    return sched_yield() == 0 ? NX_SUCCESS : NX_ERROR_IO;
}
