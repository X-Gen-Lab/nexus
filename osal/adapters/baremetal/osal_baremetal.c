/**
 * \file            osal_baremetal.c
 * \brief           OSAL Baremetal Adapter with Cooperative Scheduling
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Baremetal implementation of OSAL with simple cooperative
 *                  scheduling. This implementation is suitable for single-core
 *                  embedded systems without an RTOS.
 *
 *                  Features:
 *                  - Cooperative task scheduling (tasks must yield)
 *                  - Simple mutex implementation (non-blocking in single-thread)
 *                  - Counting semaphores
 *                  - FIFO message queues
 *
 *                  Limitations:
 *                  - No preemption (tasks must cooperate)
 *                  - No priority-based scheduling
 *                  - Limited number of tasks/mutexes/semaphores/queues
 */

#include "osal/osal.h"
#include <string.h>

/* Disable MSVC deprecation warnings for standard C functions */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable: 4996)
#endif

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define OSAL_MAX_TASKS          8
#define OSAL_MAX_MUTEXES        8
#define OSAL_MAX_SEMS           8
#define OSAL_MAX_QUEUES         4
#define OSAL_QUEUE_MAX_SIZE     256
#define OSAL_TASK_NAME_MAX      16

/*---------------------------------------------------------------------------*/
/* Platform-specific functions                                               */
/*---------------------------------------------------------------------------*/

/* Weak symbol support for different compilers */
#if defined(__GNUC__) || defined(__clang__)
    #define OSAL_WEAK __attribute__((weak))
#elif defined(_MSC_VER)
    /* MSVC doesn't support weak symbols directly */
    #define OSAL_WEAK
#else
    #define OSAL_WEAK
#endif

/**
 * \brief           Platform-specific microsecond delay
 * \param[in]       us: Microseconds to delay
 * \note            This is a weak symbol that can be overridden by platform
 */
OSAL_WEAK void osal_platform_delay_us(uint32_t us)
{
    /* Default busy-wait implementation */
    volatile uint32_t count = us;
    while (count--) {
#if defined(__GNUC__) || defined(__clang__)
        __asm volatile("nop");
#elif defined(_MSC_VER) && defined(_M_ARM)
        __nop();
#else
        /* Generic: just loop */
        (void)0;
#endif
    }
}

/**
 * \brief           Platform-specific critical section enter
 * \note            Weak symbol - override for specific MCU
 */
OSAL_WEAK void osal_platform_enter_critical(void)
{
#if defined(__ARM_ARCH) && (defined(__GNUC__) || defined(__clang__))
    __asm volatile("cpsid i" ::: "memory");
#elif defined(_MSC_VER) && defined(_M_ARM)
    __disable_irq();
#else
    /* Native/host platform: no-op */
    (void)0;
#endif
}

/**
 * \brief           Platform-specific critical section exit
 * \note            Weak symbol - override for specific MCU
 */
OSAL_WEAK void osal_platform_exit_critical(void)
{
#if defined(__ARM_ARCH) && (defined(__GNUC__) || defined(__clang__))
    __asm volatile("cpsie i" ::: "memory");
#elif defined(_MSC_VER) && defined(_M_ARM)
    __enable_irq();
#else
    /* Native/host platform: no-op */
    (void)0;
#endif
}

/**
 * \brief           Platform-specific ISR check
 * \return          true if in ISR context
 * \note            Weak symbol - override for specific MCU
 */
OSAL_WEAK bool osal_platform_is_isr(void)
{
#if defined(__ARM_ARCH) && (defined(__GNUC__) || defined(__clang__))
    uint32_t ipsr;
    __asm volatile("mrs %0, ipsr" : "=r"(ipsr));
    return (ipsr != 0);
#else
    /* Native/host platform: never in ISR */
    return false;
#endif
}

/*---------------------------------------------------------------------------*/
/* Task Internal Structures                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Task state enumeration
 */
typedef enum {
    TASK_STATE_UNUSED = 0,      /**< Slot not in use */
    TASK_STATE_READY,           /**< Task ready to run */
    TASK_STATE_RUNNING,         /**< Task currently running */
    TASK_STATE_SUSPENDED,       /**< Task suspended */
    TASK_STATE_BLOCKED,         /**< Task blocked (waiting) */
    TASK_STATE_DELETED          /**< Task marked for deletion */
} task_state_t;

/**
 * \brief           Task control block
 */
