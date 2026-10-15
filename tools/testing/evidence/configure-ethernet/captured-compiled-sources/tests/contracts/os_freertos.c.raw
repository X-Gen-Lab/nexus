/**
 * \file            os_freertos.c
 * \brief           Actual FreeRTOS static queue, wake and reclaimable task
 *                  joins
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/freertos.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);    \
            abort();                                                           \
        }                                                                      \
    } while (0)

static nx_freertos_task_t s_coordinator;
static nx_freertos_task_t s_child;
static StackType_t s_coordinator_stack[4096];
static StackType_t s_child_stack[4096];
static nx_freertos_notify_t s_notification;
static uint32_t s_counter;
static uint32_t s_ready;
static uint32_t s_done;

/**
 * \brief           Execute a returning child and deliberately saturate its wake
 *                  latch.
 */
static void child_wake(void* context) {
    nx_wait_port_t* port = context;
    for (unsigned i = 0; i < 1000; ++i) {
        CHECK(port->wake(port->context) == NX_SUCCESS);
    }
    ++s_counter;
}

/** \brief Delay completion long enough to exercise join's retained timeout. */
static void child_delayed(void* context) {
    (void)context;
    vTaskDelay(20);
    ++s_counter;
}

/** \brief Predicate is the level authority independent of latch saturation. */
static bool ready(void* context) {
    (void)context;
    return __atomic_load_n(&s_ready, __ATOMIC_ACQUIRE) != 0;
}

/**
 * \brief           Wait in the real kernel and retain notification until
 *                  publisher joins.
 */
static void child_wait(void* context) {
    nx_wait_port_t* port = context;
    CHECK(nx_wait_until(port, ready, NULL,
                        nx_deadline_after(nx_time_now_us(), 1000000)) ==
          NX_SUCCESS);
    ++s_counter;
}

/** \brief Exercise real scheduler behavior from a separately declared task. */
static void coordinator(void* context) {
    (void)context;
    CHECK(!nx_freertos_isr_allowed());
    CHECK(nx_freertos_notify_init(&s_notification) == NX_SUCCESS);
    nx_wait_port_t port = nx_freertos_notify_port(&s_notification);
    uint32_t sequence = port.arm(port.context);
    CHECK(nx_freertos_task_start(&s_child, "wake", s_child_stack, 4096, 3,
                                 child_wake, &port) == NX_SUCCESS);
    CHECK(port.wait(port.context, sequence,
                    nx_deadline_after(nx_time_now_us(), 1000000)) ==
          NX_SUCCESS);
    fputs("FreeRTOS phase: join saturated notification publisher\n", stderr);
    CHECK(nx_freertos_task_join(&s_child,
                                nx_deadline_after(nx_time_now_us(), 1000000)) ==
          NX_SUCCESS);
    CHECK(s_counter == 1 && s_child.handle == NULL);
    sequence = port.arm(port.context);
    CHECK(port.wait(port.context, sequence,
                    nx_deadline_after(nx_time_now_us(), 2000)) ==
          NX_ERROR_TIMEOUT);
    CHECK(nx_freertos_task_start(&s_child, "delay", s_child_stack, 4096, 3,
                                 child_delayed, NULL) == NX_SUCCESS);
    CHECK(nx_freertos_task_join(&s_child,
                                nx_deadline_after(nx_time_now_us(), 1000)) ==
          NX_ERROR_TIMEOUT);
    CHECK(s_child.handle != NULL);
    fputs("FreeRTOS phase: join after retained timeout\n", stderr);
    CHECK(nx_freertos_task_join(&s_child,
                                nx_deadline_after(nx_time_now_us(), 1000000)) ==
          NX_SUCCESS);
    CHECK(s_counter == 2);
    fputs("FreeRTOS phase: repeated task reclamation\n", stderr);
    for (unsigned i = 0; i < 10; ++i) {
        CHECK(nx_freertos_task_start(&s_child, "reuse", s_child_stack, 4096, 3,
                                     child_wake, &port) == NX_SUCCESS);
        CHECK(nx_freertos_task_join(
                  &s_child, nx_deadline_after(nx_time_now_us(), 1000000)) ==
              NX_SUCCESS);
    }
    CHECK(s_counter == 12);
    fputs("FreeRTOS phase: active waiter quiescence\n", stderr);
    CHECK(nx_freertos_task_start(&s_child, "wait", s_child_stack, 4096, 3,
                                 child_wait, &port) == NX_SUCCESS);
    CHECK(__atomic_load_n(&s_notification.waiting, __ATOMIC_ACQUIRE) == 1);
    CHECK(nx_freertos_notify_destroy(&s_notification) == NX_ERROR_BUSY);
    __atomic_store_n(&s_ready, 1, __ATOMIC_RELEASE);
    CHECK(port.wake(port.context) == NX_SUCCESS);
    CHECK(nx_freertos_task_join(&s_child,
                                nx_deadline_after(nx_time_now_us(), 1000000)) ==
          NX_SUCCESS);
    CHECK(nx_freertos_notify_destroy(&s_notification) == NX_SUCCESS);
    fputs("FreeRTOS phase: exact static queue storage\n", stderr);
    nx_freertos_queue_t queue = {0};
    uint32_t bytes[3];
    CHECK(nx_freertos_queue_init(&queue, (uint8_t*)bytes, sizeof(bytes), 4,
                                 sizeof(uint32_t)) == NX_ERROR_INVALID);
    CHECK(nx_freertos_queue_init(&queue, (uint8_t*)bytes, sizeof(bytes), 3,
                                 sizeof(uint32_t)) == NX_SUCCESS);
    for (uint32_t i = 0; i < 3; ++i) {
        CHECK(nx_freertos_queue_send(&queue, &i, 0) == NX_SUCCESS);
    }
    uint32_t value = 99;
    CHECK(nx_freertos_queue_send(&queue, &value, 0) == NX_ERROR_TIMEOUT);
    CHECK(nx_freertos_queue_destroy(&queue) == NX_ERROR_BUSY);
    for (uint32_t i = 0; i < 3; ++i) {
        CHECK(nx_freertos_queue_receive(&queue, &value, 0) == NX_SUCCESS);
        CHECK(value == i);
    }
    CHECK(nx_freertos_queue_receive(&queue, &value, 0) == NX_ERROR_TIMEOUT);
    CHECK(nx_freertos_queue_destroy(&queue) == NX_SUCCESS);
    puts("Real FreeRTOS scheduler static queue, latched wake, timed join and "
         "TCB reuse passed");
    __atomic_store_n(&s_done, 1, __ATOMIC_RELEASE);
    vTaskEndScheduler();
}

/** \brief Start the actual pinned FreeRTOS POSIX port scheduler. */
int main(void) {
    CHECK(nx_freertos_task_start(&s_coordinator, "checks", s_coordinator_stack,
                                 4096, 2, coordinator, NULL) == NX_SUCCESS);
    vTaskStartScheduler();
    CHECK(__atomic_load_n(&s_done, __ATOMIC_ACQUIRE) == 1);
    return 0;
}
