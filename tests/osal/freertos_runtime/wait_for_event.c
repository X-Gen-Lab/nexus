/*
 * FreeRTOS Kernel V11.1.0
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 * Nexus host-test adaptation of portable/ThirdParty/GCC/Posix/utils/
 * wait_for_event.c at dbf70559b27d39c1fdb68dfb9a32140b6a6777a0.
 * Changes: cancellation releases the event mutex; timed waits use a normalized
 * monotonic deadline and return false on timeout/error; initialization failures
 * release allocated resources. The pinned kernel and POSIX port are unchanged.
 */

#include "wait_for_event.h"
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

struct event {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool event_triggered;
};

static void unlock_event_mutex(void* mutex) {
    (void)pthread_mutex_unlock(mutex);
}

struct event* event_create(void) {
    struct event* ev = malloc(sizeof(*ev));
    if (!ev) return NULL;
    if (pthread_mutex_init(&ev->mutex, NULL) != 0) {
        free(ev);
        return NULL;
    }
    pthread_condattr_t attributes;
    if (pthread_condattr_init(&attributes) != 0) {
        (void)pthread_mutex_destroy(&ev->mutex);
        free(ev);
        return NULL;
    }
    int status = pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
    if (status == 0) status = pthread_cond_init(&ev->cond, &attributes);
    (void)pthread_condattr_destroy(&attributes);
    if (status != 0) {
        (void)pthread_mutex_destroy(&ev->mutex);
        free(ev);
        return NULL;
    }
    ev->event_triggered = false;
    return ev;
}

/* The POSIX port joins a cancelled task before deleting its event. Callers
 * must likewise settle every waiter before destroying an event. */
void event_delete(struct event* ev) {
    if (!ev) return;
    (void)pthread_cond_destroy(&ev->cond);
    (void)pthread_mutex_destroy(&ev->mutex);
    free(ev);
}

bool event_wait(struct event* ev) {
    if (!ev || pthread_mutex_lock(&ev->mutex) != 0) return false;
    bool received = false;
    pthread_cleanup_push(unlock_event_mutex, &ev->mutex);
    int status = 0;
    while (!ev->event_triggered && status == 0)
        status = pthread_cond_wait(&ev->cond, &ev->mutex);
    if (status == 0 && ev->event_triggered) {
        ev->event_triggered = false;
        received = true;
    }
    /* pthread_cond_wait reacquires the mutex before cancellation cleanup. */
    pthread_cleanup_pop(1);
    return received;
}

static bool monotonic_deadline(time_t ms, struct timespec* deadline) {
    if (ms < 0 || clock_gettime(CLOCK_MONOTONIC, deadline) != 0 ||
        deadline->tv_sec < 0) return false;
    _Static_assert(sizeof(time_t) <= sizeof(uintmax_t), "time_t exceeds uintmax_t");
    uintmax_t seconds = (uintmax_t)ms / 1000u;
    long nanos = deadline->tv_nsec + (long)((uintmax_t)ms % 1000u) * 1000000L;
    if (nanos >= 1000000000L) {
        ++seconds;
        nanos -= 1000000000L;
    }
    uintmax_t maximum = UINTMAX_MAX >>
        ((sizeof(uintmax_t) - sizeof(time_t)) * CHAR_BIT);
    if ((time_t)-1 < 0) maximum >>= 1;
    uintmax_t current = (uintmax_t)deadline->tv_sec;
    if (seconds > maximum - current) return false;
    deadline->tv_sec = (time_t)(current + seconds);
    deadline->tv_nsec = nanos;
    return true;
}

bool event_wait_timed(struct event* ev, time_t ms) {
    struct timespec deadline;
    if (!ev || !monotonic_deadline(ms, &deadline) ||
        pthread_mutex_lock(&ev->mutex) != 0) return false;
    bool received = false;
    pthread_cleanup_push(unlock_event_mutex, &ev->mutex);
    int status = 0;
    while (!ev->event_triggered && status == 0)
        status = pthread_cond_timedwait(&ev->cond, &ev->mutex, &deadline);
    /* pthreads returns ETIMEDOUT/error directly; it does not use errno. */
    if (status == 0 && ev->event_triggered) {
        ev->event_triggered = false;
        received = true;
    }
    pthread_cleanup_pop(1);
    return received;
}

void event_signal(struct event* ev) {
    if (!ev || pthread_mutex_lock(&ev->mutex) != 0) return;
    ev->event_triggered = true;
    (void)pthread_cond_signal(&ev->cond);
    (void)pthread_mutex_unlock(&ev->mutex);
}
