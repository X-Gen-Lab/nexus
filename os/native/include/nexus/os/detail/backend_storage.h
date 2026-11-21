/**
 * \file            backend_storage.h
 * \brief           Selected Native object layouts for static assembly
 * \author          Nexus Team
 * \note            Include through nexus/os/storage.h. Zero-initialize every
 *                  object before first use. Fields are backend-owned; business
 *                  code keeps only the public opaque references.
 */
#ifndef NEXUS_OS_NATIVE_BACKEND_STORAGE_H
#define NEXUS_OS_NATIVE_BACKEND_STORAGE_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef unsigned char nx_os_stack_word_t;
#define NX_OS_STACK_ALIGNMENT 16U
#define NX_OS_PRIORITY_LEVELS 1U

/** \brief Returning pthread retains caller storage until successful join. */
struct nx_task {
    pthread_t thread;
    void (*entry)(void*);
    void* context;
    bool started;
};

/** \brief Permanent pthread has no join or completion object. */
struct nx_permanent_task {
    pthread_t thread;
    void (*entry)(void*);
    void* context;
    bool started;
};

/** \brief One waiter and multiple task publishers share a monotonic latch. */
struct nx_notify {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    uint32_t sequence;
    bool initialized;
    bool waiting;
    uint8_t resources;
};

/** \brief Bounded byte queue without close state or a waiter registry. */
struct nx_queue {
    pthread_mutex_t mutex;
    pthread_cond_t readable;
    pthread_cond_t writable;
    uint8_t* bytes;
    size_t depth;
    size_t item_size;
    size_t head;
    size_t count;
    size_t users;
    bool initialized;
    uint8_t resources;
};

/** \brief Close-aware operations own one live exclusive waiter each. */
struct nx_channel_waiter {
    uint32_t active;
    bool initialized;
};

/** \brief Optional close broadcast uses the queue's native conditions. */
struct nx_channel {
    struct nx_queue queue;
    size_t maximum_waiters;
    bool closed;
};

#endif /* NEXUS_OS_NATIVE_BACKEND_STORAGE_H */
