/**
 * \file            osal_baremetal.c
 * \brief           OSAL Baremetal Adapter
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Simple baremetal implementation of OSAL.
 *                  This is a minimal implementation for single-threaded
 *                  applications without an RTOS.
 */

#include "osal/osal.h"
#include <string.h>

/**
 * \brief           OSAL initialization flag
 */
static bool s_osal_initialized = false;

/**
 * \brief           Critical section nesting counter
 */
static volatile uint32_t s_critical_nesting = 0;

/*---------------------------------------------------------------------------*/
/* OSAL Core                                                                 */
/*---------------------------------------------------------------------------*/

osal_status_t osal_init(void)
{
    if (s_osal_initialized) {
        return OSAL_OK;
    }

    s_critical_nesting = 0;
    s_osal_initialized = true;

    return OSAL_OK;
}

void osal_start(void)
{
    /* Baremetal: no scheduler to start */
    /* Main loop should be in application */
    while (1) {
        /* Idle */
    }
}

bool osal_is_running(void)
{
    return s_osal_initialized;
}

void osal_enter_critical(void)
{
    __asm volatile("cpsid i" ::: "memory");
    s_critical_nesting++;
}

void osal_exit_critical(void)
{
    if (s_critical_nesting > 0) {
        s_critical_nesting--;
        if (s_critical_nesting == 0) {
            __asm volatile("cpsie i" ::: "memory");
        }
    }
}

bool osal_is_isr(void)
{
    uint32_t ipsr;
    __asm volatile("mrs %0, ipsr" : "=r"(ipsr));
    return (ipsr != 0);
}

/*---------------------------------------------------------------------------*/
/* Task Management (Stub for baremetal)                                      */
/*---------------------------------------------------------------------------*/

osal_status_t osal_task_create(const osal_task_config_t* config,
                               osal_task_handle_t* handle)
{
    /* Baremetal: no task support */
    (void)config;
    (void)handle;
    return OSAL_ERROR;
}

osal_status_t osal_task_delete(osal_task_handle_t handle)
{
    (void)handle;
    return OSAL_ERROR;
}

osal_status_t osal_task_suspend(osal_task_handle_t handle)
{
    (void)handle;
    return OSAL_ERROR;
}

osal_status_t osal_task_resume(osal_task_handle_t handle)
{
    (void)handle;
    return OSAL_ERROR;
}

osal_status_t osal_task_delay(uint32_t ms)
{
    /* Simple busy-wait delay */
    volatile uint32_t count = ms * 1000;
    while (count--) {
        __asm volatile("nop");
    }
    return OSAL_OK;
}

osal_status_t osal_task_yield(void)
{
    return OSAL_OK;
}

osal_task_handle_t osal_task_get_current(void)
{
    return NULL;
}

const char* osal_task_get_name(osal_task_handle_t handle)
{
    (void)handle;
    return "main";
}

/*---------------------------------------------------------------------------*/
/* Mutex (Simple implementation)                                             */
/*---------------------------------------------------------------------------*/

typedef struct {
    bool locked;
} osal_mutex_t;

static osal_mutex_t s_mutexes[8];
static uint8_t s_mutex_count = 0;

osal_status_t osal_mutex_create(osal_mutex_handle_t* handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (s_mutex_count >= 8) {
        return OSAL_ERROR_NO_MEMORY;
    }

    osal_mutex_t* mutex = &s_mutexes[s_mutex_count++];
    mutex->locked = false;
    *handle = (osal_mutex_handle_t)mutex;

    return OSAL_OK;
}

osal_status_t osal_mutex_delete(osal_mutex_handle_t handle)
{
    (void)handle;
    return OSAL_OK;
}

osal_status_t osal_mutex_lock(osal_mutex_handle_t handle, uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_mutex_t* mutex = (osal_mutex_t*)handle;

    osal_enter_critical();
    if (mutex->locked) {
        osal_exit_critical();
        return OSAL_ERROR_BUSY;
    }
    mutex->locked = true;
    osal_exit_critical();

    return OSAL_OK;
}

osal_status_t osal_mutex_unlock(osal_mutex_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_mutex_t* mutex = (osal_mutex_t*)handle;

    osal_enter_critical();
    mutex->locked = false;
    osal_exit_critical();

    return OSAL_OK;
}

/*---------------------------------------------------------------------------*/
/* Semaphore (Simple implementation)                                         */
/*---------------------------------------------------------------------------*/

typedef struct {
    uint32_t count;
    uint32_t max_count;
} osal_sem_t;

static osal_sem_t s_sems[8];
static uint8_t s_sem_count = 0;

osal_status_t osal_sem_create_binary(uint32_t initial,
                                     osal_sem_handle_t* handle)
{
    return osal_sem_create_counting(1, initial, handle);
}

