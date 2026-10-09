/**
 * \file            os_wait.c
 * \brief           Actual Native wait races, explicit stack and queue lifecycle
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define _POSIX_C_SOURCE 200809L
#include "nexus/os/baremetal.h"
#include "nexus/os/native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);    \
            abort();                                                           \
        }                                                                      \
    } while (0)

/** \brief Fixture preserves every publisher until join. */
typedef struct {
    nx_wait_port_t port;
    uint32_t ready;
    uint32_t waiting;
} fixture_t;

/** \brief Observe authoritative acquire predicate, not event count. */
static bool ready(void* context) {
    fixture_t* fixture = context;
    return __atomic_load_n(&fixture->ready, __ATOMIC_ACQUIRE) != 0;
}

/** \brief Wait from a real caller-owned Native stack. */
static void wait_task(void* context) {
    fixture_t* fixture = context;
    __atomic_store_n(&fixture->waiting, 1, __ATOMIC_RELEASE);
    CHECK(nx_wait_until(&fixture->port, ready, fixture,
                        nx_native_now_us() + 1000000u) == NX_SUCCESS);
}

/** \brief Wake between arm and sleep, including repeated latch saturation. */
static void native_lost_wake(void) {
    nx_native_notify_t notification;
    CHECK(nx_native_notify_init(&notification) == NX_SUCCESS);
    nx_wait_port_t port = nx_native_notify_port(&notification);
    uint32_t sequence = port.arm(port.context);
    for (unsigned i = 0; i < 1000; ++i) {
        CHECK(port.wake(port.context) == NX_SUCCESS);
    }
    CHECK(port.wait(port.context, sequence, nx_native_now_us() + 10000) ==
          NX_SUCCESS);
    sequence = port.arm(port.context);
    CHECK(port.wait(port.context, sequence, nx_native_now_us() + 1000) ==
          NX_ERROR_TIMEOUT);
    fixture_t fixture = {port, 0, 0};
    nx_native_task_t task = {0};
    void* stack;
    CHECK(posix_memalign(&stack, 4096, 65536) == 0);
    CHECK(nx_native_task_start(&task, stack, 16, wait_task, &fixture) ==
          NX_ERROR_INVALID);
    CHECK(nx_native_task_start(&task, stack, 65536, wait_task, &fixture) ==
          NX_SUCCESS);
    while (__atomic_load_n(&fixture.waiting, __ATOMIC_ACQUIRE) == 0) {
        struct timespec pause = {0, 100000};
        nanosleep(&pause, NULL);
    }
    bool entered = false;
    for (unsigned i = 0; i < 1000 && !entered; ++i) {
        pthread_mutex_lock(&notification.mutex);
        entered = notification.waiting;
        pthread_mutex_unlock(&notification.mutex);
        if (!entered) {
            struct timespec pause = {0, 100000};
            nanosleep(&pause, NULL);
        }
    }
    CHECK(entered);
    CHECK(nx_native_notify_destroy(&notification) == NX_ERROR_BUSY);
    __atomic_store_n(&fixture.ready, 1, __ATOMIC_RELEASE);
    CHECK(port.wake(port.context) == NX_SUCCESS);
    CHECK(nx_native_task_join(&task) == NX_SUCCESS);
    free(stack);
    CHECK(nx_native_notify_destroy(&notification) == NX_SUCCESS);
}

/** \brief Model a monotonic bare-metal source. */
static uint64_t clock_read(void* context) {
    return *(uint64_t*)context;
}

/** \brief Bare-metal BUSY never pretends to block or run a task. */
static void baremetal_poll(void) {
    uint64_t now = 100;
    nx_baremetal_notify_t notification;
    CHECK(nx_baremetal_notify_init(&notification, clock_read, &now) ==
          NX_SUCCESS);
    nx_wait_port_t port = nx_baremetal_notify_port(&notification);
    uint32_t sequence = port.arm(port.context);
    CHECK(port.wait(port.context, sequence, 200) == NX_ERROR_BUSY);
    CHECK(port.wake(port.context) == NX_SUCCESS);
    CHECK(port.wait(port.context, sequence, 200) == NX_SUCCESS);
    sequence = port.arm(port.context);
    now = 200;
    CHECK(port.wait(port.context, sequence, 200) == NX_ERROR_TIMEOUT);
}

/**
 * \brief           Exact-depth queues reject exhaustion and close without
 *                  losing drain.
 */
static void queue_lifecycle(void) {
    nx_native_queue_t queue;
    uint32_t storage[3];
    CHECK(nx_native_queue_init(&queue, storage, sizeof(storage), 4,
                               sizeof(uint32_t)) == NX_ERROR_INVALID);
    CHECK(nx_native_queue_init(&queue, storage, sizeof(storage), 3,
                               sizeof(uint32_t)) == NX_SUCCESS);
    for (uint32_t i = 0; i < 3; ++i) {
        CHECK(nx_native_queue_send(&queue, &i, nx_native_now_us()) ==
              NX_SUCCESS);
    }
    uint32_t value = 99;
    CHECK(nx_native_queue_send(&queue, &value, nx_native_now_us()) ==
          NX_ERROR_TIMEOUT);
    CHECK(nx_native_queue_destroy(&queue) == NX_ERROR_BUSY);
    CHECK(nx_native_queue_receive(&queue, &value, nx_native_now_us()) ==
          NX_SUCCESS);
    CHECK(value == 0);
    value = 3;
    CHECK(nx_native_queue_send(&queue, &value, nx_native_now_us()) ==
          NX_SUCCESS);
    CHECK(nx_native_queue_close(&queue) == NX_SUCCESS);
    CHECK(nx_native_queue_send(&queue, &value, nx_native_now_us()) ==
          NX_ERROR_STATE);
    for (uint32_t i = 1; i < 4; ++i) {
        CHECK(nx_native_queue_receive(&queue, &value, nx_native_now_us()) ==
              NX_SUCCESS);
        CHECK(value == i);
    }
    CHECK(nx_native_queue_receive(&queue, &value, nx_native_now_us()) ==
          NX_ERROR_STATE);
    CHECK(nx_native_queue_destroy(&queue) == NX_SUCCESS);
}

/** \brief Run real host synchronization and bare-metal progress contracts. */
int main(void) {
    baremetal_poll();
    native_lost_wake();
    queue_lifecycle();
    puts("Native arm/recheck, saturation, static stack join and queue drain "
         "passed");
    return 0;
}