typedef struct {
    task_state_t        state;                      /**< Task state */
    char                name[OSAL_TASK_NAME_MAX];   /**< Task name */
    osal_task_func_t    func;                       /**< Task function */
    void*               arg;                        /**< Task argument */
    uint8_t             priority;                   /**< Task priority (0-31) */
    uint32_t            delay_ticks;                /**< Remaining delay ticks */
} osal_task_tcb_t;

/*---------------------------------------------------------------------------*/
/* Mutex Internal Structures                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Mutex control block
 */
typedef struct {
    bool                used;           /**< Slot in use flag */
    bool                locked;         /**< Lock state */
    osal_task_handle_t  owner;          /**< Owner task handle */
    uint32_t            lock_count;     /**< Recursive lock count */
} osal_mutex_cb_t;

/*---------------------------------------------------------------------------*/
/* Semaphore Internal Structures                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Semaphore control block
 */
typedef struct {
    bool                used;           /**< Slot in use flag */
    uint32_t            count;          /**< Current count */
    uint32_t            max_count;      /**< Maximum count */
} osal_sem_cb_t;

/*---------------------------------------------------------------------------*/
/* Queue Internal Structures                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Queue control block
 */
typedef struct {
    bool                used;           /**< Slot in use flag */
    uint8_t             buffer[OSAL_QUEUE_MAX_SIZE]; /**< Data buffer */
    size_t              item_size;      /**< Size of each item */
    size_t              item_count;     /**< Maximum number of items */
    size_t              head;           /**< Head index */
    size_t              tail;           /**< Tail index */
    size_t              count;          /**< Current item count */
} osal_queue_cb_t;

/*---------------------------------------------------------------------------*/
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

/** OSAL initialization flag */
static bool s_osal_initialized = false;

/** OSAL scheduler running flag */
static bool s_osal_running = false;

/** Critical section nesting counter */
static volatile uint32_t s_critical_nesting = 0;

/** Task control blocks */
static osal_task_tcb_t s_tasks[OSAL_MAX_TASKS];

/** Current running task index (-1 = none) */
static int s_current_task = -1;

/** Mutex control blocks */
static osal_mutex_cb_t s_mutexes[OSAL_MAX_MUTEXES];

/** Semaphore control blocks */
static osal_sem_cb_t s_sems[OSAL_MAX_SEMS];

/** Queue control blocks */
static osal_queue_cb_t s_queues[OSAL_MAX_QUEUES];

/** System tick counter (for delays) */
static volatile uint32_t s_tick_count = 0;

/*---------------------------------------------------------------------------*/
/* OSAL Core Functions                                                       */
/*---------------------------------------------------------------------------*/

osal_status_t osal_init(void)
{
    if (s_osal_initialized) {
        return OSAL_OK;
    }

    /* Initialize all control blocks */
    memset(s_tasks, 0, sizeof(s_tasks));
    memset(s_mutexes, 0, sizeof(s_mutexes));
    memset(s_sems, 0, sizeof(s_sems));
    memset(s_queues, 0, sizeof(s_queues));

    s_critical_nesting = 0;
    s_current_task = -1;
    s_tick_count = 0;
    s_osal_initialized = true;

    return OSAL_OK;
}

/**
 * \brief           Simple cooperative scheduler
 * \details         Runs tasks in round-robin fashion. Each task runs until
 *                  it yields or completes. This is a cooperative scheduler,
 *                  so tasks must call osal_task_yield() or osal_task_delay()
 *                  to allow other tasks to run.
 */
static void scheduler_run(void)
{
    while (s_osal_running) {
        bool task_ran = false;

        /* Find next ready task (round-robin) */
        for (int i = 0; i < OSAL_MAX_TASKS; i++) {
            int idx = (s_current_task + 1 + i) % OSAL_MAX_TASKS;
            osal_task_tcb_t* task = &s_tasks[idx];

            if (task->state == TASK_STATE_READY) {
                /* Run this task */
                s_current_task = idx;
                task->state = TASK_STATE_RUNNING;

                /* Execute task function */
                if (task->func != NULL) {
                    task->func(task->arg);
                }

                /* Task completed - mark as deleted */
                task->state = TASK_STATE_DELETED;
                task_ran = true;
                break;
            }
        }

        /* If no task ran, idle */
        if (!task_ran) {
            /* Small delay to prevent busy-waiting */
            osal_platform_delay_us(1000);
        }

        /* Update delay counters */
        s_tick_count++;
        for (int i = 0; i < OSAL_MAX_TASKS; i++) {
            if (s_tasks[i].state == TASK_STATE_BLOCKED && 
                s_tasks[i].delay_ticks > 0) {
                s_tasks[i].delay_ticks--;
                if (s_tasks[i].delay_ticks == 0) {
                    s_tasks[i].state = TASK_STATE_READY;
                }
            }
        }
    }
}

