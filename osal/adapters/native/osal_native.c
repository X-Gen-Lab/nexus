/**
 * \file            osal_native.c
 * \brief           OSAL Native Platform Adapter
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Native platform implementation of OSAL using pthreads.
 *                  Supports task management, mutexes, semaphores, and queues.
 */

/* Enable usleep and pthread on POSIX systems */
#ifndef _WIN32
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#endif

/* Disable MSVC deprecation warnings */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)
#endif

#include "osal/osal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#endif

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define OSAL_MAX_TASKS      16
#define OSAL_MAX_MUTEXES    16
#define OSAL_MAX_SEMS       16
#define OSAL_MAX_QUEUES     8
#define OSAL_QUEUE_MAX_SIZE 256
#define OSAL_TASK_NAME_MAX  32

/*---------------------------------------------------------------------------*/
/* Task Internal Structures                                                  */
/*---------------------------------------------------------------------------*/

typedef struct {
    bool used;
    bool running;
    bool suspended;
    bool delete_pending;
    char name[OSAL_TASK_NAME_MAX];
    osal_task_func_t func;
    void* arg;
    uint8_t priority;
#ifdef _WIN32
    HANDLE thread;
    HANDLE suspend_event;
#else
    pthread_t thread;
    pthread_mutex_t suspend_mutex;
    pthread_cond_t suspend_cond;
#endif
} osal_task_internal_t;

/*---------------------------------------------------------------------------*/
/* Mutex Internal Structures                                                 */
/*---------------------------------------------------------------------------*/

typedef struct {
    bool used;
#ifdef _WIN32
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t mutex;
#endif
} osal_mutex_internal_t;

/*---------------------------------------------------------------------------*/
/* Semaphore Internal Structures                                             */
/*---------------------------------------------------------------------------*/

typedef struct {
    bool used;
    uint32_t count;
    uint32_t max_count;
#ifdef _WIN32
    HANDLE sem;
#else
    pthread_mutex_t mutex;
    pthread_cond_t cond;
#endif
} osal_sem_internal_t;

/*---------------------------------------------------------------------------*/
/* Queue Internal Structures                                                 */
/*---------------------------------------------------------------------------*/

typedef struct {
    bool used;
    uint8_t* buffer;
    size_t item_size;
    size_t item_count;
    size_t head;
    size_t tail;
    size_t count;
#ifdef _WIN32
    CRITICAL_SECTION cs;
    HANDLE not_empty;
    HANDLE not_full;
#else
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
#endif
} osal_queue_internal_t;

/*---------------------------------------------------------------------------*/
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

static bool s_osal_initialized = false;
static bool s_osal_running = false;
static volatile uint32_t s_critical_nesting = 0;

static osal_task_internal_t s_tasks[OSAL_MAX_TASKS];
static osal_mutex_internal_t s_mutexes[OSAL_MAX_MUTEXES];
static osal_sem_internal_t s_sems[OSAL_MAX_SEMS];
static osal_queue_internal_t s_queues[OSAL_MAX_QUEUES];

#ifdef _WIN32
static CRITICAL_SECTION s_global_cs;
static DWORD s_tls_index = TLS_OUT_OF_INDEXES;
#else
static pthread_mutex_t s_global_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t s_tls_key;
static bool s_tls_key_created = false;
#endif

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

static void global_lock(void) {
#ifdef _WIN32
    EnterCriticalSection(&s_global_cs);
#else
    pthread_mutex_lock(&s_global_mutex);
#endif
}

static void global_unlock(void) {
#ifdef _WIN32
    LeaveCriticalSection(&s_global_cs);
#else
    pthread_mutex_unlock(&s_global_mutex);
#endif
}

#ifndef _WIN32
/**
 * \brief           Convert milliseconds to timespec for pthread_cond_timedwait
 */
static void ms_to_timespec(uint32_t ms, struct timespec* ts) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    uint64_t nsec = now.tv_nsec + (uint64_t)ms * 1000000ULL;
    ts->tv_sec = now.tv_sec + (time_t)(nsec / 1000000000ULL);
    ts->tv_nsec = (long)(nsec % 1000000000ULL);
}
#endif

/*---------------------------------------------------------------------------*/
/* OSAL Core Functions                                                       */
/*---------------------------------------------------------------------------*/

osal_status_t osal_init(void) {
    if (s_osal_initialized) {
        return OSAL_OK;
    }

#ifdef _WIN32
    InitializeCriticalSection(&s_global_cs);
    s_tls_index = TlsAlloc();
    if (s_tls_index == TLS_OUT_OF_INDEXES) {
        return OSAL_ERROR_NO_MEMORY;
    }
#else
    if (!s_tls_key_created) {
        if (pthread_key_create(&s_tls_key, NULL) != 0) {
            return OSAL_ERROR_NO_MEMORY;
        }
        s_tls_key_created = true;
    }
#endif

    /* Initialize all arrays */
    memset(s_tasks, 0, sizeof(s_tasks));
    memset(s_mutexes, 0, sizeof(s_mutexes));
    memset(s_sems, 0, sizeof(s_sems));
    memset(s_queues, 0, sizeof(s_queues));

    s_osal_initialized = true;
    return OSAL_OK;
}

void osal_start(void) {
    s_osal_running = true;

    /* In native platform, we don't have a real scheduler.
     * Just keep the main thread alive while tasks run. */
    while (s_osal_running) {
#ifdef _WIN32
        Sleep(100);
#else
        usleep(100000);
#endif
    }
}

bool osal_is_running(void) {
    return s_osal_running;
}

void osal_enter_critical(void) {
    global_lock();
    s_critical_nesting++;
}

void osal_exit_critical(void) {
    if (s_critical_nesting > 0) {
        s_critical_nesting--;
    }
    global_unlock();
}

bool osal_is_isr(void) {
    /* Native platform doesn't have ISR context */
    return false;
}

/*---------------------------------------------------------------------------*/
/* Task Functions                                                            */
/*---------------------------------------------------------------------------*/

#ifdef _WIN32
static unsigned __stdcall task_wrapper(void* arg) {
    osal_task_internal_t* task = (osal_task_internal_t*)arg;

    /* Store task pointer in TLS */
    TlsSetValue(s_tls_index, task);

    task->running = true;

    /* Wait for initial resume if suspended */
    while (task->suspended && !task->delete_pending) {
        WaitForSingleObject(task->suspend_event, INFINITE);
    }

    if (!task->delete_pending && task->func) {
        task->func(task->arg);
    }

    task->running = false;
    return 0;
}
#else
static void* task_wrapper(void* arg) {
    osal_task_internal_t* task = (osal_task_internal_t*)arg;

    /* Store task pointer in TLS */
    pthread_setspecific(s_tls_key, task);

    task->running = true;

    /* Check for suspend at start */
    pthread_mutex_lock(&task->suspend_mutex);
    while (task->suspended && !task->delete_pending) {
        pthread_cond_wait(&task->suspend_cond, &task->suspend_mutex);
    }
    pthread_mutex_unlock(&task->suspend_mutex);

    if (!task->delete_pending && task->func) {
        task->func(task->arg);
    }

    task->running = false;
    return NULL;
}
#endif

osal_status_t osal_task_create(const osal_task_config_t* config,
                               osal_task_handle_t* handle) {
    if (config == NULL || handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (config->func == NULL) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    if (config->priority > 31) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Auto-initialize if needed */
    if (!s_osal_initialized) {
        osal_status_t status = osal_init();
        if (status != OSAL_OK) {
            return status;
        }
    }

    global_lock();

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < OSAL_MAX_TASKS; i++) {
        if (!s_tasks[i].used) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }

    osal_task_internal_t* task = &s_tasks[slot];
    memset(task, 0, sizeof(*task));

    task->used = true;
    task->func = config->func;
    task->arg = config->arg;
    task->priority = config->priority;
    task->suspended = false;
    task->delete_pending = false;

    if (config->name != NULL) {
        strncpy(task->name, config->name, OSAL_TASK_NAME_MAX - 1);
        task->name[OSAL_TASK_NAME_MAX - 1] = '\0';
    } else {
        snprintf(task->name, OSAL_TASK_NAME_MAX, "task_%d", slot);
    }

#ifdef _WIN32
    task->suspend_event = CreateEvent(NULL, FALSE, TRUE, NULL);
    if (task->suspend_event == NULL) {
        task->used = false;
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }

    task->thread = (HANDLE)_beginthreadex(NULL, (unsigned)config->stack_size,
                                          task_wrapper, task, 0, NULL);
    if (task->thread == NULL) {
        CloseHandle(task->suspend_event);
        task->used = false;
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }
#else
    pthread_mutex_init(&task->suspend_mutex, NULL);
    pthread_cond_init(&task->suspend_cond, NULL);

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    if (config->stack_size > 0) {
        size_t stack_size = config->stack_size;
        /* Ensure minimum stack size */
        if (stack_size < PTHREAD_STACK_MIN) {
            stack_size = PTHREAD_STACK_MIN;
        }
        pthread_attr_setstacksize(&attr, stack_size);
    }

    int result = pthread_create(&task->thread, &attr, task_wrapper, task);
    pthread_attr_destroy(&attr);

    if (result != 0) {
        pthread_mutex_destroy(&task->suspend_mutex);
        pthread_cond_destroy(&task->suspend_cond);
        task->used = false;
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }
#endif

    *handle = (osal_task_handle_t)task;
    global_unlock();

    return OSAL_OK;
}