osal_status_t osal_sem_create_counting(uint32_t max_count,
                                       uint32_t initial,
                                       osal_sem_handle_t* handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (s_sem_count >= 8) {
        return OSAL_ERROR_NO_MEMORY;
    }

    osal_sem_t* sem = &s_sems[s_sem_count++];
    sem->count = initial;
    sem->max_count = max_count;
    *handle = (osal_sem_handle_t)sem;

    return OSAL_OK;
}

osal_status_t osal_sem_delete(osal_sem_handle_t handle)
{
    (void)handle;
    return OSAL_OK;
}

osal_status_t osal_sem_take(osal_sem_handle_t handle, uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_sem_t* sem = (osal_sem_t*)handle;

    osal_enter_critical();
    if (sem->count == 0) {
        osal_exit_critical();
        return OSAL_ERROR_TIMEOUT;
    }
    sem->count--;
    osal_exit_critical();

    return OSAL_OK;
}

osal_status_t osal_sem_give(osal_sem_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_sem_t* sem = (osal_sem_t*)handle;

    osal_enter_critical();
    if (sem->count < sem->max_count) {
        sem->count++;
    }
    osal_exit_critical();

    return OSAL_OK;
}

osal_status_t osal_sem_give_from_isr(osal_sem_handle_t handle)
{
    return osal_sem_give(handle);
}

/*---------------------------------------------------------------------------*/
/* Queue (Simple implementation)                                             */
/*---------------------------------------------------------------------------*/

#define OSAL_QUEUE_MAX_SIZE 256

typedef struct {
    uint8_t  buffer[OSAL_QUEUE_MAX_SIZE];
    size_t   item_size;
    size_t   item_count;
    size_t   head;
    size_t   tail;
    size_t   count;
} osal_queue_t;

static osal_queue_t s_queues[4];
static uint8_t s_queue_count = 0;

osal_status_t osal_queue_create(size_t item_size,
                                size_t item_count,
                                osal_queue_handle_t* handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (s_queue_count >= 4) {
        return OSAL_ERROR_NO_MEMORY;
    }

    if (item_size * item_count > OSAL_QUEUE_MAX_SIZE) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_queue_t* queue = &s_queues[s_queue_count++];
    queue->item_size = item_size;
    queue->item_count = item_count;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    *handle = (osal_queue_handle_t)queue;

    return OSAL_OK;
}

osal_status_t osal_queue_delete(osal_queue_handle_t handle)
{
    (void)handle;
    return OSAL_OK;
}

osal_status_t osal_queue_send(osal_queue_handle_t handle,
                              const void* item,
                              uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_t* queue = (osal_queue_t*)handle;

    osal_enter_critical();
    if (queue->count >= queue->item_count) {
        osal_exit_critical();
        return OSAL_ERROR_FULL;
    }

    memcpy(&queue->buffer[queue->tail * queue->item_size],
           item, queue->item_size);
    queue->tail = (queue->tail + 1) % queue->item_count;
    queue->count++;
    osal_exit_critical();

    return OSAL_OK;
}

osal_status_t osal_queue_send_front(osal_queue_handle_t handle,
                                    const void* item,
                                    uint32_t timeout_ms)
{
    /* Simplified: same as send */
    return osal_queue_send(handle, item, timeout_ms);
}

osal_status_t osal_queue_receive(osal_queue_handle_t handle,
                                 void* item,
                                 uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_t* queue = (osal_queue_t*)handle;

    osal_enter_critical();
    if (queue->count == 0) {
        osal_exit_critical();
        return OSAL_ERROR_EMPTY;
    }

    memcpy(item, &queue->buffer[queue->head * queue->item_size],
           queue->item_size);
    queue->head = (queue->head + 1) % queue->item_count;
    queue->count--;
    osal_exit_critical();

    return OSAL_OK;
}

osal_status_t osal_queue_peek(osal_queue_handle_t handle, void* item)
{
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_t* queue = (osal_queue_t*)handle;

    osal_enter_critical();
    if (queue->count == 0) {
        osal_exit_critical();
        return OSAL_ERROR_EMPTY;
    }

    memcpy(item, &queue->buffer[queue->head * queue->item_size],
           queue->item_size);
    osal_exit_critical();

    return OSAL_OK;
}

size_t osal_queue_get_count(osal_queue_handle_t handle)
{
    if (handle == NULL) {
        return 0;
    }

    osal_queue_t* queue = (osal_queue_t*)handle;
    return queue->count;
}

bool osal_queue_is_empty(osal_queue_handle_t handle)
{
    return osal_queue_get_count(handle) == 0;
}

bool osal_queue_is_full(osal_queue_handle_t handle)
{
    if (handle == NULL) {
        return true;
    }

    osal_queue_t* queue = (osal_queue_t*)handle;
    return queue->count >= queue->item_count;
}

osal_status_t osal_queue_send_from_isr(osal_queue_handle_t handle,
                                       const void* item)
{
    return osal_queue_send(handle, item, 0);
}

osal_status_t osal_queue_receive_from_isr(osal_queue_handle_t handle,
                                          void* item)
{
    return osal_queue_receive(handle, item, 0);
}