void osal_start(void)
{
    if (!s_osal_initialized) {
        osal_init();
    }

    s_osal_running = true;
    scheduler_run();
}

bool osal_is_running(void)
{
    return s_osal_running;
}

void osal_enter_critical(void)
{
    osal_platform_enter_critical();
    s_critical_nesting++;
}

void osal_exit_critical(void)
{
    if (s_critical_nesting > 0) {
        s_critical_nesting--;
        if (s_critical_nesting == 0) {
            osal_platform_exit_critical();
        }
    }
}

bool osal_is_isr(void)
{
    return osal_platform_is_isr();
}

/*---------------------------------------------------------------------------*/
/* Task Management Functions                                                 */
/*---------------------------------------------------------------------------*/

osal_status_t osal_task_create(const osal_task_config_t* config,
                               osal_task_handle_t* handle)
{
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
        osal_init();
    }

    osal_enter_critical();

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < OSAL_MAX_TASKS; i++) {
        if (s_tasks[i].state == TASK_STATE_UNUSED ||
            s_tasks[i].state == TASK_STATE_DELETED) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        osal_exit_critical();
        return OSAL_ERROR_NO_MEMORY;
    }

    /* Initialize task control block */
    osal_task_tcb_t* task = &s_tasks[slot];
    memset(task, 0, sizeof(*task));

    task->func = config->func;
    task->arg = config->arg;
    task->priority = config->priority;
    task->state = TASK_STATE_READY;
    task->delay_ticks = 0;

    if (config->name != NULL) {
        strncpy(task->name, config->name, OSAL_TASK_NAME_MAX - 1);
        task->name[OSAL_TASK_NAME_MAX - 1] = '\0';
    } else {
        task->name[0] = '\0';
    }

    *handle = (osal_task_handle_t)task;

    osal_exit_critical();
    return OSAL_OK;
}

osal_status_t osal_task_delete(osal_task_handle_t handle)
{
    osal_task_tcb_t* task;

    if (handle == NULL) {
        /* Delete current task */
        if (s_current_task >= 0) {
            task = &s_tasks[s_current_task];
        } else {
            return OSAL_ERROR_INVALID_PARAM;
        }
    } else {
        task = (osal_task_tcb_t*)handle;
    }

    /* Validate task pointer */
    bool valid = false;
    for (int i = 0; i < OSAL_MAX_TASKS; i++) {
        if (&s_tasks[i] == task) {
            valid = true;
            break;
        }
    }

    if (!valid || task->state == TASK_STATE_UNUSED) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_enter_critical();
    task->state = TASK_STATE_DELETED;
    osal_exit_critical();

    return OSAL_OK;
}

osal_status_t osal_task_suspend(osal_task_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_task_tcb_t* task = (osal_task_tcb_t*)handle;

    /* Validate task pointer */
    bool valid = false;
    for (int i = 0; i < OSAL_MAX_TASKS; i++) {
        if (&s_tasks[i] == task) {
            valid = true;
            break;
        }
    }

    if (!valid || task->state == TASK_STATE_UNUSED ||
        task->state == TASK_STATE_DELETED) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_enter_critical();
    task->state = TASK_STATE_SUSPENDED;
    osal_exit_critical();

    return OSAL_OK;
}

osal_status_t osal_task_resume(osal_task_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_task_tcb_t* task = (osal_task_tcb_t*)handle;

    /* Validate task pointer */
    bool valid = false;
    for (int i = 0; i < OSAL_MAX_TASKS; i++) {
        if (&s_tasks[i] == task) {
            valid = true;
            break;
        }
    }

    if (!valid || task->state == TASK_STATE_UNUSED ||
        task->state == TASK_STATE_DELETED) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_enter_critical();
    if (task->state == TASK_STATE_SUSPENDED) {
        task->state = TASK_STATE_READY;
    }
    osal_exit_critical();

    return OSAL_OK;
}

