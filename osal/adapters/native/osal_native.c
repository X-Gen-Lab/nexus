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

#include "osal/osal.h"
#include "osal/osal_internal.h"
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

/* macOS compatibility: Define PTHREAD_STACK_MIN if not available */
#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN 16384
#endif
#endif

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

/* Use configuration from osal_config.h for resource limits */
/* Local configuration for native-specific settings */
#define OSAL_QUEUE_MAX_SIZE 256
#define OSAL_TASK_NAME_MAX  32

/*---------------------------------------------------------------------------*/
/* Task Internal Structures                                                  */
/*---------------------------------------------------------------------------*/

typedef struct {
    osal_handle_header_t header; /**< Handle validation header */
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
    osal_handle_header_t header; /**< Handle validation header */
    bool used;
    bool locked;                 /**< Lock state for tracking */
    osal_task_internal_t* owner; /**< Owner task pointer */
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
    osal_handle_header_t header; /**< Handle validation header */
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
    osal_handle_header_t header; /**< Handle validation header */
    bool used;
    uint8_t* buffer;
    size_t item_size;
    size_t item_count;
    size_t head;
    size_t tail;
    size_t count;
    osal_queue_mode_t mode; /**< Queue mode (normal/overwrite) */
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
/* Diagnostics - Resource Statistics Tracking                                */
/*---------------------------------------------------------------------------*/

#if OSAL_STATS_ENABLE

/**
 * \brief           Resource statistics structure for internal tracking
 */
typedef struct {
    volatile uint16_t count;     /**< Current count */
    volatile uint16_t watermark; /**< Peak count (high watermark) */
} osal_resource_stats_internal_t;

/**
 * \brief           Global statistics context
 */
typedef struct {
    osal_resource_stats_internal_t tasks;   /**< Task statistics */
    osal_resource_stats_internal_t mutexes; /**< Mutex statistics */
    osal_resource_stats_internal_t sems;    /**< Semaphore statistics */
    osal_resource_stats_internal_t queues;  /**< Queue statistics */
    osal_resource_stats_internal_t events;  /**< Event flags statistics */
    osal_resource_stats_internal_t timers;  /**< Timer statistics */
} osal_global_stats_t;

/** \brief          Global statistics instance */
static osal_global_stats_t s_osal_stats = {0};

/**
 * \brief           Increment resource count and update watermark
 * \param[in]       stats: Pointer to statistics structure
 */
static inline void osal_stats_inc(osal_resource_stats_internal_t* stats) {
    global_lock();
    stats->count++;
    if (stats->count > stats->watermark) {
        stats->watermark = stats->count;
    }
    global_unlock();
}

/**
 * \brief           Decrement resource count
 * \param[in]       stats: Pointer to statistics structure
 */
static inline void osal_stats_dec(osal_resource_stats_internal_t* stats) {
    global_lock();
    if (stats->count > 0) {
        stats->count--;
    }
    global_unlock();
}

#endif /* OSAL_STATS_ENABLE */

/*---------------------------------------------------------------------------*/
/* Diagnostics - Error Callback                                              */
/*---------------------------------------------------------------------------*/

/** \brief          Registered error callback function */
static osal_error_callback_t s_error_callback = NULL;

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

    /* Initialize handle header for validation */
    OSAL_HANDLE_INIT(&task->header, OSAL_TYPE_TASK);

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
        if (stack_size < (size_t)PTHREAD_STACK_MIN) {
            stack_size = (size_t)PTHREAD_STACK_MIN;
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

#if OSAL_STATS_ENABLE
    /* Update task statistics */
    osal_stats_inc(&s_osal_stats.tasks);
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
        /* Validate handle using magic number and type check */
        OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_TASK);
    }

    if (!task->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#if OSAL_STATS_ENABLE
    /* Update task statistics */
    osal_stats_dec(&s_osal_stats.tasks);
#endif

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
    /* Invalidate handle header before marking as unused */
    OSAL_HANDLE_DEINIT(&task->header);
    task->used = false;
    global_unlock();

    return OSAL_OK;
}

osal_status_t osal_task_suspend(osal_task_handle_t handle) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_TASK);

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
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_TASK);

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

/**
 * \brief           Get task priority
 *
 * \details         Returns the current priority of the specified task.
 *
 * \note            Requirements: 9.1
 */
