/**
 * \file            notify.c
 * \brief           Lock-free bare-metal sequence notification
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/baremetal.h"
#include <stddef.h>

/** \brief Snapshot published wake sequence. */
static uint32_t notify_arm(void* context) {
    nx_baremetal_notify_t* notification = context;
    return __atomic_load_n(&notification->sequence, __ATOMIC_ACQUIRE);
}

/** \brief Observe a wake or leave progress to the explicit owner loop. */
static nx_result_t notify_wait(void* context, uint32_t sequence,
                               uint64_t deadline_us) {
    nx_baremetal_notify_t* notification = context;
    if (notify_arm(context) != sequence) {
        return NX_SUCCESS;
    }
    return notification->now_us(notification->clock_context) >= deadline_us
               ? NX_ERROR_TIMEOUT
               : NX_ERROR_BUSY;
}

/** \brief Publish a wake without allocation or a completion queue. */
static nx_result_t notify_wake(void* context) {
    nx_baremetal_notify_t* notification = context;
    __atomic_fetch_add(&notification->sequence, 1, __ATOMIC_RELEASE);
    return NX_SUCCESS;
}

/** \brief Prepare an explicitly nonblocking backend. */
nx_result_t nx_baremetal_notify_init(nx_baremetal_notify_t* notification,
                                     uint64_t (*now_us)(void*),
                                     void* clock_context) {
    if (notification == NULL || now_us == NULL) {
        return NX_ERROR_INVALID;
    }
    notification->sequence = 0;
    if (!__atomic_always_lock_free(sizeof(notification->sequence), 0)) {
        return NX_ERROR_UNSUPPORTED;
    }
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