osal_status_t osal_task_delay(uint32_t ms)
{
    if (ms == 0) {
        return OSAL_OK;
    }

    /* In cooperative scheduling, we do a busy-wait delay */
    /* Convert ms to microseconds and delay */
    uint32_t us = ms * 1000;
    osal_platform_delay_us(us);

    return OSAL_OK;
}

osal_status_t osal_task_yield(void)
{
    /* In cooperative scheduling, yield does nothing special */
    /* The scheduler will pick the next task when current task returns */
    return OSAL_OK;
}

osal_task_handle_t osal_task_get_current(void)
{
    if (s_current_task >= 0 && s_current_task < OSAL_MAX_TASKS) {
        return (osal_task_handle_t)&s_tasks[s_current_task];
    }
    return NULL;
}

const char* osal_task_get_name(osal_task_handle_t handle)
{
    if (handle == NULL) {
        return "main";
    }

    osal_task_tcb_t* task = (osal_task_tcb_t*)handle;

    /* Validate task pointer */
    for (int i = 0; i < OSAL_MAX_TASKS; i++) {
        if (&s_tasks[i] == task && task->state != TASK_STATE_UNUSED) {
            return task->name[0] != '\0' ? task->name : "unnamed";
        }
    }

    return NULL;
}

/*---------------------------------------------------------------------------*/
/* Mutex Functions                                                           */
/*---------------------------------------------------------------------------*/

osal_status_t osal_mutex_create(osal_mutex_handle_t* handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Auto-initialize if needed */
    if (!s_osal_initialized) {
        osal_init();
    }

    osal_enter_critical();

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < OSAL_MAX_MUTEXES; i++) {
        if (!s_mutexes[i].used) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        osal_exit_critical();
        return OSAL_ERROR_NO_MEMORY;
    }

    /* Initialize mutex */
    osal_mutex_cb_t* mutex = &s_mutexes[slot];
    mutex->used = true;
    mutex->locked = false;
    mutex->owner = NULL;
    mutex->lock_count = 0;

    *handle = (osal_mutex_handle_t)mutex;

    osal_exit_critical();
    return OSAL_OK;
}

osal_status_t osal_mutex_delete(osal_mutex_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_mutex_cb_t* mutex = (osal_mutex_cb_t*)handle;

    /* Validate mutex pointer */
    bool valid = false;
    for (int i = 0; i < OSAL_MAX_MUTEXES; i++) {
        if (&s_mutexes[i] == mutex) {
            valid = true;
            break;
        }
    }

    if (!valid || !mutex->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_enter_critical();
    mutex->used = false;
    mutex->locked = false;
    mutex->owner = NULL;
    mutex->lock_count = 0;
    osal_exit_critical();

    return OSAL_OK;
}

osal_status_t osal_mutex_lock(osal_mutex_handle_t handle, uint32_t timeout_ms)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_mutex_cb_t* mutex = (osal_mutex_cb_t*)handle;

    /* Validate mutex pointer */
    bool valid = false;
    for (int i = 0; i < OSAL_MAX_MUTEXES; i++) {
        if (&s_mutexes[i] == mutex) {
            valid = true;
            break;
        }
    }

    if (!valid || !mutex->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_task_handle_t current = osal_task_get_current();

    osal_enter_critical();

    /* Check if already owned by current task (recursive) */
    if (mutex->locked && mutex->owner == current) {
        mutex->lock_count++;
        osal_exit_critical();
        return OSAL_OK;
    }

    /* Try to acquire */
    if (!mutex->locked) {
        mutex->locked = true;
        mutex->owner = current;
        mutex->lock_count = 1;
        osal_exit_critical();
        return OSAL_OK;
    }

    osal_exit_critical();

    /* Mutex is locked by another task */
    if (timeout_ms == OSAL_NO_WAIT) {
        return OSAL_ERROR_TIMEOUT;
    }

    /* In baremetal cooperative scheduling, we can't truly block */
    /* So we busy-wait with timeout */
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms || timeout_ms == OSAL_WAIT_FOREVER) {
        osal_platform_delay_us(1000); /* 1ms delay */
        elapsed++;

        osal_enter_critical();
        if (!mutex->locked) {
            mutex->locked = true;
            mutex->owner = current;
            mutex->lock_count = 1;
            osal_exit_critical();
            return OSAL_OK;
        }
        osal_exit_critical();

        if (timeout_ms != OSAL_WAIT_FOREVER && elapsed >= timeout_ms) {
            break;
        }
    }

    return OSAL_ERROR_TIMEOUT;
}

