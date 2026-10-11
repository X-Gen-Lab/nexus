/**
 * \file            main.c
 * \brief           Software-only retained kernel profile and hook consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/freertos.h"
#include "nexus/os/tick.h"
#include "nexus_config.h"
#if NEXUS_OS_TRACE
#include "nexus/os/diagnostic.h"
#include "nexus/os/freertos_diagnostics.h"
#endif
#if NEXUS_OS_TICKLESS
#include "nexus/os/lowpower.h"
#endif

void _start(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

static void (*const exception_references[])(void)
    __attribute__((used, section(".runtime.references"))) = {
        SVC_Handler, PendSV_Handler, SysTick_Handler};
static volatile uint32_t observed_tick_hz;
static volatile uint32_t counter;
static nx_freertos_direct_notify_t direct;
static nx_freertos_permanent_task_t permanent_task;
static StackType_t task_stack[256];

static void permanent_entry(void* context) {
    (void)context;
    for (;;) {
    }
}

#if NEXUS_OS_TRACE
static nx_diagnostic_ring_t trace_ring;
static nx_diagnostic_event_t trace_events[8];

void nx_freertos_trace_event(nx_freertos_trace_kind_t event, uintptr_t identity,
                             uint32_t value) {
    nx_diagnostic_event_t record = {counter, identity, value, (uint32_t)event};
    (void)nx_diagnostic_ring_write(&trace_ring, &record);
}
#endif

#if NEXUS_OS_TICKLESS
static uint64_t alarm_deadline;

static uint64_t fixture_now_us(void* context) {
    (void)context;
    return counter;
}

static nx_result_t fixture_pause_tick(void* context,
                                      nx_freertos_tick_snapshot_t* snapshot) {
    (void)context;
    snapshot->time_us = counter;
    snapshot->phase_us = 0;
    return NX_SUCCESS;
}

static nx_result_t fixture_arm_deadline(void* context, uint64_t deadline_us) {
    (void)context;
    alarm_deadline = deadline_us;
    return NX_SUCCESS;
}

static void fixture_disarm(void* context) {
    (void)context;
    alarm_deadline = 0;
}

static void fixture_resume_tick(void* context, uint64_t next_tick_us) {
    (void)context;
    alarm_deadline = next_tick_us;
}

static const nx_freertos_lowpower_port_t lowpower_port = {NULL,
                                                          fixture_now_us,
                                                          fixture_pause_tick,
                                                          fixture_arm_deadline,
                                                          fixture_disarm,
                                                          fixture_resume_tick};

const nx_freertos_lowpower_port_t* nx_freertos_lowpower_port(void) {
    return &lowpower_port;
}
#endif

nx_time_us_t nx_time_now_us(void) {
    return counter;
}

#if defined(NEXUS_TEST_EXTERNAL_TICK_PROVIDER)
/* A link fixture records the requested frequency; it supplies no real timer. */
void nx_freertos_external_tick_setup(uint32_t tick_hz) {
    observed_tick_hz = tick_hz;
}
#endif

#if defined(NEXUS_TEST_RUNTIME_COUNTER_PROVIDER)
void nx_freertos_runtime_counter_start(void) {
    counter = 0u;
}

uint32_t nx_freertos_runtime_counter_now(void) {
    return ++counter;
}
#endif

void _start(void) {
    /* Absolute ELF symbols encode actual target-ABI sizeof without allocating
     * probe objects or changing the image's RAM footprint. */
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
                     :
                     : "i"(sizeof(StaticTask_t)), "i"(sizeof(nx_wait_port_t)),
                       "i"(sizeof(nx_freertos_task_t)),
                       "i"(sizeof(nx_freertos_permanent_task_t)),
                       "i"(sizeof(nx_freertos_notify_t)),
                       "i"(sizeof(nx_freertos_direct_notify_t)),
                       "i"(sizeof(nx_freertos_queue_t)),
                       "i"(sizeof(nx_freertos_closable_queue_t)),
                       "i"(sizeof(nx_freertos_queue_waiter_t)));
    observed_tick_hz = (uint32_t)configTICK_RATE_HZ;
#if NEXUS_OS_TRACE
    (void)nx_diagnostic_ring_init(&trace_ring, trace_events, 8u);
#endif
    (void)nx_freertos_direct_notify_init(&direct, xTaskGetCurrentTaskHandle(),
                                         0u);
    nx_wait_port_t port = nx_freertos_direct_notify_port(&direct);
    if (port.wake != NULL) {
        (void)port.wake(port.context);
    }
    if (port.wait != NULL) {
        (void)port.wait(port.context, 0u, 1u);
    }
    (void)nx_freertos_permanent_task_start(
        &permanent_task, "worker", task_stack, 256u, 0u, permanent_entry, NULL);
    vTaskStartScheduler();
    for (;;) {
    }
}
