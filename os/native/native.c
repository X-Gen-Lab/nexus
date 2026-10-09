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
    struct timespec deadline = absolute_time(deadline_us);
    pthread_mutex_lock(&notification->mutex);
    if (notification->waiting) {
        pthread_mutex_unlock(&notification->mutex);
        return NX_ERROR_BUSY;
    }
    notification->waiting = true;
    nx_result_t result = NX_SUCCESS;
    while (notification->sequence == sequence) {
        int error = pthread_cond_timedwait(&notification->condition,
                                           &notification->mutex, &deadline);
        if (error != 0) {
            result = error == ETIMEDOUT ? NX_ERROR_TIMEOUT : NX_ERROR_IO;
            break;
        }
    }
    if (notification->sequence != sequence) {
        result = NX_SUCCESS;
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
    if (pthread_mutex_init(&notification->mutex, NULL) != 0) {
        return NX_ERROR_IO;
    }
    if (condition_init(&notification->condition) != 0) {
        pthread_mutex_destroy(&notification->mutex);
        return NX_ERROR_IO;
    }
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
    if (notification == NULL || !notification->initialized) {
        return NX_ERROR_INVALID;
    }
    pthread_mutex_lock(&notification->mutex);
    bool waiting = notification->waiting;
    pthread_mutex_unlock(&notification->mutex);
    if (waiting) {
        return NX_ERROR_BUSY;
    }
    if (pthread_cond_destroy(&notification->condition) != 0 ||
        pthread_mutex_destroy(&notification->mutex) != 0) {
        return NX_ERROR_BUSY;
    }
    notification->initialized = false;
    return NX_SUCCESS;
}

/** \brief Allocate only per-instance queue metadata supplied by the caller. */
nx_result_t nx_native_queue_init(nx_native_queue_t* queue, void* storage,
                                 size_t storage_bytes, size_t depth,
                                 size_t item_size) {
    if (queue == NULL || storage == NULL || depth == 0 || item_size == 0 ||
        depth > storage_bytes / item_size) {
        return NX_ERROR_INVALID;
    }
    memset(queue, 0, sizeof(*queue));
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        return NX_ERROR_IO;
    }
    if (condition_init(&queue->readable) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        return NX_ERROR_IO;
    }
    if (condition_init(&queue->writable) != 0) {
        pthread_cond_destroy(&queue->readable);
        pthread_mutex_destroy(&queue->mutex);
        return NX_ERROR_IO;
    }
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
    struct timespec deadline = absolute_time(deadline_us);
    pthread_mutex_lock(&queue->mutex);
    ++queue->users;
    nx_result_t result = NX_SUCCESS;
    while (sending ? queue->count == queue->depth : queue->count == 0) {
        if (queue->closed) {
            result = NX_ERROR_STATE;
            break;
        }
        int error = pthread_cond_timedwait(sending ? &queue->writable
                                                   : &queue->readable,
                                           &queue->mutex, &deadline);
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
    if (queue == NULL || !queue->initialized) {
        return NX_ERROR_INVALID;
    }
    pthread_mutex_lock(&queue->mutex);
    bool busy = !queue->closed || queue->users != 0 || queue->count != 0;
    pthread_mutex_unlock(&queue->mutex);
    if (busy) {
        return NX_ERROR_BUSY;
    }
    if (pthread_cond_destroy(&queue->readable) != 0 ||
        pthread_cond_destroy(&queue->writable) != 0 ||
        pthread_mutex_destroy(&queue->mutex) != 0) {
        return NX_ERROR_BUSY;
    }
    queue->initialized = false;
    return NX_SUCCESS;
}

/** \brief Execute caller entry without retaining storage after thread exit. */
static void* task_entry(void* argument) {
    nx_native_task_t* task = argument;
    task->entry(task->context);
    return NULL;
}

/** \brief Honor the caller's exact host stack storage. */
nx_result_t nx_native_task_start(nx_native_task_t* task, void* stack,
                                 size_t stack_bytes, void (*entry)(void*),
                                 void* context) {
    if (task == NULL || stack == NULL || entry == NULL || task->started) {
        return NX_ERROR_INVALID;
    }
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0) {
        return NX_ERROR_IO;
    }
    int error = pthread_attr_setstack(&attributes, stack, stack_bytes);
    if (error == 0) {
        task->entry = entry;
        task->context = context;
        error = pthread_create(&task->thread, &attributes, task_entry, task);
    }
    pthread_attr_destroy(&attributes);
    if (error != 0) {
        return NX_ERROR_INVALID;
    }
    task->started = true;
    return NX_SUCCESS;
}

/** \brief Preserve stack and context until actual execution has exited. */
nx_result_t nx_native_task_join(nx_native_task_t* task) {
    if (task == NULL || !task->started) {
        return NX_ERROR_INVALID;
    }
    if (pthread_equal(task->thread, pthread_self())) {
        return NX_ERROR_BUSY;
    }
    if (pthread_join(task->thread, NULL) != 0) {
        return NX_ERROR_IO;
    }
    task->started = false;
    return NX_SUCCESS;
}
