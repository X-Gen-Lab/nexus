/**
 * \file            osal_native.c
 * \brief           OSAL Native Platform Adapter
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Native platform implementation of OSAL for host testing.
 *                  This is a minimal single-threaded implementation.
 */

#include "osal/osal.h"
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

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
    /* Native: no scheduler to start */
    while (1) {
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
    }
}

bool osal_is_running(void)
{
    return s_osal_initialized;
}

void osal_enter_critical(void)
{
    s_critical_nesting++;
}

void osal_exit_critical(void)
{
    if (s_critical_nesting > 0) {
        s_critical_nesting--;
    }
}

bool osal_is_isr(void)
{
    return false;  /* Native platform has no ISR context */
}

/*---------------------------------------------------------------------------*/
/* Task Management (Stub for native)                                         */
/*---------------------------------------------------------------------------*/

osal_status_t osal_task_create(const osal_task_config_t* config,
                               osal_task_handle_t* handle)
{
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
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
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

    if (mutex->locked) {
        return OSAL_ERROR_BUSY;
    }
    mutex->locked = true;

    return OSAL_OK;
}

osal_status_t osal_mutex_unlock(osal_mutex_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_mutex_t* mutex = (osal_mutex_t*)handle;
    mutex->locked = false;

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

    if (sem->count == 0) {
        return OSAL_ERROR_TIMEOUT;
    }
    sem->count--;

    return OSAL_OK;
}

osal_status_t osal_sem_give(osal_sem_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_sem_t* sem = (osal_sem_t*)handle;

    if (sem->count < sem->max_count) {
        sem->count++;
    }

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

    if (queue->count >= queue->item_count) {
        return OSAL_ERROR_FULL;
    }

    memcpy(&queue->buffer[queue->tail * queue->item_size],
           item, queue->item_size);
    queue->tail = (queue->tail + 1) % queue->item_count;
    queue->count++;

    return OSAL_OK;
}

osal_status_t osal_queue_send_front(osal_queue_handle_t handle,
                                    const void* item,
                                    uint32_t timeout_ms)
{
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

    if (queue->count == 0) {
        return OSAL_ERROR_EMPTY;
    }

    memcpy(item, &queue->buffer[queue->head * queue->item_size],
           queue->item_size);
    queue->head = (queue->head + 1) % queue->item_count;
    queue->count--;

    return OSAL_OK;
}

osal_status_t osal_queue_peek(osal_queue_handle_t handle, void* item)
{
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_t* queue = (osal_queue_t*)handle;

    if (queue->count == 0) {
        return OSAL_ERROR_EMPTY;
    }

    memcpy(item, &queue->buffer[queue->head * queue->item_size],
           queue->item_size);

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
