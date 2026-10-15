/**
 * \file            main.c
 * \brief           Actual target ABI objects and retained production OS paths
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/freertos.h"
#include "nexus_config.h"

#define NX_OS_PROBE_STORAGE __attribute__((used, section(".bss.os.probes")))

void _start(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

StaticTask_t nx_os_tcb__bytes NX_OS_PROBE_STORAGE;
nx_wait_port_t nx_os_wait_port__bytes NX_OS_PROBE_STORAGE;
nx_freertos_notify_t nx_os_notify__bytes NX_OS_PROBE_STORAGE;
nx_freertos_direct_notify_t nx_os_direct_notify__bytes NX_OS_PROBE_STORAGE;
nx_freertos_queue_t nx_os_raw_queue__bytes NX_OS_PROBE_STORAGE;
nx_freertos_closable_queue_t nx_os_closable_queue__bytes NX_OS_PROBE_STORAGE;
nx_freertos_queue_waiter_t nx_os_queue_waiter__bytes NX_OS_PROBE_STORAGE;
nx_freertos_task_t nx_os_joinable_task__bytes NX_OS_PROBE_STORAGE;
nx_freertos_permanent_task_t nx_os_permanent_task__bytes NX_OS_PROBE_STORAGE;
_Alignas(8) StackType_t nx_os_join_stack__bytes[128] NX_OS_PROBE_STORAGE;
_Alignas(8) StackType_t nx_os_permanent_stack__bytes[128] NX_OS_PROBE_STORAGE;
uint32_t nx_os_raw_payload__bytes[3] NX_OS_PROBE_STORAGE;
uint32_t nx_os_closable_payload__bytes[3] NX_OS_PROBE_STORAGE;

static void (*const exception_references[])(void)
    __attribute__((used, section(".runtime.references"))) = {
        SVC_Handler, PendSV_Handler, SysTick_Handler};
static volatile uint32_t result_sink;
static volatile nx_time_us_t fixture_time;

/** \brief Synthetic link-fixture clock, never a product timer implementation.
 */
nx_time_us_t nx_time_now_us(void) {
    return ++fixture_time;
}

/** \brief Retain a returning task entry without introducing an application. */
static void returning_entry(void* context) {
    (void)context;
}

/** \brief A permanent entry cannot return to the kernel task frame. */
static void permanent_entry(void* context) {
    (void)context;
    for (;;) {
    }
}

/** \brief This reference only retains waiter predicate mechanics. */
static bool not_ready(void* context) {
    (void)context;
    return false;
}

/** \brief Emit true target-ABI sizeof as absolute ELF symbols. */
static void object_size_symbols(void) {
    __asm__ volatile(".global __nexus_os_size_tcb\n"
                     ".equ __nexus_os_size_tcb,%c0\n"
                     ".global __nexus_os_size_wait_port\n"
                     ".equ __nexus_os_size_wait_port,%c1\n"
                     ".global __nexus_os_size_joinable_task\n"
                     ".equ __nexus_os_size_joinable_task,%c2\n"
                     ".global __nexus_os_size_permanent_task\n"
                     ".equ __nexus_os_size_permanent_task,%c3\n"
                     ".global __nexus_os_size_notify\n"
                     ".equ __nexus_os_size_notify,%c4\n"
                     ".global __nexus_os_size_direct_notify\n"
                     ".equ __nexus_os_size_direct_notify,%c5\n"
                     ".global __nexus_os_size_raw_queue\n"
                     ".equ __nexus_os_size_raw_queue,%c6\n"
                     ".global __nexus_os_size_closable_queue\n"
                     ".equ __nexus_os_size_closable_queue,%c7\n"
                     ".global __nexus_os_size_queue_waiter\n"
                     ".equ __nexus_os_size_queue_waiter,%c8\n"
                     ".global __nexus_os_size_semaphore\n"
                     ".equ __nexus_os_size_semaphore,%c9\n"
                     ".global __nexus_os_size_stack_word\n"
                     ".equ __nexus_os_size_stack_word,%c10\n"
                     :
                     : "i"(sizeof(StaticTask_t)), "i"(sizeof(nx_wait_port_t)),
                       "i"(sizeof(nx_freertos_task_t)),
                       "i"(sizeof(nx_freertos_permanent_task_t)),
                       "i"(sizeof(nx_freertos_notify_t)),
                       "i"(sizeof(nx_freertos_direct_notify_t)),
                       "i"(sizeof(nx_freertos_queue_t)),
                       "i"(sizeof(nx_freertos_closable_queue_t)),
                       "i"(sizeof(nx_freertos_queue_waiter_t)),
                       "i"(sizeof(StaticSemaphore_t)),
                       "i"(sizeof(StackType_t)));
}