osal_status_t osal_mutex_unlock(osal_mutex_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_mutex_cb_t* mutex = (osal_mutex_cb_t*)handle;

    /* Validate mutex pointer */
    bool valid = false;
    for (int i = 0; i < OSAL_MAX_MUTEXES; i++) {
        if (&s_mutexes[i] == mutex) {
            valid = true;
            break;
        }
    }

    if (!valid || !mutex->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_enter_critical();

    if (!mutex->locked) {
        osal_exit_critical();
        return OSAL_ERROR;
    }

    /* Handle recursive unlock */
    if (mutex->lock_count > 1) {
        mutex->lock_count--;
        osal_exit_critical();
        return OSAL_OK;
    }

    /* Fully unlock */
    mutex->locked = false;
    mutex->owner = NULL;
    mutex->lock_count = 0;

    osal_exit_critical();
    return OSAL_OK;
}

/*---------------------------------------------------------------------------*/
/* Semaphore Functions                                                       */
/*---------------------------------------------------------------------------*/

osal_status_t osal_sem_create(uint32_t initial_count,
                              uint32_t max_count,
                              osal_sem_handle_t* handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (initial_count > max_count) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Auto-initialize if needed */
    if (!s_osal_initialized) {
        osal_init();
    }

    osal_enter_critical();

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < OSAL_MAX_SEMS; i++) {
        if (!s_sems[i].used) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        osal_exit_critical();
        return OSAL_ERROR_NO_MEMORY;
    }

    /* Initialize semaphore */
    osal_sem_cb_t* sem = &s_sems[slot];
    sem->used = true;
    sem->count = initial_count;
    sem->max_count = max_count;

    *handle = (osal_sem_handle_t)sem;

    osal_exit_critical();
    return OSAL_OK;
}

osal_status_t osal_sem_create_binary(uint32_t initial,
                                     osal_sem_handle_t* handle)
{
    return osal_sem_create(initial ? 1 : 0, 1, handle);
}

osal_status_t osal_sem_create_counting(uint32_t max_count,
                                       uint32_t initial,
                                       osal_sem_handle_t* handle)
{
    return osal_sem_create(initial, max_count, handle);
}

osal_status_t osal_sem_delete(osal_sem_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_sem_cb_t* sem = (osal_sem_cb_t*)handle;

    /* Validate semaphore pointer */
    bool valid = false;
    for (int i = 0; i < OSAL_MAX_SEMS; i++) {
        if (&s_sems[i] == sem) {
            valid = true;
            break;
        }
    }

    if (!valid || !sem->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_enter_critical();
    sem->used = false;
    sem->count = 0;
    sem->max_count = 0;
    osal_exit_critical();

    return OSAL_OK;
}

osal_status_t osal_sem_take(osal_sem_handle_t handle, uint32_t timeout_ms)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_sem_cb_t* sem = (osal_sem_cb_t*)handle;

    /* Validate semaphore pointer */
    bool valid = false;
    for (int i = 0; i < OSAL_MAX_SEMS; i++) {
        if (&s_sems[i] == sem) {
            valid = true;
            break;
        }
    }

    if (!valid || !sem->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_enter_critical();

    /* Try to take immediately */
    if (sem->count > 0) {
        sem->count--;
        osal_exit_critical();
        return OSAL_OK;
    }

    osal_exit_critical();

    /* Semaphore not available */
    if (timeout_ms == OSAL_NO_WAIT) {
        return OSAL_ERROR_TIMEOUT;
    }

    /* Busy-wait with timeout */
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms || timeout_ms == OSAL_WAIT_FOREVER) {
        osal_platform_delay_us(1000); /* 1ms delay */
        elapsed++;

        osal_enter_critical();
        if (sem->count > 0) {
            sem->count--;
            osal_exit_critical();
            return OSAL_OK;
        }
        osal_exit_critical();

        if (timeout_ms != OSAL_WAIT_FOREVER && elapsed >= timeout_ms) {
            break;
        }
    }

    return OSAL_ERROR_TIMEOUT;
}

