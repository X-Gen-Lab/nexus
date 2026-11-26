/**
 * \file            main.c
 * \brief           Retain real ThreadX services in the actual Board image
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/storage.h"
#include "nexus/os/threadx.h"
#include "nexus_bindings.h"
#include "nexus_config.h"
static nx_notify_t notification;
static nx_task_t returning;
static nx_permanent_task_t owner;
static nx_queue_t queue;
static nx_channel_t closable;
static nx_channel_waiter_t waiter;
static uint8_t queue_payload[6];
static uint8_t close_payload[6];
_Alignas(8) static uint8_t returning_stack[1024];
_Alignas(8) static uint8_t owner_stack[2048];
static volatile nx_result_t observed;

/** \brief A returning task is reclaimed only through an actual external join.
 */
static void return_entry(void* context) {
    (void)context;
}

/** \brief Retained calls run after the actual kernel seals bootstrap. */
static void owner_entry(void* context) {
    (void)context;
    uint8_t item[3] = {1, 2, 3};
    nx_wait_port_t wait = nx_notify_port(&notification);
    uint32_t sequence = wait.arm(wait.context);
    observed = wait.wake(wait.context);
    observed = wait.wait(wait.context, sequence, nx_time_now_us());
    observed = nx_queue_send_until(&queue, item, 0);
    observed = nx_queue_receive_until(&queue, item, 0);
    observed = nx_queue_destroy(&queue);
    observed = nx_channel_send_until(&closable, item, 0, &waiter);
    observed = nx_channel_close(&closable);
    observed = nx_channel_receive_until(&closable, item, 0, &waiter);
    observed = nx_channel_destroy(&closable);
    observed = nx_channel_waiter_destroy(&waiter);
    observed = nx_task_join_until(&returning, NX_DEADLINE_NEVER);
    observed = nx_notify_destroy(&notification);
    for (;;) {
        (void)tx_thread_sleep(1);
    }
}

/** \brief Product owns all objects and uses the adapter's proven init scope. */
static void startup(void* context) {
    (void)context;
    observed = nx_notify_init(&notification);
    observed = nx_queue_init(&queue, queue_payload, sizeof queue_payload, 2, 3);
    observed = nx_channel_init(&closable, close_payload, sizeof close_payload,
                               2, 3, 1);
    observed = nx_channel_waiter_init(&waiter);
    observed = nx_task_start(
        &returning, "returned", returning_stack, sizeof returning_stack,
        NX_OS_PRIORITY_LEVELS - 1U - (8), return_entry, NULL);
    observed = nx_permanent_task_start(
        &owner, "owner", owner_stack, sizeof owner_stack,
        NX_OS_PRIORITY_LEVELS - 1U - (16), owner_entry, NULL);
}

/** \brief This fixture establishes linking; it is never a physical test. */
int main(void) {
    nx_platform_start_result_t result = nx_platform_start();
    if (result.primary != NX_SUCCESS || result.cleanup != NX_SUCCESS ||
        result.remaining_effects != 0) {
        nx_threadx_fail_stop(NX_ERROR_IO);
        for (;;) {
        }
    }
    return (int)nx_os_run(startup, NULL);
}