uint8_t osal_task_get_priority(osal_task_handle_t handle) {
    if (handle == NULL) {
        return 0;
    }

    osal_task_internal_t* task = (osal_task_internal_t*)handle;

    /* Validate handle - return 0 for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&task->header, OSAL_TYPE_TASK)) {
        return 0;
    }
#endif

    if (!task->used) {
        return 0;
    }

    return task->priority;
}

/**
 * \brief           Set task priority
 *
 * \details         Changes the priority of the specified task at runtime.
 *
 * \note            Requirements: 9.2
 */
osal_status_t osal_task_set_priority(osal_task_handle_t handle,
                                     uint8_t priority) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_TASK);

    /* Parameter validation - priority must be in valid range (0-31) */
    if (priority > 31) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_task_internal_t* task = (osal_task_internal_t*)handle;

    if (!task->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    global_lock();
    task->priority = priority;
    global_unlock();

    return OSAL_OK;
}

/**
 * \brief           Get task stack high watermark
 *
 * \details         Returns an estimated value for stack watermark.
 *                  Native platform does not have direct stack monitoring,
 *                  so this returns a placeholder value.
 *
 * \note            Requirements: 9.3
 */
size_t osal_task_get_stack_watermark(osal_task_handle_t handle) {
    if (handle == NULL) {
        return 0;
    }

    osal_task_internal_t* task = (osal_task_internal_t*)handle;

    /* Validate handle - return 0 for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&task->header, OSAL_TYPE_TASK)) {
        return 0;
    }
#endif

    if (!task->used) {
        return 0;
    }

    /*
     * Native platform (pthreads/Windows threads) does not provide
     * direct stack watermark monitoring. Return a placeholder value
     * indicating stack monitoring is not available.
     * A value of SIZE_MAX indicates "unknown/not available".
     */
    return SIZE_MAX;
}

/**
 * \brief           Get task state
 *
 * \details         Returns the current state of the specified task.
 *
 * \note            Requirements: 9.4
 */
osal_task_state_t osal_task_get_state(osal_task_handle_t handle) {
    if (handle == NULL) {
        return OSAL_TASK_STATE_DELETED;
    }

    osal_task_internal_t* task = (osal_task_internal_t*)handle;

    /* Validate handle - return DELETED for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&task->header, OSAL_TYPE_TASK)) {
        return OSAL_TASK_STATE_DELETED;
    }
#endif

    if (!task->used) {
        return OSAL_TASK_STATE_DELETED;
    }

    if (task->delete_pending) {
        return OSAL_TASK_STATE_DELETED;
    }

    if (task->suspended) {
        return OSAL_TASK_STATE_SUSPENDED;
    }

    if (task->running) {
        /* Check if this is the current task */
#ifdef _WIN32
        osal_task_internal_t* current =
            (osal_task_internal_t*)TlsGetValue(s_tls_index);
#else
        osal_task_internal_t* current =
            (osal_task_internal_t*)pthread_getspecific(s_tls_key);
#endif
        if (task == current) {
            return OSAL_TASK_STATE_RUNNING;
        }
        return OSAL_TASK_STATE_READY;
    }

    return OSAL_TASK_STATE_READY;
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

    /* Initialize handle header for validation */
    OSAL_HANDLE_INIT(&mutex->header, OSAL_TYPE_MUTEX);

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
    mutex->locked = false;
    mutex->owner = NULL;
    *handle = (osal_mutex_handle_t)mutex;

#if OSAL_STATS_ENABLE
    /* Update mutex statistics */
    osal_stats_inc(&s_osal_stats.mutexes);