/** \brief A retained link consumer, not executable SoC/Board startup. */
void _start(void) {
    object_size_symbols();
    result_sink += (uint32_t)nx_freertos_notify_init(&nx_os_notify__bytes);
    nx_os_wait_port__bytes = nx_freertos_notify_port(&nx_os_notify__bytes);
    result_sink +=
        (uint32_t)nx_os_wait_port__bytes.arm(nx_os_wait_port__bytes.context);
    result_sink +=
        (uint32_t)nx_os_wait_port__bytes.wake(nx_os_wait_port__bytes.context);
    result_sink +=
        (uint32_t)nx_wait_until(&nx_os_wait_port__bytes, not_ready, NULL, 1);
    result_sink += (uint32_t)nx_freertos_notify_destroy(&nx_os_notify__bytes);
    result_sink += (uint32_t)nx_freertos_task_start(
        &nx_os_joinable_task__bytes, "join", nx_os_join_stack__bytes, 128, 1,
        returning_entry, NULL);
    result_sink +=
        (uint32_t)nx_freertos_task_join(&nx_os_joinable_task__bytes, 1);
    result_sink += (uint32_t)nx_freertos_permanent_task_start(
        &nx_os_permanent_task__bytes, "keep", nx_os_permanent_stack__bytes, 128,
        1, permanent_entry, NULL);
    result_sink += (uint32_t)nx_freertos_direct_notify_init(
        &nx_os_direct_notify__bytes, xTaskGetCurrentTaskHandle(), 0);
    nx_wait_port_t direct =
        nx_freertos_direct_notify_port(&nx_os_direct_notify__bytes);
    if (direct.wake != NULL) {
        result_sink += (uint32_t)direct.wake(direct.context);
        result_sink += (uint32_t)direct.wait(direct.context, 0, 1);
    }
    result_sink += (uint32_t)nx_freertos_direct_notify_destroy(
        &nx_os_direct_notify__bytes);
    result_sink += (uint32_t)nx_freertos_queue_init(
        &nx_os_raw_queue__bytes, (uint8_t*)nx_os_raw_payload__bytes,
        sizeof(nx_os_raw_payload__bytes), 3, sizeof(uint32_t));
    uint32_t item = result_sink;
    result_sink +=
        (uint32_t)nx_freertos_queue_send(&nx_os_raw_queue__bytes, &item, 1);
    result_sink +=
        (uint32_t)nx_freertos_queue_receive(&nx_os_raw_queue__bytes, &item, 1);
    result_sink += (uint32_t)nx_freertos_queue_send_until(
        &nx_os_raw_queue__bytes, &item, 1);
    result_sink += (uint32_t)nx_freertos_queue_receive_until(
        &nx_os_raw_queue__bytes, &item, 1);
    result_sink += (uint32_t)nx_freertos_queue_destroy(&nx_os_raw_queue__bytes);
    result_sink +=
        (uint32_t)nx_freertos_queue_waiter_init(&nx_os_queue_waiter__bytes);
    result_sink += (uint32_t)nx_freertos_closable_queue_init(
        &nx_os_closable_queue__bytes, (uint8_t*)nx_os_closable_payload__bytes,
        sizeof(nx_os_closable_payload__bytes), 3, sizeof(uint32_t), 1);
    result_sink += (uint32_t)nx_freertos_closable_queue_send_until(
        &nx_os_closable_queue__bytes, &item, 1, &nx_os_queue_waiter__bytes);
    result_sink += (uint32_t)nx_freertos_closable_queue_receive_until(
        &nx_os_closable_queue__bytes, &item, 1, &nx_os_queue_waiter__bytes);
    result_sink += (uint32_t)nx_freertos_closable_queue_close(
        &nx_os_closable_queue__bytes);
    result_sink += (uint32_t)nx_freertos_closable_queue_destroy(
        &nx_os_closable_queue__bytes);
    result_sink +=
        (uint32_t)nx_freertos_queue_waiter_destroy(&nx_os_queue_waiter__bytes);
    vTaskStartScheduler();
    for (;;) {
    }
}
