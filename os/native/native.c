/**
 * \file            native.c
 * \brief           POSIX backend without global pools or hidden workers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define _POSIX_C_SOURCE 200809L
#include "nexus/os/native.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
    NATIVE_MUTEX = 1u,
    NATIVE_FIRST_CONDITION = 2u,
    NATIVE_SECOND_CONDITION = 4u
};

/* Only the executing pthread reads its identity; no startup handshake is
 * needed. */
static _Thread_local nx_native_task_t* current_task;

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
static nx_result_t notification_release(nx_native_notify_t* notification) {
    if (condition_release(&notification->condition, &notification->resources,
                          NATIVE_FIRST_CONDITION) != NX_SUCCESS) {
        return NX_ERROR_BUSY;
    }
    return mutex_release(&notification->mutex, &notification->resources);
}

/** \brief Preserve queue cleanup ownership through intermediate failures. */
static nx_result_t queue_release(nx_native_queue_t* queue) {
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
uint64_t nx_native_now_us(void) {
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
    if (nx_native_now_us() >= deadline_us) {
        return ETIMEDOUT;
    }
    struct timespec deadline = absolute_time(deadline_us);
    return pthread_cond_timedwait(condition, mutex, &deadline);
}

/** \brief Read wake sequence under the publisher mutex. */
static uint32_t notify_arm(void* context) {
    nx_native_notify_t* notification = context;
    pthread_mutex_lock(&notification->mutex);
    uint32_t sequence = notification->sequence;
    pthread_mutex_unlock(&notification->mutex);
    return sequence;
}

/** \brief Atomically recheck and sleep with a latched wake predicate. */
static nx_result_t notify_wait(void* context, uint32_t sequence,
                               uint64_t deadline_us) {
    nx_native_notify_t* notification = context;
    pthread_mutex_lock(&notification->mutex);
    if (notification->waiting) {
        pthread_mutex_unlock(&notification->mutex);
        return NX_ERROR_BUSY;
    }
    notification->waiting = true;
    nx_result_t result = NX_SUCCESS;
    for (;;) {
        /* A wake is a hint, so it cannot extend the caller's finite budget. */
        if (deadline_us != NX_DEADLINE_NEVER &&
            nx_native_now_us() >= deadline_us) {
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
    nx_native_notify_t* notification = context;
    pthread_mutex_lock(&notification->mutex);
    ++notification->sequence;
    pthread_cond_signal(&notification->condition);
    pthread_mutex_unlock(&notification->mutex);
    return NX_SUCCESS;
}

/** \brief Initialize actual caller-owned host synchronization. */
nx_result_t nx_native_notify_init(nx_native_notify_t* notification) {
    if (notification == NULL) {
        return NX_ERROR_INVALID;
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
nx_wait_port_t nx_native_notify_port(nx_native_notify_t* notification) {
    if (notification == NULL || !notification->initialized) {
        nx_wait_port_t invalid = {0};
        return invalid;
    }
    nx_wait_port_t port = {notification, notify_arm, notify_wait, notify_wake};
    return port;
}

/** \brief Release synchronization only after all publishers are quiesced. */
nx_result_t nx_native_notify_destroy(nx_native_notify_t* notification) {
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
nx_result_t nx_native_queue_init(nx_native_queue_t* queue, void* storage,
                                 size_t storage_bytes, size_t depth,
                                 size_t item_size) {
    if (queue == NULL || storage == NULL || depth == 0 || item_size == 0 ||
        depth > storage_bytes / item_size ||
        !storage_disjoint(queue, sizeof *queue, storage, depth * item_size)) {
        return NX_ERROR_INVALID;
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
static nx_result_t queue_transfer(nx_native_queue_t* queue, void* item,
                                  uint64_t deadline_us, bool sending) {
    if (queue == NULL || !queue->initialized || item == NULL) {
        return NX_ERROR_INVALID;
    }
    pthread_mutex_lock(&queue->mutex);
    ++queue->users;
    nx_result_t result = NX_SUCCESS;
    while (sending ? queue->count == queue->depth : queue->count == 0) {
        if (queue->closed) {
            result = NX_ERROR_STATE;
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
    if (sending && queue->closed) {
        result = NX_ERROR_STATE;
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
nx_result_t nx_native_queue_send(nx_native_queue_t* queue, const void* item,
                                 uint64_t deadline_us) {
    return queue_transfer(queue, (void*)item, deadline_us, true);
}

/** \brief Drain one consumer item. */
nx_result_t nx_native_queue_receive(nx_native_queue_t* queue, void* item,
                                    uint64_t deadline_us) {
    return queue_transfer(queue, item, deadline_us, false);
}

/** \brief Close admission without destroying the ability to drain. */
nx_result_t nx_native_queue_close(nx_native_queue_t* queue) {
    if (queue == NULL || !queue->initialized) {
        return NX_ERROR_INVALID;
    }
    pthread_mutex_lock(&queue->mutex);
    queue->closed = true;
    pthread_cond_broadcast(&queue->readable);
    pthread_cond_broadcast(&queue->writable);
    pthread_mutex_unlock(&queue->mutex);
    return NX_SUCCESS;
}

/** \brief Require quiesced callers and drained items before reclamation. */
nx_result_t nx_native_queue_destroy(nx_native_queue_t* queue) {
    if (queue == NULL || (!queue->initialized && queue->resources == 0u)) {
        return NX_ERROR_INVALID;
    }
    if (queue->initialized) {
        pthread_mutex_lock(&queue->mutex);
        bool busy = !queue->closed || queue->users != 0 || queue->count != 0;
        pthread_mutex_unlock(&queue->mutex);
        if (busy) {
            return NX_ERROR_BUSY;
        }
        queue->initialized = false;
    }
    return queue_release(queue);
}

/** \brief Execute caller entry without retaining storage after thread exit. */
static void* task_entry(void* argument) {
    nx_native_task_t* task = argument;
    current_task = task;
    task->entry(task->context);
    return NULL;
}

/** \brief Honor the caller's exact host stack storage. */
nx_result_t nx_native_task_start(nx_native_task_t* task, void* stack,
                                 size_t stack_bytes, void (*entry)(void*),
                                 void* context) {
    if (task == NULL || stack == NULL || entry == NULL ||
        !storage_disjoint(task, sizeof *task, stack, stack_bytes) ||
        task->started) {
        return NX_ERROR_INVALID;
    }
    pthread_attr_t attributes;
    int error = pthread_attr_init(&attributes);
    if (error != 0) {
        return creation_result(error);
    }
    error = pthread_attr_setstack(&attributes, stack, stack_bytes);
    if (error == 0) {
        task->entry = entry;
        task->context = context;
        error = pthread_create(&task->thread, &attributes, task_entry, task);
    }
    pthread_attr_destroy(&attributes);
    if (error != 0) {
        task->entry = NULL;
        task->context = NULL;
        return creation_result(error);
    }
    task->started = true;
    return NX_SUCCESS;
}

/** \brief Preserve stack and context until actual execution has exited. */
nx_result_t nx_native_task_join(nx_native_task_t* task) {
    if (task == NULL) {
        return NX_ERROR_INVALID;
    }
    if (task == current_task) {
        return NX_ERROR_BUSY;
    }
    if (!task->started) {
        return NX_ERROR_INVALID;
    }
    if (pthread_join(task->thread, NULL) != 0) {
        return NX_ERROR_IO;
    }
    task->started = false;
    return NX_SUCCESS;
}