osal_status_t osal_task_delete(osal_task_handle_t handle) {
    osal_task_internal_t* task;

    if (handle == NULL) {
        /* Delete current task */
#ifdef _WIN32
        task = (osal_task_internal_t*)TlsGetValue(s_tls_index);
#else
        task = (osal_task_internal_t*)pthread_getspecific(s_tls_key);
#endif
        if (task == NULL) {
            return OSAL_ERROR_INVALID_PARAM;
        }
    } else {
        task = (osal_task_internal_t*)handle;
    }

    if (!task->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    global_lock();

    task->delete_pending = true;

    /* Wake up if suspended */
#ifdef _WIN32
    SetEvent(task->suspend_event);
#else
    pthread_mutex_lock(&task->suspend_mutex);
    task->suspended = false;
    pthread_cond_signal(&task->suspend_cond);
    pthread_mutex_unlock(&task->suspend_mutex);
#endif

    global_unlock();

    /* Wait for thread to finish if not deleting self */
#ifdef _WIN32
    osal_task_internal_t* current =
        (osal_task_internal_t*)TlsGetValue(s_tls_index);
#else
    osal_task_internal_t* current =
        (osal_task_internal_t*)pthread_getspecific(s_tls_key);
#endif

    if (task != current) {
#ifdef _WIN32
        WaitForSingleObject(task->thread, INFINITE);
        CloseHandle(task->thread);
        CloseHandle(task->suspend_event);
#else
        pthread_join(task->thread, NULL);
        pthread_mutex_destroy(&task->suspend_mutex);
        pthread_cond_destroy(&task->suspend_cond);
#endif
    }

    global_lock();
    task->used = false;
    global_unlock();

    return OSAL_OK;
}

osal_status_t osal_task_suspend(osal_task_handle_t handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_task_internal_t* task = (osal_task_internal_t*)handle;

    if (!task->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    ResetEvent(task->suspend_event);
    task->suspended = true;
#else
    pthread_mutex_lock(&task->suspend_mutex);
    task->suspended = true;
    pthread_mutex_unlock(&task->suspend_mutex);
#endif

    return OSAL_OK;
}

osal_status_t osal_task_resume(osal_task_handle_t handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_task_internal_t* task = (osal_task_internal_t*)handle;

    if (!task->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    task->suspended = false;
    SetEvent(task->suspend_event);
#else
    pthread_mutex_lock(&task->suspend_mutex);
    task->suspended = false;
    pthread_cond_signal(&task->suspend_cond);
    pthread_mutex_unlock(&task->suspend_mutex);
#endif

    return OSAL_OK;
}

osal_status_t osal_task_delay(uint32_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif

    /* Check if current task should be suspended */
#ifdef _WIN32
    osal_task_internal_t* task =
        (osal_task_internal_t*)TlsGetValue(s_tls_index);
    if (task != NULL && task->suspended) {
        WaitForSingleObject(task->suspend_event, INFINITE);
    }
#else
    osal_task_internal_t* task =
        (osal_task_internal_t*)pthread_getspecific(s_tls_key);
    if (task != NULL) {
        pthread_mutex_lock(&task->suspend_mutex);
        while (task->suspended && !task->delete_pending) {
            pthread_cond_wait(&task->suspend_cond, &task->suspend_mutex);
        }
        pthread_mutex_unlock(&task->suspend_mutex);
    }
#endif

    return OSAL_OK;
}

osal_status_t osal_task_yield(void) {
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
    return OSAL_OK;
}

osal_task_handle_t osal_task_get_current(void) {
#ifdef _WIN32
    return (osal_task_handle_t)TlsGetValue(s_tls_index);
#else
    return (osal_task_handle_t)pthread_getspecific(s_tls_key);
#endif
}

const char* osal_task_get_name(osal_task_handle_t handle) {
    if (handle == NULL) {
        return NULL;
    }

    osal_task_internal_t* task = (osal_task_internal_t*)handle;

    if (!task->used) {
        return NULL;
    }

    return task->name;
}

/*---------------------------------------------------------------------------*/
/* Mutex Functions                                                           */
/*---------------------------------------------------------------------------*/

osal_status_t osal_mutex_create(osal_mutex_handle_t* handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Auto-initialize if needed */
    if (!s_osal_initialized) {
        osal_status_t status = osal_init();
        if (status != OSAL_OK) {
            return status;
        }
    }

    global_lock();

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < OSAL_MAX_MUTEXES; i++) {
        if (!s_mutexes[i].used) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }

    osal_mutex_internal_t* mutex = &s_mutexes[slot];

#ifdef _WIN32
    InitializeCriticalSection(&mutex->cs);
#else
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
#endif

    mutex->used = true;
    *handle = (osal_mutex_handle_t)mutex;

    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_mutex_delete(osal_mutex_handle_t handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_mutex_internal_t* mutex = (osal_mutex_internal_t*)handle;

    if (!mutex->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    global_lock();

#ifdef _WIN32
    DeleteCriticalSection(&mutex->cs);
#else
    pthread_mutex_destroy(&mutex->mutex);
#endif

    mutex->used = false;

    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_mutex_lock(osal_mutex_handle_t handle, uint32_t timeout_ms) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_mutex_internal_t* mutex = (osal_mutex_internal_t*)handle;

    if (!mutex->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    if (timeout_ms == OSAL_WAIT_FOREVER) {
        EnterCriticalSection(&mutex->cs);
        return OSAL_OK;
    } else if (timeout_ms == OSAL_NO_WAIT) {
        if (TryEnterCriticalSection(&mutex->cs)) {
            return OSAL_OK;
        }
        return OSAL_ERROR_TIMEOUT;
    } else {
        /* Windows CRITICAL_SECTION doesn't support timeout directly */
        /* Use a polling approach with small sleeps */
        uint32_t elapsed = 0;
        while (elapsed < timeout_ms) {
            if (TryEnterCriticalSection(&mutex->cs)) {
                return OSAL_OK;
            }
            Sleep(1);
            elapsed++;
        }
        return OSAL_ERROR_TIMEOUT;
    }
#else
    if (timeout_ms == OSAL_WAIT_FOREVER) {
        pthread_mutex_lock(&mutex->mutex);
        return OSAL_OK;
    } else if (timeout_ms == OSAL_NO_WAIT) {
        if (pthread_mutex_trylock(&mutex->mutex) == 0) {
            return OSAL_OK;
        }
        return OSAL_ERROR_TIMEOUT;
    } else {
        struct timespec ts;
        ms_to_timespec(timeout_ms, &ts);

        int result = pthread_mutex_timedlock(&mutex->mutex, &ts);
        if (result == 0) {
            return OSAL_OK;
        } else if (result == ETIMEDOUT) {
            return OSAL_ERROR_TIMEOUT;
        }
        return OSAL_ERROR;
    }
#endif
}

osal_status_t osal_mutex_unlock(osal_mutex_handle_t handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_mutex_internal_t* mutex = (osal_mutex_internal_t*)handle;

    if (!mutex->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    LeaveCriticalSection(&mutex->cs);
#else
    pthread_mutex_unlock(&mutex->mutex);
#endif

    return OSAL_OK;
}

/*---------------------------------------------------------------------------*/
/* Semaphore Functions                                                       */
/*---------------------------------------------------------------------------*/

osal_status_t osal_sem_create(uint32_t initial_count, uint32_t max_count,
                              osal_sem_handle_t* handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (max_count == 0 || initial_count > max_count) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Auto-initialize if needed */
    if (!s_osal_initialized) {
        osal_status_t status = osal_init();
        if (status != OSAL_OK) {
            return status;
        }
    }

    global_lock();

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < OSAL_MAX_SEMS; i++) {
        if (!s_sems[i].used) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }

    osal_sem_internal_t* sem = &s_sems[slot];

    sem->count = initial_count;
    sem->max_count = max_count;

#ifdef _WIN32
    sem->sem = CreateSemaphore(NULL, initial_count, max_count, NULL);
    if (sem->sem == NULL) {
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }
#else
    pthread_mutex_init(&sem->mutex, NULL);
    pthread_cond_init(&sem->cond, NULL);
#endif

    sem->used = true;
    *handle = (osal_sem_handle_t)sem;

    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_sem_create_binary(uint32_t initial,
                                     osal_sem_handle_t* handle) {
    return osal_sem_create(initial ? 1 : 0, 1, handle);
}

osal_status_t osal_sem_create_counting(uint32_t max_count, uint32_t initial,
                                       osal_sem_handle_t* handle) {
    return osal_sem_create(initial, max_count, handle);
}

osal_status_t osal_sem_delete(osal_sem_handle_t handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_sem_internal_t* sem = (osal_sem_internal_t*)handle;

    if (!sem->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    global_lock();

#ifdef _WIN32
    CloseHandle(sem->sem);
#else
    pthread_mutex_destroy(&sem->mutex);
    pthread_cond_destroy(&sem->cond);
#endif

    sem->used = false;

    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_sem_take(osal_sem_handle_t handle, uint32_t timeout_ms) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_sem_internal_t* sem = (osal_sem_internal_t*)handle;

    if (!sem->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    DWORD wait_time = (timeout_ms == OSAL_WAIT_FOREVER) ? INFINITE : timeout_ms;
    DWORD result = WaitForSingleObject(sem->sem, wait_time);

    if (result == WAIT_OBJECT_0) {
        return OSAL_OK;
    } else if (result == WAIT_TIMEOUT) {
        return OSAL_ERROR_TIMEOUT;
    }
    return OSAL_ERROR;
#else
    pthread_mutex_lock(&sem->mutex);

    if (timeout_ms == OSAL_WAIT_FOREVER) {
        while (sem->count == 0) {
            pthread_cond_wait(&sem->cond, &sem->mutex);
        }
        sem->count--;
        pthread_mutex_unlock(&sem->mutex);
        return OSAL_OK;
    } else if (timeout_ms == OSAL_NO_WAIT) {
        if (sem->count > 0) {
            sem->count--;
            pthread_mutex_unlock(&sem->mutex);
            return OSAL_OK;
        }
        pthread_mutex_unlock(&sem->mutex);
        return OSAL_ERROR_TIMEOUT;
    } else {
        struct timespec ts;
        ms_to_timespec(timeout_ms, &ts);

        while (sem->count == 0) {
            int result = pthread_cond_timedwait(&sem->cond, &sem->mutex, &ts);
            if (result == ETIMEDOUT) {
                pthread_mutex_unlock(&sem->mutex);
                return OSAL_ERROR_TIMEOUT;
            }
        }
        sem->count--;
        pthread_mutex_unlock(&sem->mutex);
        return OSAL_OK;
    }
#endif
}

osal_status_t osal_sem_give(osal_sem_handle_t handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_sem_internal_t* sem = (osal_sem_internal_t*)handle;

    if (!sem->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    if (ReleaseSemaphore(sem->sem, 1, NULL)) {
        return OSAL_OK;
    }
    return OSAL_ERROR;
#else
    pthread_mutex_lock(&sem->mutex);

    if (sem->count < sem->max_count) {
        sem->count++;
        pthread_cond_signal(&sem->cond);
    }

    pthread_mutex_unlock(&sem->mutex);
    return OSAL_OK;
#endif
}

osal_status_t osal_sem_give_from_isr(osal_sem_handle_t handle) {
    /* In native platform, ISR context is the same as normal context */
    return osal_sem_give(handle);
}

/*---------------------------------------------------------------------------*/
/* Queue Functions                                                           */
/*---------------------------------------------------------------------------*/

osal_status_t osal_queue_create(size_t item_size, size_t item_count,
                                osal_queue_handle_t* handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (item_size == 0 || item_count == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Auto-initialize if needed */
    if (!s_osal_initialized) {
        osal_status_t status = osal_init();
        if (status != OSAL_OK) {
            return status;
        }
    }

    global_lock();

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < OSAL_MAX_QUEUES; i++) {
        if (!s_queues[i].used) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }

    osal_queue_internal_t* queue = &s_queues[slot];

    /* Allocate buffer */
    queue->buffer = (uint8_t*)malloc(item_size * item_count);
    if (queue->buffer == NULL) {
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }

    queue->item_size = item_size;
    queue->item_count = item_count;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;

#ifdef _WIN32
    InitializeCriticalSection(&queue->cs);
    queue->not_empty = CreateEvent(NULL, TRUE, FALSE, NULL);
    queue->not_full = CreateEvent(NULL, TRUE, TRUE, NULL);

    if (queue->not_empty == NULL || queue->not_full == NULL) {
        if (queue->not_empty)
            CloseHandle(queue->not_empty);
        if (queue->not_full)
            CloseHandle(queue->not_full);
        DeleteCriticalSection(&queue->cs);
        free(queue->buffer);
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }
#else
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
#endif

    queue->used = true;
    *handle = (osal_queue_handle_t)queue;

    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_queue_delete(osal_queue_handle_t handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_internal_t* queue = (osal_queue_internal_t*)handle;

    if (!queue->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    global_lock();

#ifdef _WIN32
    CloseHandle(queue->not_empty);
    CloseHandle(queue->not_full);
    DeleteCriticalSection(&queue->cs);
#else
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
#endif

    free(queue->buffer);
    queue->buffer = NULL;
    queue->used = false;

    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_queue_send(osal_queue_handle_t handle, const void* item,
                              uint32_t timeout_ms) {
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_internal_t* queue = (osal_queue_internal_t*)handle;

    if (!queue->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    EnterCriticalSection(&queue->cs);

    if (timeout_ms == OSAL_WAIT_FOREVER) {
        while (queue->count >= queue->item_count) {
            LeaveCriticalSection(&queue->cs);
            WaitForSingleObject(queue->not_full, INFINITE);
            EnterCriticalSection(&queue->cs);
        }
    } else if (timeout_ms == OSAL_NO_WAIT) {
        if (queue->count >= queue->item_count) {
            LeaveCriticalSection(&queue->cs);
            return OSAL_ERROR_FULL;
        }
    } else {
        DWORD start = GetTickCount();
        while (queue->count >= queue->item_count) {
            LeaveCriticalSection(&queue->cs);
            DWORD elapsed = GetTickCount() - start;
            if (elapsed >= timeout_ms) {
                return OSAL_ERROR_TIMEOUT;
            }
            WaitForSingleObject(queue->not_full, timeout_ms - elapsed);
            EnterCriticalSection(&queue->cs);
        }
    }

    /* Copy item to queue */
    memcpy(queue->buffer + (queue->tail * queue->item_size), item,
           queue->item_size);
    queue->tail = (queue->tail + 1) % queue->item_count;
    queue->count++;

    /* Signal not empty */
    SetEvent(queue->not_empty);
    if (queue->count >= queue->item_count) {
        ResetEvent(queue->not_full);
    }

    LeaveCriticalSection(&queue->cs);
    return OSAL_OK;
#else
    pthread_mutex_lock(&queue->mutex);

    if (timeout_ms == OSAL_WAIT_FOREVER) {
        while (queue->count >= queue->item_count) {
            pthread_cond_wait(&queue->not_full, &queue->mutex);
        }
    } else if (timeout_ms == OSAL_NO_WAIT) {
        if (queue->count >= queue->item_count) {
            pthread_mutex_unlock(&queue->mutex);
            return OSAL_ERROR_FULL;
        }
    } else {
        struct timespec ts;
        ms_to_timespec(timeout_ms, &ts);

        while (queue->count >= queue->item_count) {
            int result =
                pthread_cond_timedwait(&queue->not_full, &queue->mutex, &ts);
            if (result == ETIMEDOUT) {
                pthread_mutex_unlock(&queue->mutex);
                return OSAL_ERROR_TIMEOUT;
            }
        }
    }

    /* Copy item to queue */
    memcpy(queue->buffer + (queue->tail * queue->item_size), item,
           queue->item_size);
    queue->tail = (queue->tail + 1) % queue->item_count;
    queue->count++;

    /* Signal not empty */
    pthread_cond_signal(&queue->not_empty);

    pthread_mutex_unlock(&queue->mutex);
    return OSAL_OK;
#endif
}

osal_status_t osal_queue_send_front(osal_queue_handle_t handle,
                                    const void* item, uint32_t timeout_ms) {
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_internal_t* queue = (osal_queue_internal_t*)handle;

    if (!queue->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    EnterCriticalSection(&queue->cs);

    if (timeout_ms == OSAL_NO_WAIT) {
        if (queue->count >= queue->item_count) {
            LeaveCriticalSection(&queue->cs);
            return OSAL_ERROR_FULL;
        }
    } else {
        while (queue->count >= queue->item_count) {
            LeaveCriticalSection(&queue->cs);
            DWORD wait_time =
                (timeout_ms == OSAL_WAIT_FOREVER) ? INFINITE : timeout_ms;
            if (WaitForSingleObject(queue->not_full, wait_time) ==
                WAIT_TIMEOUT) {
                return OSAL_ERROR_TIMEOUT;
            }
            EnterCriticalSection(&queue->cs);
        }
    }

    /* Move head back and copy item */
    queue->head = (queue->head == 0) ? queue->item_count - 1 : queue->head - 1;
    memcpy(queue->buffer + (queue->head * queue->item_size), item,
           queue->item_size);
    queue->count++;

    SetEvent(queue->not_empty);
    if (queue->count >= queue->item_count) {
        ResetEvent(queue->not_full);
    }

    LeaveCriticalSection(&queue->cs);
    return OSAL_OK;
#else
    pthread_mutex_lock(&queue->mutex);

    if (timeout_ms == OSAL_NO_WAIT) {
        if (queue->count >= queue->item_count) {
            pthread_mutex_unlock(&queue->mutex);
            return OSAL_ERROR_FULL;
        }
    } else if (timeout_ms == OSAL_WAIT_FOREVER) {
        while (queue->count >= queue->item_count) {
            pthread_cond_wait(&queue->not_full, &queue->mutex);
        }
    } else {
        struct timespec ts;
        ms_to_timespec(timeout_ms, &ts);

        while (queue->count >= queue->item_count) {
            int result =
                pthread_cond_timedwait(&queue->not_full, &queue->mutex, &ts);
            if (result == ETIMEDOUT) {
                pthread_mutex_unlock(&queue->mutex);
                return OSAL_ERROR_TIMEOUT;
            }
        }
    }

    /* Move head back and copy item */
    queue->head = (queue->head == 0) ? queue->item_count - 1 : queue->head - 1;
    memcpy(queue->buffer + (queue->head * queue->item_size), item,
           queue->item_size);
    queue->count++;

    pthread_cond_signal(&queue->not_empty);

    pthread_mutex_unlock(&queue->mutex);
    return OSAL_OK;
#endif
}

osal_status_t osal_queue_receive(osal_queue_handle_t handle, void* item,
                                 uint32_t timeout_ms) {
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_internal_t* queue = (osal_queue_internal_t*)handle;

    if (!queue->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    EnterCriticalSection(&queue->cs);

    if (timeout_ms == OSAL_WAIT_FOREVER) {
        while (queue->count == 0) {
            LeaveCriticalSection(&queue->cs);
            WaitForSingleObject(queue->not_empty, INFINITE);
            EnterCriticalSection(&queue->cs);
        }
    } else if (timeout_ms == OSAL_NO_WAIT) {
        if (queue->count == 0) {
            LeaveCriticalSection(&queue->cs);
            return OSAL_ERROR_EMPTY;
        }
    } else {
        DWORD start = GetTickCount();
        while (queue->count == 0) {
            LeaveCriticalSection(&queue->cs);
            DWORD elapsed = GetTickCount() - start;
            if (elapsed >= timeout_ms) {
                return OSAL_ERROR_TIMEOUT;
            }
            WaitForSingleObject(queue->not_empty, timeout_ms - elapsed);
            EnterCriticalSection(&queue->cs);
        }
    }

    /* Copy item from queue */
    memcpy(item, queue->buffer + (queue->head * queue->item_size),
           queue->item_size);
    queue->head = (queue->head + 1) % queue->item_count;
    queue->count--;

    /* Signal not full */
    SetEvent(queue->not_full);
    if (queue->count == 0) {
        ResetEvent(queue->not_empty);
    }

    LeaveCriticalSection(&queue->cs);
    return OSAL_OK;
#else
    pthread_mutex_lock(&queue->mutex);

    if (timeout_ms == OSAL_WAIT_FOREVER) {
        while (queue->count == 0) {
            pthread_cond_wait(&queue->not_empty, &queue->mutex);
        }
    } else if (timeout_ms == OSAL_NO_WAIT) {
        if (queue->count == 0) {
            pthread_mutex_unlock(&queue->mutex);
            return OSAL_ERROR_EMPTY;
        }
    } else {
        struct timespec ts;
        ms_to_timespec(timeout_ms, &ts);

        while (queue->count == 0) {
            int result =
                pthread_cond_timedwait(&queue->not_empty, &queue->mutex, &ts);
            if (result == ETIMEDOUT) {
                pthread_mutex_unlock(&queue->mutex);
                return OSAL_ERROR_TIMEOUT;
            }
        }
    }

    /* Copy item from queue */
    memcpy(item, queue->buffer + (queue->head * queue->item_size),
           queue->item_size);
    queue->head = (queue->head + 1) % queue->item_count;
    queue->count--;

    /* Signal not full */
    pthread_cond_signal(&queue->not_full);

    pthread_mutex_unlock(&queue->mutex);
    return OSAL_OK;
#endif
}

osal_status_t osal_queue_peek(osal_queue_handle_t handle, void* item) {
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_internal_t* queue = (osal_queue_internal_t*)handle;

    if (!queue->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    EnterCriticalSection(&queue->cs);

    if (queue->count == 0) {
        LeaveCriticalSection(&queue->cs);
        return OSAL_ERROR_EMPTY;
    }

    memcpy(item, queue->buffer + (queue->head * queue->item_size),
           queue->item_size);

    LeaveCriticalSection(&queue->cs);
    return OSAL_OK;
#else
    pthread_mutex_lock(&queue->mutex);

    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return OSAL_ERROR_EMPTY;
    }

    memcpy(item, queue->buffer + (queue->head * queue->item_size),
           queue->item_size);

    pthread_mutex_unlock(&queue->mutex);
    return OSAL_OK;
#endif
}

size_t osal_queue_get_count(osal_queue_handle_t handle) {
    if (handle == NULL) {
        return 0;
    }

    osal_queue_internal_t* queue = (osal_queue_internal_t*)handle;

    if (!queue->used) {
        return 0;
    }

    return queue->count;
}

bool osal_queue_is_empty(osal_queue_handle_t handle) {
    if (handle == NULL) {
        return true;
    }

    osal_queue_internal_t* queue = (osal_queue_internal_t*)handle;

    if (!queue->used) {
        return true;
    }

    return queue->count == 0;
}

bool osal_queue_is_full(osal_queue_handle_t handle) {
    if (handle == NULL) {
        return false;
    }

    osal_queue_internal_t* queue = (osal_queue_internal_t*)handle;

    if (!queue->used) {
        return false;
    }

    return queue->count >= queue->item_count;
}

osal_status_t osal_queue_send_from_isr(osal_queue_handle_t handle,
                                       const void* item) {
    /* In native platform, ISR context is the same as normal context */
    return osal_queue_send(handle, item, OSAL_NO_WAIT);
}

osal_status_t osal_queue_receive_from_isr(osal_queue_handle_t handle,
                                          void* item) {
    /* In native platform, ISR context is the same as normal context */
    return osal_queue_receive(handle, item, OSAL_NO_WAIT);
}

/*---------------------------------------------------------------------------*/
/* Timer Internal Structures                                                 */
/*---------------------------------------------------------------------------*/

#define OSAL_MAX_TIMERS     16
#define OSAL_TIMER_NAME_MAX 32

typedef struct {
    bool used;
    bool active;
    bool delete_pending;
    char name[OSAL_TIMER_NAME_MAX];
    uint32_t period_ms;
    osal_timer_mode_t mode;
    osal_timer_callback_t callback;
    void* arg;
#ifdef _WIN32
    HANDLE thread;
    HANDLE stop_event;
    HANDLE reset_event;
    CRITICAL_SECTION cs;
#else
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool stop_requested;
    bool reset_requested;
#endif
} osal_timer_internal_t;

static osal_timer_internal_t s_timers[OSAL_MAX_TIMERS];

/*---------------------------------------------------------------------------*/
/* Timer Helper Functions                                                    */
/*---------------------------------------------------------------------------*/

#ifdef _WIN32
static unsigned __stdcall timer_thread_func(void* arg) {
    osal_timer_internal_t* timer = (osal_timer_internal_t*)arg;

    while (!timer->delete_pending) {
        /* Wait for the timer period or stop/reset event */
        HANDLE events[2] = {timer->stop_event, timer->reset_event};
        DWORD result =
            WaitForMultipleObjects(2, events, FALSE, timer->period_ms);

        EnterCriticalSection(&timer->cs);

        if (timer->delete_pending) {
            LeaveCriticalSection(&timer->cs);
            break;
        }

        if (result == WAIT_OBJECT_0) {
            /* Stop event signaled - wait until started again */
            timer->active = false;
            LeaveCriticalSection(&timer->cs);

            /* Wait for reset event (which also acts as start) */
            while (!timer->delete_pending && !timer->active) {
                WaitForSingleObject(timer->reset_event, INFINITE);
                EnterCriticalSection(&timer->cs);
                if (timer->delete_pending) {
                    LeaveCriticalSection(&timer->cs);
                    break;
                }
                LeaveCriticalSection(&timer->cs);
            }
            continue;
        } else if (result == WAIT_OBJECT_0 + 1) {
            /* Reset event signaled - restart countdown */
            LeaveCriticalSection(&timer->cs);
            continue;
        } else if (result == WAIT_TIMEOUT) {
            /* Timer expired - invoke callback */
            if (timer->active && timer->callback != NULL) {
                osal_timer_callback_t cb = timer->callback;
                void* cb_arg = timer->arg;

                /* For one-shot timer, mark as inactive before callback */
                if (timer->mode == OSAL_TIMER_ONE_SHOT) {
                    timer->active = false;
                }

                LeaveCriticalSection(&timer->cs);

                /* Invoke callback outside of lock */
                cb(cb_arg);

                /* For one-shot timer, wait for restart */
                if (timer->mode == OSAL_TIMER_ONE_SHOT) {
                    EnterCriticalSection(&timer->cs);
                    while (!timer->delete_pending && !timer->active) {
                        LeaveCriticalSection(&timer->cs);
                        WaitForSingleObject(timer->reset_event, INFINITE);
                        EnterCriticalSection(&timer->cs);
                    }
                    LeaveCriticalSection(&timer->cs);
                }
                continue;
            }
        }

        LeaveCriticalSection(&timer->cs);
    }

    return 0;
}
#else
static void* timer_thread_func(void* arg) {
    osal_timer_internal_t* timer = (osal_timer_internal_t*)arg;

    pthread_mutex_lock(&timer->mutex);

    while (!timer->delete_pending) {
        if (!timer->active) {
            /* Wait until timer is started */
            while (!timer->active && !timer->delete_pending) {
                pthread_cond_wait(&timer->cond, &timer->mutex);
            }
            if (timer->delete_pending) {
                break;
            }
        }

        /* Calculate absolute timeout */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        uint64_t nsec = ts.tv_nsec + (uint64_t)timer->period_ms * 1000000ULL;
        ts.tv_sec += (time_t)(nsec / 1000000000ULL);
        ts.tv_nsec = (long)(nsec % 1000000000ULL);

        /* Wait for timeout or signal */
        timer->stop_requested = false;
        timer->reset_requested = false;

        int result = pthread_cond_timedwait(&timer->cond, &timer->mutex, &ts);

        if (timer->delete_pending) {
            break;
        }

        if (timer->stop_requested) {
            /* Stop requested - mark as inactive and wait */
            timer->active = false;
            continue;
        }

        if (timer->reset_requested) {
            /* Reset requested - restart countdown */
            continue;
        }

        if (result == ETIMEDOUT) {
            /* Timer expired - invoke callback */
            if (timer->active && timer->callback != NULL) {
                osal_timer_callback_t cb = timer->callback;
                void* cb_arg = timer->arg;

                /* For one-shot timer, mark as inactive before callback */
                if (timer->mode == OSAL_TIMER_ONE_SHOT) {
                    timer->active = false;
                }

                pthread_mutex_unlock(&timer->mutex);

                /* Invoke callback outside of lock */
                cb(cb_arg);

                pthread_mutex_lock(&timer->mutex);
            }
        }
    }

    pthread_mutex_unlock(&timer->mutex);
    return NULL;
}
#endif

/*---------------------------------------------------------------------------*/
/* Timer Functions                                                           */
/*---------------------------------------------------------------------------*/

osal_status_t osal_timer_create(const osal_timer_config_t* config,
                                osal_timer_handle_t* handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (config == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (config->callback == NULL) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    if (config->period_ms == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Auto-initialize if needed */
    if (!s_osal_initialized) {
        osal_status_t status = osal_init();
        if (status != OSAL_OK) {
            return status;
        }
    }

    global_lock();

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < OSAL_MAX_TIMERS; i++) {
        if (!s_timers[i].used) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }

    osal_timer_internal_t* timer = &s_timers[slot];
    memset(timer, 0, sizeof(*timer));

    timer->used = true;
    timer->active = false;
    timer->delete_pending = false;
    timer->period_ms = config->period_ms;
    timer->mode = config->mode;
    timer->callback = config->callback;
    timer->arg = config->arg;

    if (config->name != NULL) {
        strncpy(timer->name, config->name, OSAL_TIMER_NAME_MAX - 1);
        timer->name[OSAL_TIMER_NAME_MAX - 1] = '\0';
    } else {
        snprintf(timer->name, OSAL_TIMER_NAME_MAX, "timer_%d", slot);
    }

#ifdef _WIN32
    InitializeCriticalSection(&timer->cs);
    timer->stop_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    timer->reset_event = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (timer->stop_event == NULL || timer->reset_event == NULL) {
        if (timer->stop_event)
            CloseHandle(timer->stop_event);
        if (timer->reset_event)
            CloseHandle(timer->reset_event);
        DeleteCriticalSection(&timer->cs);
        timer->used = false;
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }

    timer->thread =
        (HANDLE)_beginthreadex(NULL, 0, timer_thread_func, timer, 0, NULL);
    if (timer->thread == NULL) {
        CloseHandle(timer->stop_event);
        CloseHandle(timer->reset_event);
        DeleteCriticalSection(&timer->cs);
        timer->used = false;
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }
#else
    pthread_mutex_init(&timer->mutex, NULL);
    pthread_cond_init(&timer->cond, NULL);
    timer->stop_requested = false;
    timer->reset_requested = false;

    int result = pthread_create(&timer->thread, NULL, timer_thread_func, timer);
    if (result != 0) {
        pthread_mutex_destroy(&timer->mutex);
        pthread_cond_destroy(&timer->cond);
        timer->used = false;
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }
#endif

    *handle = (osal_timer_handle_t)timer;
    global_unlock();

    return OSAL_OK;
}

osal_status_t osal_timer_delete(osal_timer_handle_t handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_timer_internal_t* timer = (osal_timer_internal_t*)handle;

    if (!timer->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    global_lock();

    timer->delete_pending = true;

#ifdef _WIN32
    /* Signal events to wake up the thread */
    SetEvent(timer->stop_event);
    SetEvent(timer->reset_event);

    global_unlock();

    /* Wait for thread to finish */
    WaitForSingleObject(timer->thread, INFINITE);

    /* Clean up resources */
    CloseHandle(timer->thread);
    CloseHandle(timer->stop_event);
    CloseHandle(timer->reset_event);
    DeleteCriticalSection(&timer->cs);
#else
    /* Signal condition to wake up the thread */
    pthread_mutex_lock(&timer->mutex);
    pthread_cond_signal(&timer->cond);
    pthread_mutex_unlock(&timer->mutex);

    global_unlock();

    /* Wait for thread to finish */
    pthread_join(timer->thread, NULL);

    /* Clean up resources */
    pthread_mutex_destroy(&timer->mutex);
    pthread_cond_destroy(&timer->cond);
#endif

    global_lock();
    timer->used = false;
    global_unlock();

    return OSAL_OK;
}

osal_status_t osal_timer_start(osal_timer_handle_t handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_timer_internal_t* timer = (osal_timer_internal_t*)handle;

    if (!timer->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    EnterCriticalSection(&timer->cs);
    timer->active = true;
    SetEvent(timer->reset_event); /* Signal to start/restart the timer */
    LeaveCriticalSection(&timer->cs);
#else
    pthread_mutex_lock(&timer->mutex);
    timer->active = true;
    timer->reset_requested = true;
    pthread_cond_signal(&timer->cond);
    pthread_mutex_unlock(&timer->mutex);
#endif

    return OSAL_OK;
}

osal_status_t osal_timer_stop(osal_timer_handle_t handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_timer_internal_t* timer = (osal_timer_internal_t*)handle;

    if (!timer->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    EnterCriticalSection(&timer->cs);
    timer->active = false;
    SetEvent(timer->stop_event);
    LeaveCriticalSection(&timer->cs);
#else
    pthread_mutex_lock(&timer->mutex);
    timer->active = false;
    timer->stop_requested = true;
    pthread_cond_signal(&timer->cond);
    pthread_mutex_unlock(&timer->mutex);
#endif

    return OSAL_OK;
}

osal_status_t osal_timer_reset(osal_timer_handle_t handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_timer_internal_t* timer = (osal_timer_internal_t*)handle;

    if (!timer->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    EnterCriticalSection(&timer->cs);
    timer->active = true; /* Reset also starts the timer if not running */
    SetEvent(timer->reset_event);
    LeaveCriticalSection(&timer->cs);
#else
    pthread_mutex_lock(&timer->mutex);
    timer->active = true; /* Reset also starts the timer if not running */
    timer->reset_requested = true;
    pthread_cond_signal(&timer->cond);
    pthread_mutex_unlock(&timer->mutex);
#endif

    return OSAL_OK;
}

osal_status_t osal_timer_set_period(osal_timer_handle_t handle,
                                    uint32_t period_ms) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (period_ms == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_timer_internal_t* timer = (osal_timer_internal_t*)handle;

    if (!timer->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    EnterCriticalSection(&timer->cs);
    timer->period_ms = period_ms;
    /* Signal reset to apply new period on next cycle */
    if (timer->active) {
        SetEvent(timer->reset_event);
    }
    LeaveCriticalSection(&timer->cs);
#else
    pthread_mutex_lock(&timer->mutex);
    timer->period_ms = period_ms;
    /* Signal reset to apply new period on next cycle */
    if (timer->active) {
        timer->reset_requested = true;
        pthread_cond_signal(&timer->cond);
    }
    pthread_mutex_unlock(&timer->mutex);
#endif

    return OSAL_OK;
}

bool osal_timer_is_active(osal_timer_handle_t handle) {
    if (handle == NULL) {
        return false;
    }

    osal_timer_internal_t* timer = (osal_timer_internal_t*)handle;

    if (!timer->used) {
        return false;
    }

    bool active;

#ifdef _WIN32
    EnterCriticalSection(&timer->cs);
    active = timer->active;
    LeaveCriticalSection(&timer->cs);
#else
    pthread_mutex_lock(&timer->mutex);
    active = timer->active;
    pthread_mutex_unlock(&timer->mutex);
#endif

    return active;
}

osal_status_t osal_timer_start_from_isr(osal_timer_handle_t handle) {
    /* In native platform, ISR context is the same as normal context */
    return osal_timer_start(handle);
}

osal_status_t osal_timer_stop_from_isr(osal_timer_handle_t handle) {
    /* In native platform, ISR context is the same as normal context */
    return osal_timer_stop(handle);
}

osal_status_t osal_timer_reset_from_isr(osal_timer_handle_t handle) {
    /* In native platform, ISR context is the same as normal context */
    return osal_timer_reset(handle);
}

/*---------------------------------------------------------------------------*/
/* Event Flags Internal Structures                                           */
/*---------------------------------------------------------------------------*/

#define OSAL_MAX_EVENTS      16
#define OSAL_EVENT_BITS_MASK 0x00FFFFFF /* 24-bit support */

typedef struct {
    bool used;
    osal_event_bits_t bits;
#ifdef _WIN32
    CRITICAL_SECTION cs;
    HANDLE cond; /* Manual-reset event for signaling */
#else
    pthread_mutex_t mutex;
    pthread_cond_t cond;
#endif
} osal_event_internal_t;

static osal_event_internal_t s_events[OSAL_MAX_EVENTS];

/*---------------------------------------------------------------------------*/
/* Event Flags Functions                                                     */
/*---------------------------------------------------------------------------*/

osal_status_t osal_event_create(osal_event_handle_t* handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Auto-initialize if needed */
    if (!s_osal_initialized) {
        osal_status_t status = osal_init();
        if (status != OSAL_OK) {
            return status;
        }
    }

    global_lock();

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < OSAL_MAX_EVENTS; i++) {
        if (!s_events[i].used) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }

    osal_event_internal_t* event = &s_events[slot];
    memset(event, 0, sizeof(*event));

    event->bits = 0;

#ifdef _WIN32
    InitializeCriticalSection(&event->cs);
    event->cond = CreateEvent(NULL, TRUE, FALSE, NULL); /* Manual-reset event */

    if (event->cond == NULL) {
        DeleteCriticalSection(&event->cs);
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }
#else
    pthread_mutex_init(&event->mutex, NULL);
    pthread_cond_init(&event->cond, NULL);
#endif

    event->used = true;
    *handle = (osal_event_handle_t)event;

    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_event_delete(osal_event_handle_t handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_event_internal_t* event = (osal_event_internal_t*)handle;

    if (!event->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    global_lock();

#ifdef _WIN32
    CloseHandle(event->cond);
    DeleteCriticalSection(&event->cs);
#else
    pthread_mutex_destroy(&event->mutex);
    pthread_cond_destroy(&event->cond);
#endif

    event->used = false;

    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_event_set(osal_event_handle_t handle,
                             osal_event_bits_t bits) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (bits == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_event_internal_t* event = (osal_event_internal_t*)handle;

    if (!event->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    EnterCriticalSection(&event->cs);
    event->bits |= (bits & OSAL_EVENT_BITS_MASK);
    SetEvent(event->cond); /* Signal all waiting threads */
    LeaveCriticalSection(&event->cs);
#else
    pthread_mutex_lock(&event->mutex);
    event->bits |= (bits & OSAL_EVENT_BITS_MASK);
    pthread_cond_broadcast(&event->cond); /* Wake all waiting threads */
    pthread_mutex_unlock(&event->mutex);
#endif

    return OSAL_OK;
}

osal_status_t osal_event_clear(osal_event_handle_t handle,
                               osal_event_bits_t bits) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (bits == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_event_internal_t* event = (osal_event_internal_t*)handle;

    if (!event->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    EnterCriticalSection(&event->cs);
    event->bits &= ~(bits & OSAL_EVENT_BITS_MASK);
    /* Reset event if no bits are set */
    if (event->bits == 0) {
        ResetEvent(event->cond);
    }
    LeaveCriticalSection(&event->cs);
#else
    pthread_mutex_lock(&event->mutex);
    event->bits &= ~(bits & OSAL_EVENT_BITS_MASK);
    pthread_mutex_unlock(&event->mutex);
#endif

    return OSAL_OK;
}

osal_status_t osal_event_wait(osal_event_handle_t handle,
                              osal_event_bits_t bits,
                              const osal_event_wait_options_t* options,
                              osal_event_bits_t* bits_out) {
    if (handle == NULL || options == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (bits == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_event_internal_t* event = (osal_event_internal_t*)handle;

    if (!event->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Mask bits to valid range */
    bits &= OSAL_EVENT_BITS_MASK;

#ifdef _WIN32
    EnterCriticalSection(&event->cs);

    /* Check if condition is already met */
    bool condition_met = false;
    if (options->mode == OSAL_EVENT_WAIT_ALL) {
        condition_met = ((event->bits & bits) == bits);
    } else {
        condition_met = ((event->bits & bits) != 0);
    }

    if (condition_met) {
        /* Condition already met - return immediately */
        osal_event_bits_t matched_bits = event->bits & bits;

        if (options->auto_clear) {
            event->bits &= ~matched_bits;
            if (event->bits == 0) {
                ResetEvent(event->cond);
            }
        }

        if (bits_out != NULL) {
            *bits_out = matched_bits;
        }

        LeaveCriticalSection(&event->cs);
        return OSAL_OK;
    }

    /* Need to wait */
    if (options->timeout_ms == OSAL_NO_WAIT) {
        LeaveCriticalSection(&event->cs);
        return OSAL_ERROR_TIMEOUT;
    }

    DWORD wait_time = (options->timeout_ms == OSAL_WAIT_FOREVER)
                          ? INFINITE
                          : options->timeout_ms;
    DWORD start_time = GetTickCount();

    while (!condition_met) {
        LeaveCriticalSection(&event->cs);

        /* Calculate remaining timeout */
        DWORD remaining = wait_time;
        if (wait_time != INFINITE) {
            DWORD elapsed = GetTickCount() - start_time;
            if (elapsed >= wait_time) {
                return OSAL_ERROR_TIMEOUT;
            }
            remaining = wait_time - elapsed;
        }

        DWORD result = WaitForSingleObject(event->cond, remaining);

        EnterCriticalSection(&event->cs);

        if (result == WAIT_TIMEOUT) {
            LeaveCriticalSection(&event->cs);
            return OSAL_ERROR_TIMEOUT;
        }

        /* Check condition again */
        if (options->mode == OSAL_EVENT_WAIT_ALL) {
            condition_met = ((event->bits & bits) == bits);
        } else {
            condition_met = ((event->bits & bits) != 0);
        }
    }

    /* Condition met */
    osal_event_bits_t matched_bits = event->bits & bits;

    if (options->auto_clear) {
        event->bits &= ~matched_bits;
        if (event->bits == 0) {
            ResetEvent(event->cond);
        }
    }

    if (bits_out != NULL) {
        *bits_out = matched_bits;
    }

    LeaveCriticalSection(&event->cs);
    return OSAL_OK;
#else
    pthread_mutex_lock(&event->mutex);

    /* Check if condition is already met */
    bool condition_met = false;
    if (options->mode == OSAL_EVENT_WAIT_ALL) {
        condition_met = ((event->bits & bits) == bits);
    } else {
        condition_met = ((event->bits & bits) != 0);
    }

    if (condition_met) {
        /* Condition already met - return immediately */
        osal_event_bits_t matched_bits = event->bits & bits;

        if (options->auto_clear) {
            event->bits &= ~matched_bits;
        }

        if (bits_out != NULL) {
            *bits_out = matched_bits;
        }

        pthread_mutex_unlock(&event->mutex);
        return OSAL_OK;
    }

    /* Need to wait */
    if (options->timeout_ms == OSAL_NO_WAIT) {
        pthread_mutex_unlock(&event->mutex);
        return OSAL_ERROR_TIMEOUT;
    }

    if (options->timeout_ms == OSAL_WAIT_FOREVER) {
        /* Wait forever */
        while (!condition_met) {
            pthread_cond_wait(&event->cond, &event->mutex);

            /* Check condition */
            if (options->mode == OSAL_EVENT_WAIT_ALL) {
                condition_met = ((event->bits & bits) == bits);
            } else {
                condition_met = ((event->bits & bits) != 0);
            }
        }
    } else {
        /* Wait with timeout */
        struct timespec ts;
        ms_to_timespec(options->timeout_ms, &ts);

        while (!condition_met) {
            int result =
                pthread_cond_timedwait(&event->cond, &event->mutex, &ts);

            if (result == ETIMEDOUT) {
                pthread_mutex_unlock(&event->mutex);
                return OSAL_ERROR_TIMEOUT;
            }

            /* Check condition */
            if (options->mode == OSAL_EVENT_WAIT_ALL) {
                condition_met = ((event->bits & bits) == bits);
            } else {
                condition_met = ((event->bits & bits) != 0);
            }
        }
    }

    /* Condition met */
    osal_event_bits_t matched_bits = event->bits & bits;

    if (options->auto_clear) {
        event->bits &= ~matched_bits;
    }

    if (bits_out != NULL) {
        *bits_out = matched_bits;
    }

    pthread_mutex_unlock(&event->mutex);
    return OSAL_OK;
#endif
}

osal_event_bits_t osal_event_get(osal_event_handle_t handle) {
    if (handle == NULL) {
        return 0;
    }

    osal_event_internal_t* event = (osal_event_internal_t*)handle;

    if (!event->used) {
        return 0;
    }

    osal_event_bits_t bits;

#ifdef _WIN32
    EnterCriticalSection(&event->cs);
    bits = event->bits;
    LeaveCriticalSection(&event->cs);
#else
    pthread_mutex_lock(&event->mutex);
    bits = event->bits;
    pthread_mutex_unlock(&event->mutex);
#endif

    return bits;
}

osal_status_t osal_event_set_from_isr(osal_event_handle_t handle,
                                      osal_event_bits_t bits) {
    /* In native platform, ISR context is the same as normal context */
    return osal_event_set(handle, bits);
}

/*---------------------------------------------------------------------------*/
/* Memory Management Internal Structures                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Memory allocation header for tracking
 * \details         Stores metadata about each allocation for statistics
 * tracking
 */
typedef struct osal_mem_header {
    size_t size;                  /**< Allocated size (excluding header) */
    size_t alignment;             /**< Alignment used (0 for normal alloc) */
    void* original_ptr;           /**< Original pointer (for aligned alloc) */
    struct osal_mem_header* next; /**< Next allocation in list */
    struct osal_mem_header* prev; /**< Previous allocation in list */
} osal_mem_header_t;

/**
 * \brief           Memory statistics tracking structure
 */
typedef struct {
    size_t total_allocated;  /**< Total bytes currently allocated */
    size_t peak_allocated;   /**< Peak bytes allocated (watermark) */
    size_t allocation_count; /**< Number of active allocations */
#ifdef _WIN32
    CRITICAL_SECTION cs; /**< Critical section for thread safety */
#else
    pthread_mutex_t mutex; /**< Mutex for thread safety */
#endif
    osal_mem_header_t* alloc_list; /**< Linked list of allocations */
    bool initialized; /**< Whether memory tracking is initialized */
} osal_mem_stats_internal_t;

/**
 * \brief           Simulated total heap size for native platform
 * \details         This is a simulated value since native platform uses system
 * heap
 */
#define OSAL_NATIVE_HEAP_SIZE (1024 * 1024) /* 1 MB simulated heap */

static osal_mem_stats_internal_t s_mem_stats = {0};

/*---------------------------------------------------------------------------*/
/* Memory Management Helper Functions                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize memory tracking if not already done
 */
static void mem_init_tracking(void) {
    if (s_mem_stats.initialized) {
        return;
    }

#ifdef _WIN32
    InitializeCriticalSection(&s_mem_stats.cs);
#else
    pthread_mutex_init(&s_mem_stats.mutex, NULL);
#endif

    s_mem_stats.total_allocated = 0;
    s_mem_stats.peak_allocated = 0;
    s_mem_stats.allocation_count = 0;
    s_mem_stats.alloc_list = NULL;
    s_mem_stats.initialized = true;
}

/**
 * \brief           Lock memory statistics mutex
 */
static void mem_lock(void) {
    mem_init_tracking();
#ifdef _WIN32
    EnterCriticalSection(&s_mem_stats.cs);
#else
    pthread_mutex_lock(&s_mem_stats.mutex);
#endif
}

/**
 * \brief           Unlock memory statistics mutex
 */
static void mem_unlock(void) {
#ifdef _WIN32
    LeaveCriticalSection(&s_mem_stats.cs);
#else
    pthread_mutex_unlock(&s_mem_stats.mutex);
#endif
}

/**
 * \brief           Track a new allocation
 * \param[in]       header: Pointer to allocation header
 * \param[in]       size: Size of allocation (excluding header)
 */
static void mem_track_alloc(osal_mem_header_t* header, size_t size) {
    header->size = size;
    header->alignment = 0;
    header->original_ptr = NULL;

    mem_lock();

    /* Add to linked list */
    header->next = s_mem_stats.alloc_list;
    header->prev = NULL;
    if (s_mem_stats.alloc_list != NULL) {
        s_mem_stats.alloc_list->prev = header;
    }
    s_mem_stats.alloc_list = header;

    /* Update statistics */
    s_mem_stats.total_allocated += size;
    s_mem_stats.allocation_count++;

    /* Update peak if necessary */
    if (s_mem_stats.total_allocated > s_mem_stats.peak_allocated) {
        s_mem_stats.peak_allocated = s_mem_stats.total_allocated;
    }

    mem_unlock();
}

/**
 * \brief           Untrack an allocation
 * \param[in]       header: Pointer to allocation header
 */
static void mem_untrack_alloc(osal_mem_header_t* header) {
    mem_lock();

    /* Remove from linked list */
    if (header->prev != NULL) {
        header->prev->next = header->next;
    } else {
        s_mem_stats.alloc_list = header->next;
    }
    if (header->next != NULL) {
        header->next->prev = header->prev;
    }

    /* Update statistics */
    s_mem_stats.total_allocated -= header->size;
    s_mem_stats.allocation_count--;

    mem_unlock();
}

/*---------------------------------------------------------------------------*/
/* Memory Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Allocate memory
 *
 * \details         Allocates memory from the system heap using malloc().
 *                  Includes tracking wrapper for statistics.
 *                  This function is thread-safe.
 *
 * \note            Requirements: 5.1-5.6
 */
void* osal_mem_alloc(size_t size) {
    /* Return NULL for zero size allocation */
    if (size == 0) {
        return NULL;
    }

    /* Allocate memory with header for tracking */
    size_t total_size = sizeof(osal_mem_header_t) + size;

    /* Check for overflow */
    if (total_size < size) {
        return NULL;
    }

    osal_mem_header_t* header = (osal_mem_header_t*)malloc(total_size);
    if (header == NULL) {
        return NULL;
    }

    /* Track the allocation */
    mem_track_alloc(header, size);

    /* Return pointer to user data (after header) */
    return (void*)(header + 1);
}

/**
 * \brief           Free memory
 *
 * \details         Frees memory back to the system heap using free().
 *                  This function is thread-safe and safe to call with NULL.
 *
 * \note            Requirements: 5.4, 5.5
 */
void osal_mem_free(void* ptr) {
    /* Safe to call with NULL - just return */
    if (ptr == NULL) {
        return;
    }

    /* Get header from user pointer */
    osal_mem_header_t* header = ((osal_mem_header_t*)ptr) - 1;

    /* Check if this is an aligned allocation */
    if (header->alignment != 0 && header->original_ptr != NULL) {
        /* For aligned allocations, free the original pointer */
        void* original = header->original_ptr;
        mem_untrack_alloc(header);
        free(original);
    } else {
        /* Normal allocation - untrack and free */
        mem_untrack_alloc(header);
        free(header);
    }
}

/**
 * \brief           Allocate and zero-initialize memory
 *
 * \details         Allocates memory from the system heap and initializes
 *                  all bytes to zero. Implemented using malloc() + memset().
 *                  This function is thread-safe.
 *
 * \note            Requirements: 6.1
 */
void* osal_mem_calloc(size_t count, size_t size) {
    /* Return NULL for zero count or size */
    if (count == 0 || size == 0) {
        return NULL;
    }

    /* Calculate total size with overflow check */
    size_t total_size = count * size;

    /* Check for multiplication overflow */
    if (total_size / count != size) {
        return NULL;
    }

    /* Allocate memory using our tracked allocator */
    void* ptr = osal_mem_alloc(total_size);
    if (ptr == NULL) {
        return NULL;
    }

    /* Zero-initialize the memory */
    memset(ptr, 0, total_size);

    return ptr;
}

/**
 * \brief           Reallocate memory
 *
 * \details         Reallocates memory, preserving the original data up to
 *                  the minimum of old and new sizes.
 *
 *                  Special cases:
 *                  - If ptr is NULL, behaves like osal_mem_alloc(size)
 *                  - If size is 0, frees the memory and returns NULL
 *
 * \note            Requirements: 6.2, 6.4, 6.5
 */
void* osal_mem_realloc(void* ptr, size_t size) {
    /* If ptr is NULL, behave like malloc */
    if (ptr == NULL) {
        return osal_mem_alloc(size);
    }

    /* If size is 0, free the memory and return NULL */
    if (size == 0) {
        osal_mem_free(ptr);
        return NULL;
    }

    /* Get header from user pointer */
    osal_mem_header_t* old_header = ((osal_mem_header_t*)ptr) - 1;
    size_t old_size = old_header->size;

    /* Allocate new memory block */
    void* new_ptr = osal_mem_alloc(size);
    if (new_ptr == NULL) {
        /* Allocation failed - original memory is unchanged */
        return NULL;
    }

    /* Copy data from old block to new block */
    size_t copy_size = (old_size < size) ? old_size : size;
    memcpy(new_ptr, ptr, copy_size);

    /* Free the old memory block */
    osal_mem_free(ptr);

    return new_ptr;
}

/**
 * \brief           Allocate aligned memory
 *
 * \details         Allocates memory with a specific alignment requirement.
 *                  Implemented by over-allocating and adjusting the returned
 * pointer.
 *
 *                  The implementation stores the original pointer in the header
 *                  so it can be freed correctly.
 *
 * \note            Requirements: 6.3
 */
void* osal_mem_alloc_aligned(size_t alignment, size_t size) {
    /* Return NULL for zero size */
    if (size == 0) {
        return NULL;
    }

    /* Validate alignment is a power of 2 */
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return NULL;
    }

    /* Ensure minimum alignment of pointer size */
    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }

    /*
     * Calculate total size needed:
     * - Header size
     * - Original size
     * - Extra space for alignment adjustment (alignment - 1 bytes max)
     */
    size_t total_size = sizeof(osal_mem_header_t) + size + alignment - 1;

    /* Check for overflow */
    if (total_size < size) {
        return NULL;
    }

    /* Allocate the memory block */
    void* raw_ptr = malloc(total_size);
    if (raw_ptr == NULL) {
        return NULL;
    }

    /*
     * Calculate aligned pointer:
     * 1. Start after the header
     * 2. Add (alignment - 1) and mask off low bits to align
     */
    uintptr_t raw_addr = (uintptr_t)raw_ptr + sizeof(osal_mem_header_t);
    uintptr_t aligned_addr = (raw_addr + alignment - 1) & ~(alignment - 1);

    /* Place header just before the aligned user data */
    osal_mem_header_t* header = (osal_mem_header_t*)(aligned_addr)-1;

    /* Track the allocation with alignment info */
    header->size = size;
    header->alignment = alignment;
    header->original_ptr = raw_ptr;

    mem_lock();

    /* Add to linked list */
    header->next = s_mem_stats.alloc_list;
    header->prev = NULL;
    if (s_mem_stats.alloc_list != NULL) {
        s_mem_stats.alloc_list->prev = header;
    }
    s_mem_stats.alloc_list = header;

    /* Update statistics */
    s_mem_stats.total_allocated += size;
    s_mem_stats.allocation_count++;

    /* Update peak if necessary */
    if (s_mem_stats.total_allocated > s_mem_stats.peak_allocated) {
        s_mem_stats.peak_allocated = s_mem_stats.total_allocated;
    }

    mem_unlock();

    return (void*)aligned_addr;
}

/**
 * \brief           Get memory statistics
 *
 * \details         Retrieves memory usage statistics from the tracking system.
 *
 * \note            Requirements: 7.1-7.4
 */
osal_status_t osal_mem_get_stats(osal_mem_stats_t* stats) {
    /* Parameter validation - NULL pointer check */
    if (stats == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    mem_init_tracking();

    mem_lock();

    /*
     * For native platform, we simulate a fixed heap size.
     * The free size is calculated as total - allocated.
     * The min_free_size is calculated from peak allocation.
     */
    stats->total_size = OSAL_NATIVE_HEAP_SIZE;
    stats->free_size = OSAL_NATIVE_HEAP_SIZE - s_mem_stats.total_allocated;
    stats->min_free_size = OSAL_NATIVE_HEAP_SIZE - s_mem_stats.peak_allocated;

    mem_unlock();

    return OSAL_OK;
}

/**
 * \brief           Get free heap size
 *
 * \details         Returns the current free heap size in bytes.
 *                  For native platform, this is simulated based on tracked
 * allocations.
 *
 * \note            Requirements: 7.2
 */
size_t osal_mem_get_free_size(void) {
    mem_init_tracking();

    mem_lock();
    size_t free_size = OSAL_NATIVE_HEAP_SIZE - s_mem_stats.total_allocated;
    mem_unlock();

    return free_size;
}

/**
 * \brief           Get minimum ever free heap size
 *
 * \details         Returns the minimum free heap size that has existed since
 *                  the system started. This is useful for detecting heap usage
 *                  high-water marks.
 *
 * \note            Requirements: 7.3
 */
size_t osal_mem_get_min_free_size(void) {
    mem_init_tracking();

    mem_lock();
    size_t min_free = OSAL_NATIVE_HEAP_SIZE - s_mem_stats.peak_allocated;
    mem_unlock();

    return min_free;
}
