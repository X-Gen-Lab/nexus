/**
 * \file            notify.c
 * \brief           Bounded bare-metal sequence publication
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/atomic.h"
#include "nexus/os/baremetal.h"
#include <stddef.h>

/** \brief Snapshot published wake sequence. */
static uint32_t notify_arm(void* context) {
    nx_baremetal_notify_t* notification = context;
    return nx_atomic_u32_load_acquire(&notification->sequence);
}

/** \brief Observe a wake or leave progress to the explicit owner loop. */
static nx_result_t notify_wait(void* context, uint32_t sequence,
                               uint64_t deadline_us) {
    nx_baremetal_notify_t* notification = context;
    if (deadline_us != NX_DEADLINE_NEVER &&
        notification->now_us(notification->clock_context) >= deadline_us) {
        return NX_ERROR_TIMEOUT;
    }
    if (notify_arm(context) != sequence) {
        return NX_SUCCESS;
    }
    return NX_ERROR_BUSY;
}

/** \brief Publish a wake without allocation or a completion queue. */
static nx_result_t notify_wake(void* context) {
    nx_baremetal_notify_t* notification = context;
    (void)nx_atomic_u32_fetch_add_release(&notification->sequence, 1);
    return NX_SUCCESS;
}

/** \brief Prepare an explicitly nonblocking backend. */
nx_result_t nx_baremetal_notify_init(nx_baremetal_notify_t* notification,
                                     uint64_t (*now_us)(void*),
                                     void* clock_context) {
    if (notification == NULL || now_us == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_atomic_u32_store_relaxed(&notification->sequence, 0);
    notification->now_us = now_us;
    notification->clock_context = clock_context;
    return NX_SUCCESS;
}

/** \brief Expose only caller-selected notification storage. */
nx_wait_port_t nx_baremetal_notify_port(nx_baremetal_notify_t* notification) {
    if (notification == NULL || notification->now_us == NULL) {
        nx_wait_port_t invalid = {0};
        return invalid;
    }
    nx_wait_port_t port = {notification, notify_arm, notify_wait, notify_wake};
    return port;
}
