/** Backend capabilities and effective resource budgets. No kernel types. */
#ifndef OSAL_BACKEND_H
#define OSAL_BACKEND_H

#include "osal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OSAL_BACKEND_NATIVE,
    OSAL_BACKEND_BAREMETAL,
    OSAL_BACKEND_FREERTOS
} osal_backend_t;

enum {
    OSAL_CAP_TASKS = 1u << 0,
    OSAL_CAP_SOFTWARE_TIMERS = 1u << 1,
    OSAL_CAP_STATIC_OBJECTS = 1u << 2,
    OSAL_CAP_DYNAMIC_MEMORY = 1u << 3,
    OSAL_CAP_HARDWARE_ISR = 1u << 4,
    OSAL_CAP_PRIORITY_SCHEDULER = 1u << 5,
    OSAL_CAP_MONOTONIC_CLOCK = 1u << 6,
    OSAL_CAP_MEMORY_SEAL = 1u << 7
};

typedef enum {
    OSAL_DELETE_CANCELS_WAITERS,
    OSAL_DELETE_REQUIRES_IDLE
} osal_delete_policy_t;

typedef struct {
    osal_backend_t backend;
    uint32_t capabilities;
    osal_delete_policy_t delete_policy;
    uint16_t max_tasks, max_mutexes, max_semaphores;
    uint16_t max_queues, max_events, max_timers;
    size_t max_queue_item_bytes;
    size_t max_queue_storage_bytes;
    size_t max_task_stack_bytes; /**< 0: unsupported or host-managed. */
    size_t reserved_object_bytes; /**< Backend-owned static arrays only. */
    uint32_t event_bits_mask;
} osal_backend_info_t;

/** Nonblocking snapshot; accepts task or supported ISR context. Baremetal's
 * MONOTONIC_CLOCK bit appears only after the board installs its clock.
 * STATIC_OBJECTS excludes a POSIX port's internal host allocations. */
osal_status_t osal_get_backend_info(osal_backend_info_t* info);

#ifdef __cplusplus
}
#endif
#endif