osal_status_t osal_sem_give(osal_sem_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_sem_cb_t* sem = (osal_sem_cb_t*)handle;

    /* Validate semaphore pointer */
    bool valid = false;
    for (int i = 0; i < OSAL_MAX_SEMS; i++) {
        if (&s_sems[i] == sem) {
            valid = true;
            break;
        }
    }

    if (!valid || !sem->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_enter_critical();

    if (sem->count < sem->max_count) {
        sem->count++;
    }

    osal_exit_critical();
    return OSAL_OK;
}

osal_status_t osal_sem_give_from_isr(osal_sem_handle_t handle)
{
    /* Same as osal_sem_give for baremetal */
    return osal_sem_give(handle);
}

/*---------------------------------------------------------------------------*/
/* Queue Functions                                                           */
/*---------------------------------------------------------------------------*/

osal_status_t osal_queue_create(size_t item_size,
                                size_t item_count,
                                osal_queue_handle_t* handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (item_size == 0 || item_count == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    if (item_size * item_count > OSAL_QUEUE_MAX_SIZE) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Auto-initialize if needed */
    if (!s_osal_initialized) {
        osal_init();
    }

    osal_enter_critical();

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < OSAL_MAX_QUEUES; i++) {
        if (!s_queues[i].used) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        osal_exit_critical();
        return OSAL_ERROR_NO_MEMORY;
    }

    /* Initialize queue */
    osal_queue_cb_t* queue = &s_queues[slot];
    memset(queue, 0, sizeof(*queue));
    queue->used = true;
    queue->item_size = item_size;
    queue->item_count = item_count;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;

    *handle = (osal_queue_handle_t)queue;

    osal_exit_critical();
    return OSAL_OK;
}

osal_status_t osal_queue_delete(osal_queue_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_cb_t* queue = (osal_queue_cb_t*)handle;

    /* Validate queue pointer */
    bool valid = false;
    for (int i = 0; i < OSAL_MAX_QUEUES; i++) {
        if (&s_queues[i] == queue) {
            valid = true;
            break;
        }
    }

    if (!valid || !queue->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_enter_critical();
    queue->used = false;
    queue->count = 0;
    osal_exit_critical();

    return OSAL_OK;
}

osal_status_t osal_queue_send(osal_queue_handle_t handle,
                              const void* item,
                              uint32_t timeout_ms)
{
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_cb_t* queue = (osal_queue_cb_t*)handle;

    /* Validate queue pointer */
    bool valid = false;
    for (int i = 0; i < OSAL_MAX_QUEUES; i++) {
        if (&s_queues[i] == queue) {
            valid = true;
            break;
        }
    }

    if (!valid || !queue->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_enter_critical();

    /* Try to send immediately */
    if (queue->count < queue->item_count) {
        memcpy(&queue->buffer[queue->tail * queue->item_size],
               item, queue->item_size);
        queue->tail = (queue->tail + 1) % queue->item_count;
        queue->count++;
        osal_exit_critical();
        return OSAL_OK;
    }

    osal_exit_critical();

    /* Queue is full */
    if (timeout_ms == OSAL_NO_WAIT) {
        return OSAL_ERROR_FULL;
    }

    /* Busy-wait with timeout */
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms || timeout_ms == OSAL_WAIT_FOREVER) {
        osal_platform_delay_us(1000); /* 1ms delay */
        elapsed++;

        osal_enter_critical();
        if (queue->count < queue->item_count) {
            memcpy(&queue->buffer[queue->tail * queue->item_size],
                   item, queue->item_size);
            queue->tail = (queue->tail + 1) % queue->item_count;
            queue->count++;
            osal_exit_critical();
            return OSAL_OK;
        }
        osal_exit_critical();

        if (timeout_ms != OSAL_WAIT_FOREVER && elapsed >= timeout_ms) {
            break;
        }
    }

    return OSAL_ERROR_TIMEOUT;
}

osal_status_t osal_queue_send_front(osal_queue_handle_t handle,
                                    const void* item,
                                    uint32_t timeout_ms)
{
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_cb_t* queue = (osal_queue_cb_t*)handle;

    /* Validate queue pointer */
    bool valid = false;
    for (int i = 0; i < OSAL_MAX_QUEUES; i++) {
        if (&s_queues[i] == queue) {
            valid = true;
            break;
        }
    }

    if (!valid || !queue->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_enter_critical();

    /* Try to send immediately */
    if (queue->count < queue->item_count) {
        /* Move head back */
        queue->head = (queue->head == 0) ? queue->item_count - 1 : queue->head - 1;
        memcpy(&queue->buffer[queue->head * queue->item_size],
               item, queue->item_size);
        queue->count++;
        osal_exit_critical();
        return OSAL_OK;
    }

    osal_exit_critical();

    /* Queue is full */
    if (timeout_ms == OSAL_NO_WAIT) {
        return OSAL_ERROR_FULL;
    }

    /* Busy-wait with timeout */
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms || timeout_ms == OSAL_WAIT_FOREVER) {
        osal_platform_delay_us(1000);
        elapsed++;

        osal_enter_critical();
        if (queue->count < queue->item_count) {
            queue->head = (queue->head == 0) ? queue->item_count - 1 : queue->head - 1;
            memcpy(&queue->buffer[queue->head * queue->item_size],
                   item, queue->item_size);
            queue->count++;
            osal_exit_critical();
            return OSAL_OK;
        }
        osal_exit_critical();

        if (timeout_ms != OSAL_WAIT_FOREVER && elapsed >= timeout_ms) {
            break;
        }
    }

    return OSAL_ERROR_TIMEOUT;
}

osal_status_t osal_queue_receive(osal_queue_handle_t handle,
                                 void* item,
                                 uint32_t timeout_ms)
{
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_cb_t* queue = (osal_queue_cb_t*)handle;

    /* Validate queue pointer */
    bool valid = false;
    for (int i = 0; i < OSAL_MAX_QUEUES; i++) {
        if (&s_queues[i] == queue) {
            valid = true;
            break;
        }
    }

    if (!valid || !queue->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_enter_critical();

    /* Try to receive immediately */
    if (queue->count > 0) {
        memcpy(item, &queue->buffer[queue->head * queue->item_size],
               queue->item_size);
        queue->head = (queue->head + 1) % queue->item_count;
        queue->count--;
        osal_exit_critical();
        return OSAL_OK;
    }

    osal_exit_critical();

    /* Queue is empty */
    if (timeout_ms == OSAL_NO_WAIT) {
        return OSAL_ERROR_EMPTY;
    }

    /* Busy-wait with timeout */
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms || timeout_ms == OSAL_WAIT_FOREVER) {
        osal_platform_delay_us(1000);
        elapsed++;

        osal_enter_critical();
        if (queue->count > 0) {
            memcpy(item, &queue->buffer[queue->head * queue->item_size],
                   queue->item_size);
            queue->head = (queue->head + 1) % queue->item_count;
            queue->count--;
            osal_exit_critical();
            return OSAL_OK;
        }
        osal_exit_critical();

        if (timeout_ms != OSAL_WAIT_FOREVER && elapsed >= timeout_ms) {
            break;
        }
    }

    return OSAL_ERROR_TIMEOUT;
}

osal_status_t osal_queue_peek(osal_queue_handle_t handle, void* item)
{
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_cb_t* queue = (osal_queue_cb_t*)handle;

    /* Validate queue pointer */
    bool valid = false;
    for (int i = 0; i < OSAL_MAX_QUEUES; i++) {
        if (&s_queues[i] == queue) {
            valid = true;
            break;
        }
    }

    if (!valid || !queue->used) {
        return OSAL_ERROR_INVALID_PARAM;
    }

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

    osal_queue_cb_t* queue = (osal_queue_cb_t*)handle;

    /* Validate queue pointer */
    for (int i = 0; i < OSAL_MAX_QUEUES; i++) {
        if (&s_queues[i] == queue && queue->used) {
            return queue->count;
        }
    }

    return 0;
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

    osal_queue_cb_t* queue = (osal_queue_cb_t*)handle;

    /* Validate queue pointer */
    for (int i = 0; i < OSAL_MAX_QUEUES; i++) {
        if (&s_queues[i] == queue && queue->used) {
            return queue->count >= queue->item_count;
        }
    }

    return true;
}

osal_status_t osal_queue_send_from_isr(osal_queue_handle_t handle,
                                       const void* item)
{
    /* Same as osal_queue_send with no wait for baremetal */
    return osal_queue_send(handle, item, OSAL_NO_WAIT);
}

osal_status_t osal_queue_receive_from_isr(osal_queue_handle_t handle,
                                          void* item)
{
    /* Same as osal_queue_receive with no wait for baremetal */
    return osal_queue_receive(handle, item, OSAL_NO_WAIT);
}