#endif

    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_mutex_delete(osal_mutex_handle_t handle) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_MUTEX);

    osal_mutex_internal_t* mutex = (osal_mutex_internal_t*)handle;

    if (!mutex->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#if OSAL_STATS_ENABLE
    /* Update mutex statistics */
    osal_stats_dec(&s_osal_stats.mutexes);
#endif

    global_lock();

#ifdef _WIN32
    DeleteCriticalSection(&mutex->cs);
#else
    pthread_mutex_destroy(&mutex->mutex);
#endif

    /* Invalidate handle header before marking as unused */
    OSAL_HANDLE_DEINIT(&mutex->header);
    mutex->used = false;

    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_mutex_lock(osal_mutex_handle_t handle, uint32_t timeout_ms) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_MUTEX);

    osal_mutex_internal_t* mutex = (osal_mutex_internal_t*)handle;

    if (!mutex->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Get current task for owner tracking */
#ifdef _WIN32
    osal_task_internal_t* current_task =
        (osal_task_internal_t*)TlsGetValue(s_tls_index);
#else
    osal_task_internal_t* current_task =
        (osal_task_internal_t*)pthread_getspecific(s_tls_key);
#endif

#ifdef _WIN32
    if (timeout_ms == OSAL_WAIT_FOREVER) {
        EnterCriticalSection(&mutex->cs);
        mutex->locked = true;
        mutex->owner = current_task;
        return OSAL_OK;
    } else if (timeout_ms == OSAL_NO_WAIT) {
        if (TryEnterCriticalSection(&mutex->cs)) {
            mutex->locked = true;
            mutex->owner = current_task;
            return OSAL_OK;
        }
        return OSAL_ERROR_TIMEOUT;
    } else {
        /* Windows CRITICAL_SECTION doesn't support timeout directly */
        /* Use a polling approach with small sleeps */
        uint32_t elapsed = 0;
        while (elapsed < timeout_ms) {
            if (TryEnterCriticalSection(&mutex->cs)) {
                mutex->locked = true;
                mutex->owner = current_task;
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
        mutex->locked = true;
        mutex->owner = current_task;
        return OSAL_OK;
    } else if (timeout_ms == OSAL_NO_WAIT) {
        if (pthread_mutex_trylock(&mutex->mutex) == 0) {
            mutex->locked = true;
            mutex->owner = current_task;
            return OSAL_OK;
        }
        return OSAL_ERROR_TIMEOUT;
    } else {
#ifdef __APPLE__
        /* macOS doesn't support pthread_mutex_timedlock */
        /* Use polling approach with small sleeps */
        uint32_t elapsed = 0;
        while (elapsed < timeout_ms) {
            if (pthread_mutex_trylock(&mutex->mutex) == 0) {
                mutex->locked = true;
                mutex->owner = current_task;
                return OSAL_OK;
            }
            usleep(1000); /* Sleep 1ms */
            elapsed++;
        }
        return OSAL_ERROR_TIMEOUT;
#else
        struct timespec ts;
        ms_to_timespec(timeout_ms, &ts);

        int result = pthread_mutex_timedlock(&mutex->mutex, &ts);
        if (result == 0) {
            mutex->locked = true;
            mutex->owner = current_task;
            return OSAL_OK;
        } else if (result == ETIMEDOUT) {
            return OSAL_ERROR_TIMEOUT;
        }
        return OSAL_ERROR;
#endif
    }
#endif
}

osal_status_t osal_mutex_unlock(osal_mutex_handle_t handle) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_MUTEX);

    osal_mutex_internal_t* mutex = (osal_mutex_internal_t*)handle;

    if (!mutex->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Clear owner tracking before unlocking */
    mutex->locked = false;
    mutex->owner = NULL;

#ifdef _WIN32
    LeaveCriticalSection(&mutex->cs);
#else
    pthread_mutex_unlock(&mutex->mutex);
#endif

    return OSAL_OK;
}

/**
 * \brief           Get mutex owner task
 *
 * \details         Returns the task handle of the task that currently holds
 *                  the mutex.
 *
 * \note            Requirements: 10.1
 */
osal_task_handle_t osal_mutex_get_owner(osal_mutex_handle_t handle) {
    if (handle == NULL) {
        return NULL;
    }

    osal_mutex_internal_t* mutex = (osal_mutex_internal_t*)handle;

    /* Validate handle - return NULL for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&mutex->header, OSAL_TYPE_MUTEX)) {
        return NULL;
    }
#endif

    if (!mutex->used) {
        return NULL;
    }

    return (osal_task_handle_t)mutex->owner;
}

/**
 * \brief           Check if mutex is locked
 *
 * \details         Returns true if the mutex is currently held by any task.
 *
 * \note            Requirements: 10.2
 */
bool osal_mutex_is_locked(osal_mutex_handle_t handle) {
    if (handle == NULL) {
        return false;
    }

    osal_mutex_internal_t* mutex = (osal_mutex_internal_t*)handle;

    /* Validate handle - return false for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&mutex->header, OSAL_TYPE_MUTEX)) {
        return false;
    }
#endif

    if (!mutex->used) {
        return false;
    }

    return mutex->locked;
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

    /* Initialize handle header for validation */
    OSAL_HANDLE_INIT(&sem->header, OSAL_TYPE_SEM);

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

#if OSAL_STATS_ENABLE
    /* Update semaphore statistics */
    osal_stats_inc(&s_osal_stats.sems);
#endif

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
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_SEM);

    osal_sem_internal_t* sem = (osal_sem_internal_t*)handle;

    if (!sem->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#if OSAL_STATS_ENABLE
    /* Update semaphore statistics */
    osal_stats_dec(&s_osal_stats.sems);
#endif

    global_lock();

#ifdef _WIN32
    CloseHandle(sem->sem);
#else
    pthread_mutex_destroy(&sem->mutex);
    pthread_cond_destroy(&sem->cond);
#endif

    /* Invalidate handle header before marking as unused */
    OSAL_HANDLE_DEINIT(&sem->header);
    sem->used = false;

    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_sem_take(osal_sem_handle_t handle, uint32_t timeout_ms) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_SEM);

    osal_sem_internal_t* sem = (osal_sem_internal_t*)handle;

    if (!sem->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    DWORD wait_time = (timeout_ms == OSAL_WAIT_FOREVER) ? INFINITE : timeout_ms;
    DWORD result = WaitForSingleObject(sem->sem, wait_time);

    if (result == WAIT_OBJECT_0) {
        /* Decrement internal count tracking */
        if (sem->count > 0) {
            sem->count--;
        }
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
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_SEM);

    osal_sem_internal_t* sem = (osal_sem_internal_t*)handle;

    if (!sem->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    if (ReleaseSemaphore(sem->sem, 1, NULL)) {
        /* Increment internal count tracking (capped at max_count) */
        if (sem->count < sem->max_count) {
            sem->count++;
        }
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

/**
 * \brief           Get semaphore current count
 *
 * \details         Returns the current count of the semaphore.
 *
 * \note            Requirements: 10.3
 */
uint32_t osal_sem_get_count(osal_sem_handle_t handle) {
    if (handle == NULL) {
        return 0;
    }

    osal_sem_internal_t* sem = (osal_sem_internal_t*)handle;

    /* Validate handle - return 0 for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&sem->header, OSAL_TYPE_SEM)) {
        return 0;
    }
#endif

    if (!sem->used) {
        return 0;
    }

#ifdef _WIN32
    /*
     * Windows semaphores don't provide a direct way to query count.
     * We track the count internally in the structure.
     */
    return sem->count;
#else
    uint32_t count;
    pthread_mutex_lock(&sem->mutex);
    count = sem->count;
    pthread_mutex_unlock(&sem->mutex);
    return count;
#endif
}

/**
 * \brief           Reset semaphore to specified count
 *
 * \details         Resets the semaphore count to the specified value.
 *
 * \note            Requirements: 10.4
 */
osal_status_t osal_sem_reset(osal_sem_handle_t handle, uint32_t count) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_SEM);

    osal_sem_internal_t* sem = (osal_sem_internal_t*)handle;

    if (!sem->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Validate count doesn't exceed max_count */
    if (count > sem->max_count) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    /*
     * Windows semaphores don't support direct reset.
     * We need to drain and refill the semaphore.
     */
    /* Drain all available counts */
    while (WaitForSingleObject(sem->sem, 0) == WAIT_OBJECT_0) {
        /* Keep taking until empty */
    }

    /* Give the semaphore 'count' times */
    for (uint32_t i = 0; i < count; i++) {
        ReleaseSemaphore(sem->sem, 1, NULL);
    }

    sem->count = count;
#else
    pthread_mutex_lock(&sem->mutex);
    sem->count = count;
    /* Wake up any waiting threads if count > 0 */
    if (count > 0) {
        pthread_cond_broadcast(&sem->cond);
    }
    pthread_mutex_unlock(&sem->mutex);
#endif

    return OSAL_OK;
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

    /* Initialize handle header for validation */
    OSAL_HANDLE_INIT(&queue->header, OSAL_TYPE_QUEUE);

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

#if OSAL_STATS_ENABLE
    /* Update queue statistics */
    osal_stats_inc(&s_osal_stats.queues);
#endif

    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_queue_delete(osal_queue_handle_t handle) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_QUEUE);

    osal_queue_internal_t* queue = (osal_queue_internal_t*)handle;

    if (!queue->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#if OSAL_STATS_ENABLE
    /* Update queue statistics */
    osal_stats_dec(&s_osal_stats.queues);
#endif

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

    /* Invalidate handle header before marking as unused */
    OSAL_HANDLE_DEINIT(&queue->header);
    queue->used = false;

    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_queue_send(osal_queue_handle_t handle, const void* item,
                              uint32_t timeout_ms) {
    if (item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_QUEUE);

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
    if (item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_QUEUE);

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
    if (item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_QUEUE);

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
    if (item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_QUEUE);

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

    /* Validate handle - return 0 for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&queue->header, OSAL_TYPE_QUEUE)) {
        return 0;
    }
#endif

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

    /* Validate handle - return true (empty) for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&queue->header, OSAL_TYPE_QUEUE)) {
        return true;
    }
#endif

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

    /* Validate handle - return false for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&queue->header, OSAL_TYPE_QUEUE)) {
        return false;
    }
#endif

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

/**
 * \brief           Get available space in queue
 *
 * \details         Returns the number of free slots in the queue.
 *
 * \note            Requirements: 8.1
 */
size_t osal_queue_get_available_space(osal_queue_handle_t handle) {
    if (handle == NULL) {
        return 0;
    }

    osal_queue_internal_t* queue = (osal_queue_internal_t*)handle;

    /* Validate handle - return 0 for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&queue->header, OSAL_TYPE_QUEUE)) {
        return 0;
    }
#endif

    if (!queue->used) {
        return 0;
    }

    size_t available;
#ifdef _WIN32
    EnterCriticalSection(&queue->cs);
    available = queue->item_count - queue->count;
    LeaveCriticalSection(&queue->cs);
#else
    pthread_mutex_lock(&queue->mutex);
    available = queue->item_count - queue->count;
    pthread_mutex_unlock(&queue->mutex);
#endif

    return available;
}

/**
 * \brief           Reset queue (clear all items)
 *
 * \details         Resets the queue to its empty state. All items in the
 *                  queue are discarded.
 *
 * \note            Requirements: 8.2
 */
osal_status_t osal_queue_reset(osal_queue_handle_t handle) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_QUEUE);

    osal_queue_internal_t* queue = (osal_queue_internal_t*)handle;

    if (!queue->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#ifdef _WIN32
    EnterCriticalSection(&queue->cs);
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    /* Signal that queue is no longer full (if anyone was waiting) */
    SetEvent(queue->not_full);
    LeaveCriticalSection(&queue->cs);
#else
    pthread_mutex_lock(&queue->mutex);
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    /* Signal that queue is no longer full (if anyone was waiting) */
    pthread_cond_broadcast(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
#endif

    return OSAL_OK;
}

/**
 * \brief           Set queue mode
 *
 * \details         Sets the queue operating mode. In native platform,
 *                  this function accepts the mode for API compatibility
 *                  but the actual overwrite behavior would need to be
 *                  implemented in the send function.
 *
 * \note            Requirements: 8.3
 */
osal_status_t osal_queue_set_mode(osal_queue_handle_t handle,
                                  osal_queue_mode_t mode) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_QUEUE);

    osal_queue_internal_t* queue = (osal_queue_internal_t*)handle;

    if (!queue->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Parameter validation - mode must be valid */
    if (mode != OSAL_QUEUE_MODE_NORMAL && mode != OSAL_QUEUE_MODE_OVERWRITE) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Store the mode in the queue structure */
    queue->mode = mode;

    return OSAL_OK;
}

/**
 * \brief           Peek item from queue in ISR context
 *
 * \details         In native platform, ISR context is the same as normal
 *                  context. This function peeks at the front item without
 *                  removing it.
 *
 * \note            Requirements: 8.5
 */
osal_status_t osal_queue_peek_from_isr(osal_queue_handle_t handle, void* item) {
    /* In native platform, ISR context is the same as normal context */
    return osal_queue_peek(handle, item);
}

/*---------------------------------------------------------------------------*/
/* Timer Internal Structures                                                 */
/*---------------------------------------------------------------------------*/

#define OSAL_TIMER_NAME_MAX 32

typedef struct {
    osal_handle_header_t header; /**< Handle validation header */
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

    /* Initialize handle header for validation */
    OSAL_HANDLE_INIT(&timer->header, OSAL_TYPE_TIMER);

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

#if OSAL_STATS_ENABLE
    /* Update timer statistics */
    osal_stats_inc(&s_osal_stats.timers);
#endif

    *handle = (osal_timer_handle_t)timer;
    global_unlock();

    return OSAL_OK;
}

osal_status_t osal_timer_delete(osal_timer_handle_t handle) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_TIMER);

    osal_timer_internal_t* timer = (osal_timer_internal_t*)handle;

    if (!timer->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#if OSAL_STATS_ENABLE
    /* Update timer statistics */
    osal_stats_dec(&s_osal_stats.timers);
#endif

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
    /* Invalidate handle header before marking as unused */
    OSAL_HANDLE_DEINIT(&timer->header);
    timer->used = false;
    global_unlock();

    return OSAL_OK;
}

osal_status_t osal_timer_start(osal_timer_handle_t handle) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_TIMER);

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
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_TIMER);

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
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_TIMER);

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
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_TIMER);

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

    /* Validate handle - return false for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&timer->header, OSAL_TYPE_TIMER)) {
        return false;
    }
#endif

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
/* Timer Enhanced Functions                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get timer remaining time
 *
 * \details         Returns the time remaining until the timer expires.
 *                  For native platform, this is an approximation since we
 *                  don't have precise tick-based timing.
 *
 * \note            Requirements: 5.1
 */
uint32_t osal_timer_get_remaining(osal_timer_handle_t handle) {
    if (handle == NULL) {
        return 0;
    }

    osal_timer_internal_t* timer = (osal_timer_internal_t*)handle;

    /* Validate handle - return 0 for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&timer->header, OSAL_TYPE_TIMER)) {
        return 0;
    }
#endif

    if (!timer->used) {
        return 0;
    }

    /* Check if timer is active */
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

    if (!active) {
        return 0;
    }

    /*
     * Native platform does not track precise remaining time.
     * Return the full period as an approximation.
     * A more accurate implementation would require tracking
     * the start time of the current period.
     */
    return timer->period_ms;
}

/**
 * \brief           Get timer configured period
 *
 * \details         Returns the period of the timer in milliseconds.
 *
 * \note            Requirements: 5.2
 */
uint32_t osal_timer_get_period(osal_timer_handle_t handle) {
    if (handle == NULL) {
        return 0;
    }

    osal_timer_internal_t* timer = (osal_timer_internal_t*)handle;

    /* Validate handle - return 0 for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&timer->header, OSAL_TYPE_TIMER)) {
        return 0;
    }
#endif

    if (!timer->used) {
        return 0;
    }

    return timer->period_ms;
}

/**
 * \brief           Set timer callback function
 *
 * \details         Changes the callback function and argument for a timer.
 *
 * \note            Requirements: 5.3
 */
osal_status_t osal_timer_set_callback(osal_timer_handle_t handle,
                                      osal_timer_callback_t callback,
                                      void* arg) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_TIMER);

    if (callback == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_timer_internal_t* timer = (osal_timer_internal_t*)handle;

    if (!timer->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Update the callback and argument */
#ifdef _WIN32
    EnterCriticalSection(&timer->cs);
    timer->callback = callback;
    timer->arg = arg;
    LeaveCriticalSection(&timer->cs);
#else
    pthread_mutex_lock(&timer->mutex);
    timer->callback = callback;
    timer->arg = arg;
    pthread_mutex_unlock(&timer->mutex);
#endif

    return OSAL_OK;
}

/*---------------------------------------------------------------------------*/
/* Event Flags Internal Structures                                           */
/*---------------------------------------------------------------------------*/

#define OSAL_EVENT_BITS_MASK 0x00FFFFFF /* 24-bit support */

typedef struct {
    osal_handle_header_t header; /**< Handle validation header */
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

    /* Initialize handle header for validation */
    OSAL_HANDLE_INIT(&event->header, OSAL_TYPE_EVENT);

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

#if OSAL_STATS_ENABLE
    /* Update event statistics */
    osal_stats_inc(&s_osal_stats.events);
#endif

    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_event_delete(osal_event_handle_t handle) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_EVENT);

    osal_event_internal_t* event = (osal_event_internal_t*)handle;

    if (!event->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

#if OSAL_STATS_ENABLE
    /* Update event statistics */
    osal_stats_dec(&s_osal_stats.events);
#endif

    global_lock();

#ifdef _WIN32
    CloseHandle(event->cond);
    DeleteCriticalSection(&event->cs);
#else
    pthread_mutex_destroy(&event->mutex);
    pthread_cond_destroy(&event->cond);
#endif

    /* Invalidate handle header before marking as unused */
    OSAL_HANDLE_DEINIT(&event->header);
    event->used = false;

    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_event_set(osal_event_handle_t handle,
                             osal_event_bits_t bits) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_EVENT);

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
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_EVENT);

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
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_EVENT);

    if (options == NULL) {
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

    /* Validate handle - return 0 for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&event->header, OSAL_TYPE_EVENT)) {
        return 0;
    }
#endif

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

/**
 * \brief           Clear event bits from ISR context
 *
 * \details         In native platform, ISR context is the same as normal
 *                  context, so this function simply delegates to
 * osal_event_clear().
 *
 * \note            Requirements: 7.2
 */
osal_status_t osal_event_clear_from_isr(osal_event_handle_t handle,
                                        osal_event_bits_t bits) {
    /* In native platform, ISR context is the same as normal context */
    return osal_event_clear(handle, bits);
}

/**
 * \brief           Synchronous set and wait operation
 *
 * \details         Atomically sets event bits and then waits for a different
 *                  set of bits to be set. This is useful for task rendezvous
 *                  synchronization patterns.
 *
 * \note            Requirements: 7.3
 */
osal_status_t osal_event_sync(osal_event_handle_t handle,
                              osal_event_bits_t set_bits,
                              osal_event_bits_t wait_bits,
                              const osal_event_wait_options_t* options,
                              osal_event_bits_t* bits_out) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_EVENT);

    if (options == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Parameter validation - zero bits mask check */
    if (set_bits == 0 || wait_bits == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_event_internal_t* event = (osal_event_internal_t*)handle;

    if (!event->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Mask bits to valid range */
    set_bits &= OSAL_EVENT_BITS_MASK;
    wait_bits &= OSAL_EVENT_BITS_MASK;

#ifdef _WIN32
    EnterCriticalSection(&event->cs);

    /* Set the bits first */
    event->bits |= set_bits;
    SetEvent(event->cond); /* Signal all waiting threads */

    /* Check if wait condition is already met */
    bool condition_met = ((event->bits & wait_bits) == wait_bits);

    if (condition_met) {
        /* Condition already met - clear the wait bits and return */
        osal_event_bits_t matched_bits = event->bits & wait_bits;
        event->bits &=
            ~wait_bits; /* xEventGroupSync clears wait bits on exit */

        if (event->bits == 0) {
            ResetEvent(event->cond);
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
        condition_met = ((event->bits & wait_bits) == wait_bits);
    }

    /* Condition met - clear the wait bits (xEventGroupSync behavior) */
    osal_event_bits_t matched_bits = event->bits & wait_bits;
    event->bits &= ~wait_bits;

    if (event->bits == 0) {
        ResetEvent(event->cond);
    }

    if (bits_out != NULL) {
        *bits_out = matched_bits;
    }

    LeaveCriticalSection(&event->cs);
    return OSAL_OK;
#else
    pthread_mutex_lock(&event->mutex);

    /* Set the bits first */
    event->bits |= set_bits;
    pthread_cond_broadcast(&event->cond); /* Wake all waiting threads */

    /* Check if wait condition is already met */
    bool condition_met = ((event->bits & wait_bits) == wait_bits);

    if (condition_met) {
        /* Condition already met - clear the wait bits and return */
        osal_event_bits_t matched_bits = event->bits & wait_bits;
        event->bits &=
            ~wait_bits; /* xEventGroupSync clears wait bits on exit */

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
            condition_met = ((event->bits & wait_bits) == wait_bits);
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
            condition_met = ((event->bits & wait_bits) == wait_bits);
        }
    }

    /* Condition met - clear the wait bits (xEventGroupSync behavior) */
    osal_event_bits_t matched_bits = event->bits & wait_bits;
    event->bits &= ~wait_bits;

    if (bits_out != NULL) {
        *bits_out = matched_bits;
    }

    pthread_mutex_unlock(&event->mutex);
    return OSAL_OK;
#endif
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

/**
 * \brief           Get active allocation count
 *
 * \details         Returns the number of active memory allocations.
 *                  This is tracked internally in the allocation list.
 *
 * \note            Requirements: 6.1
 */
size_t osal_mem_get_allocation_count(void) {
    mem_init_tracking();

    mem_lock();
    size_t count = s_mem_stats.allocation_count;
    mem_unlock();

    return count;
}

/**
 * \brief           Check heap integrity
 *
 * \details         Performs a basic integrity check on the tracked allocations.
 *                  Validates the linked list structure and statistics
 * consistency.
 *
 * \note            Requirements: 6.3
 */
osal_status_t osal_mem_check_integrity(void) {
    mem_init_tracking();

    mem_lock();

    /* Basic sanity checks */
    if (s_mem_stats.total_allocated > OSAL_NATIVE_HEAP_SIZE) {
        mem_unlock();
        return OSAL_ERROR;
    }

    if (s_mem_stats.peak_allocated > OSAL_NATIVE_HEAP_SIZE) {
        mem_unlock();
        return OSAL_ERROR;
    }

    if (s_mem_stats.total_allocated > s_mem_stats.peak_allocated) {
        /* Current allocation should never exceed peak */
        mem_unlock();
        return OSAL_ERROR;
    }

    /* Walk the allocation list and verify count */
    size_t counted = 0;
    size_t total_size = 0;
    osal_mem_header_t* current = s_mem_stats.alloc_list;

    while (current != NULL) {
        counted++;
        total_size += current->size;

        /* Check for list corruption (circular reference) */
        if (counted > s_mem_stats.allocation_count + 1) {
            mem_unlock();
            return OSAL_ERROR;
        }

        /* Verify prev/next consistency */
        if (current->next != NULL && current->next->prev != current) {
            mem_unlock();
            return OSAL_ERROR;
        }

        current = current->next;
    }

    /* Verify count matches */
    if (counted != s_mem_stats.allocation_count) {
        mem_unlock();
        return OSAL_ERROR;
    }

    /* Verify total size matches (approximately - aligned allocations may
     * differ) */
    /* Allow some tolerance for alignment overhead */
    if (total_size >
        s_mem_stats.total_allocated + (s_mem_stats.allocation_count * 64)) {
        mem_unlock();
        return OSAL_ERROR;
    }

    mem_unlock();
    return OSAL_OK;
}

/**
 * \brief           Free aligned memory
 *
 * \details         Frees memory that was allocated with
 * osal_mem_alloc_aligned(). Retrieves the original pointer stored in the header
 * and frees it.
 *
 * \note            Requirements: 6.4
 */
void osal_mem_free_aligned(void* ptr) {
    /* Safe to call with NULL - just return */
    if (ptr == NULL) {
        return;
    }

    /* Get header from user pointer */
    osal_mem_header_t* header = ((osal_mem_header_t*)ptr) - 1;

    /* Verify this is an aligned allocation */
    if (header->alignment == 0 || header->original_ptr == NULL) {
        /*
         * This doesn't look like an aligned allocation.
         * Fall back to regular free behavior for safety.
         */
        osal_mem_free(ptr);
        return;
    }

    /* Get the original pointer and untrack the allocation */
    void* original = header->original_ptr;
    mem_untrack_alloc(header);

    /* Free the original allocation */
    free(original);
}

/*---------------------------------------------------------------------------*/
/* Diagnostics Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get OSAL resource statistics
 *
 * \details         Retrieves current resource counts and watermarks for all
 *                  OSAL resource types. This function is safe to call from
 *                  any context.
 *
 * \note            Requirements: 2.1, 2.2, 2.3, 2.5
 */
osal_status_t osal_get_stats(osal_stats_t* stats) {
    /* Parameter validation - NULL pointer check */
    if (stats == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

#if OSAL_STATS_ENABLE
    /* Enter critical section to ensure consistent snapshot */
    global_lock();

    /* Copy current counts */
    stats->task_count = s_osal_stats.tasks.count;
    stats->mutex_count = s_osal_stats.mutexes.count;
    stats->sem_count = s_osal_stats.sems.count;
    stats->queue_count = s_osal_stats.queues.count;
    stats->event_count = s_osal_stats.events.count;
    stats->timer_count = s_osal_stats.timers.count;

    /* Copy watermarks */
    stats->task_watermark = s_osal_stats.tasks.watermark;
    stats->mutex_watermark = s_osal_stats.mutexes.watermark;
    stats->sem_watermark = s_osal_stats.sems.watermark;
    stats->queue_watermark = s_osal_stats.queues.watermark;
    stats->event_watermark = s_osal_stats.events.watermark;
    stats->timer_watermark = s_osal_stats.timers.watermark;

    /* Copy memory statistics from memory tracking */
    mem_init_tracking();
    mem_lock();
    stats->mem_allocated = s_mem_stats.total_allocated;
    stats->mem_peak = s_mem_stats.peak_allocated;
    stats->mem_alloc_count = s_mem_stats.allocation_count;
    mem_unlock();

    global_unlock();
#else
    /* Statistics disabled - return zeros */
    memset(stats, 0, sizeof(osal_stats_t));
#endif

    return OSAL_OK;
}

/**
 * \brief           Reset OSAL statistics watermarks
 *
 * \details         Resets all watermark values to current counts. This is
 *                  useful for monitoring peak usage over specific time periods.
 *                  This function is safe to call from any context.
 *
 * \note            Requirements: 2.3
 */
osal_status_t osal_reset_stats(void) {
#if OSAL_STATS_ENABLE
    /* Enter critical section to ensure atomic reset */
    global_lock();

    /* Reset watermarks to current counts */
    s_osal_stats.tasks.watermark = s_osal_stats.tasks.count;
    s_osal_stats.mutexes.watermark = s_osal_stats.mutexes.count;
    s_osal_stats.sems.watermark = s_osal_stats.sems.count;
    s_osal_stats.queues.watermark = s_osal_stats.queues.count;
    s_osal_stats.events.watermark = s_osal_stats.events.count;
    s_osal_stats.timers.watermark = s_osal_stats.timers.count;

    /* Reset memory peak to current allocation */
    mem_init_tracking();
    mem_lock();
    s_mem_stats.peak_allocated = s_mem_stats.total_allocated;
    mem_unlock();

    global_unlock();
#endif

    return OSAL_OK;
}

/**
 * \brief           Register error callback
 *
 * \details         Registers a callback function that will be invoked when
 *                  certain errors occur. Only one callback can be registered
 *                  at a time; registering a new callback replaces the previous
 *                  one. Pass NULL to disable the callback.
 *
 * \note            Requirements: 2.5
 */
osal_status_t osal_set_error_callback(osal_error_callback_t callback) {
    /* Enter critical section for atomic update */
    global_lock();
    s_error_callback = callback;
    global_unlock();

    return OSAL_OK;
}

/**
 * \brief           Get error callback
 *
 * \details         Returns the currently registered error callback function,
 *                  or NULL if no callback is registered.
 */
osal_error_callback_t osal_get_error_callback(void) {
    return s_error_callback;
}

/**
 * \brief           Report an error through the error callback
 *
 * \details         Invokes the registered error callback if one is set.
 *                  This function is intended for internal use by OSAL
 *                  implementations to report errors.
 *
 * \note            The callback may be invoked from any context, so it
 *                  should be kept short and should not block.
 */
void osal_report_error(osal_status_t error, const char* file, uint32_t line) {
    osal_error_callback_t callback = s_error_callback;

    if (callback != NULL) {
        callback(error, file, line);
    }
}
