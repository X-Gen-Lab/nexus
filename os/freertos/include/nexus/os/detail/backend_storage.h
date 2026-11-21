/**
 * \file            backend_storage.h
 * \brief           Selected FreeRTOS layouts for explicit OS assembly storage
 * \author          Nexus Team
 * \note            Include through nexus/os/storage.h. Fields are private to
 *                  the backend while live; callers only provide their storage.
 */
#ifndef NEXUS_OS_FREERTOS_BACKEND_STORAGE_H
#define NEXUS_OS_FREERTOS_BACKEND_STORAGE_H
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

/** \brief Native stack elements preserve the selected port's actual layout. */
typedef StackType_t nx_os_stack_word_t;
#ifdef NEXUS_OS_STACK_ALIGNMENT_BYTES
#define NX_OS_STACK_ALIGNMENT NEXUS_OS_STACK_ALIGNMENT_BYTES
#else
#define NX_OS_STACK_ALIGNMENT portBYTE_ALIGNMENT
#endif
#define NX_OS_PRIORITY_LEVELS configMAX_PRIORITIES

/** \brief One static binary notification, independent of task storage. */
struct nx_notify {
    StaticSemaphore_t storage;
    SemaphoreHandle_t handle;
    uint32_t sequence;
    uint32_t waiting;
};

/** \brief Returning task with an explicit completion latch and native TCB. */
struct nx_task {
    StaticTask_t control;
    StaticSemaphore_t finished_storage;
    SemaphoreHandle_t finished;
    TaskHandle_t handle;
    void (*entry)(void*);
    void* context;
};

/** \brief Permanent task omits all join-completion storage. */
struct nx_permanent_task {
    StaticTask_t control;
    TaskHandle_t handle;
};

/** \brief Raw queue control; actual depth and item storage remain external. */
struct nx_queue {
    StaticQueue_t control;
    QueueHandle_t handle;
    uint32_t users;
};

/** \brief Exclusive per-operation latch for an explicitly selected channel. */
struct nx_channel_waiter {
    StaticSemaphore_t storage;
    SemaphoreHandle_t handle;
    struct nx_channel_waiter* next;
    uint32_t active;
};

/** \brief Channel adds only its selected close and waiter-list state. */
struct nx_channel {
    nx_queue_t queue;
    nx_channel_waiter_t* send_waiters;
    nx_channel_waiter_t* receive_waiters;
    uint32_t maximum_waiters;
    bool closed;
};
#endif /* NEXUS_OS_FREERTOS_BACKEND_STORAGE_H */
