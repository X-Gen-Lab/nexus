/**
 * \file            posix_event.c
 * \brief           Cancellation-safe host events for the real FreeRTOS POSIX
 * port \author          Nexus Team \version         1.0.0 \date 2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define _POSIX_C_SOURCE 200809L
#include "utils/wait_for_event.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

/* The pinned POSIX port cancels a suspended task, signals its event and joins
 * its pthread. POSIX cancellation reacquires a cond-wait mutex before cleanup.
 * Every wait below releases that mutex on cancellation, preventing a task
 * reclamation deadlock. This is host scheduler support, not an MCU OS backend.
 */
struct event {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    bool triggered;
};

/** \brief A host primitive failure terminates the test instead of inventing
 * progress. */
static void require(int result) {
    if (result != 0) {
        abort();
    }
}

/** \brief Unlock the mutex reacquired by pthread_cond_wait during cancellation.
 */
static void release_wait(void* context) {
    struct event* event = context;
    require(pthread_mutex_unlock(&event->mutex));
}

/** \brief Allocate only the host port's explicit per-task event implementation.
 */
struct event* event_create(void) {
    struct event* event = calloc(1, sizeof(*event));
    if (event == NULL) {
        abort();
    }
    require(pthread_mutex_init(&event->mutex, NULL));
    require(pthread_cond_init(&event->condition, NULL));
    return event;
}

/** \brief Reclaim after the real port has joined every thread using the event.
 */
void event_delete(struct event* event) {
    require(pthread_cond_destroy(&event->condition));
    require(pthread_mutex_destroy(&event->mutex));
    free(event);
}

/** \brief Consume one latched signal and always release cancellation ownership.
 */
bool event_wait(struct event* event) {
    require(pthread_mutex_lock(&event->mutex));
    pthread_cleanup_push(release_wait, event);
    while (!event->triggered) {
        require(pthread_cond_wait(&event->condition, &event->mutex));
    }
    event->triggered = false;
    pthread_cleanup_pop(1);
    return true;
}

/** \brief Normalize the upstream millisecond deadline before a bounded wait. */
bool event_wait_timed(struct event* event, time_t milliseconds) {
    struct timespec deadline;
    if (milliseconds < 0 || clock_gettime(CLOCK_REALTIME, &deadline) != 0 ||
        __builtin_add_overflow(deadline.tv_sec, milliseconds / 1000,
                               &deadline.tv_sec)) {
        return false;
    }
    deadline.tv_nsec += (long)(milliseconds % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_nsec -= 1000000000L;
        if (__builtin_add_overflow(deadline.tv_sec, (time_t)1,
                                   &deadline.tv_sec)) {
            return false;
        }
    }
    bool signalled = false;
    require(pthread_mutex_lock(&event->mutex));
    pthread_cleanup_push(release_wait, event);
    int status = 0;
    while (!event->triggered && status == 0) {
        status =
            pthread_cond_timedwait(&event->condition, &event->mutex, &deadline);
    }
    if (status != 0 && status != ETIMEDOUT) {
        abort();
    }
    signalled = event->triggered;
    event->triggered = false;
    pthread_cleanup_pop(1);
    return signalled;
}

/** \brief Keep the signal latched even if its task has not started waiting. */
void event_signal(struct event* event) {
    require(pthread_mutex_lock(&event->mutex));
    event->triggered = true;
    require(pthread_cond_signal(&event->condition));
    require(pthread_mutex_unlock(&event->mutex));
}
