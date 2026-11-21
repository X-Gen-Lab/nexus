/**
 * \file            backend_storage.h
 * \brief           Selected ThreadX object layouts for static assembly
 * \author          Nexus Team
 * \note            Include through nexus/os/storage.h. Zero-initialize every
 *                  object before first use. Fields are backend-owned; business
 *                  code retains only the public opaque references. Task TCBs
 *                  remain first so the real kernel identity finds its owner.
 */
#ifndef NEXUS_OS_THREADX_BACKEND_STORAGE_H
#define NEXUS_OS_THREADX_BACKEND_STORAGE_H

#include "tx_api.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef ULONG nx_os_stack_word_t;
#define NX_OS_STACK_ALIGNMENT 8U
#define NX_OS_PRIORITY_LEVELS TX_MAX_PRIORITIES

/** \brief One binary latch retains its storage through publisher quiescence. */
struct nx_notify {
    TX_SEMAPHORE control;
    uint32_t sequence;
    uint32_t waiting;
    bool live;
};

/** \brief Returning task remains owned until actual deletion and join. */
struct nx_task {
    TX_THREAD control;
    TX_SEMAPHORE finished;
    void (*entry)(void*);
    void* context;
    uint32_t entered;
    uint32_t returned;
    bool live;
};

/** \brief Permanent task has no completion semaphore or join state. */
struct nx_permanent_task {
    TX_THREAD control;
    void (*entry)(void*);
    void* context;
    bool live;
};

/** \brief Exact byte items use caller payload and two real capacity tokens. */
struct nx_queue {
    TX_SEMAPHORE readable;
    TX_SEMAPHORE writable;
    uint8_t* storage;
    size_t depth;
    size_t item_size;
    size_t head;
    size_t tail;
    size_t count;
    uint32_t users;
    bool live;
};

/** \brief One explicit operation latch belongs to one channel call at a time.
 */
struct nx_channel_waiter {
    struct nx_notify notification;
    struct nx_channel_waiter* next;
    uint32_t active;
};

/** \brief Optional close broadcast adds caller-sized registration state. */
struct nx_channel {
    struct nx_queue queue;
    struct nx_channel_waiter* send_waiters;
    struct nx_channel_waiter* receive_waiters;
    size_t maximum_waiters;
    bool closed;
};

#endif /* NEXUS_OS_THREADX_BACKEND_STORAGE_H */
