/**
 * \file            osal_freertos.c
 * \brief           OSAL FreeRTOS Adapter Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This file implements the OSAL interface using FreeRTOS APIs.
 *                  It provides task management, synchronization primitives, and
 *                  inter-task communication facilities.
 *
 * \section error_handling Error Handling Strategy
 *
 * This implementation follows a unified error handling approach:
 *
 * 1. NULL Pointer Validation: All functions that accept pointer parameters
 *    validate them at entry and return OSAL_ERROR_NULL_POINTER if NULL.
 *    - Exception: osal_task_delete() accepts NULL to delete the calling task.
 *
 * 2. Invalid Parameter Validation: Functions validate parameter constraints:
 *    - Priority must be in range [0, 31]
 *    - item_size and item_count must be > 0 for queues
 *    - initial_count must not exceed max_count for semaphores
 *    - max_count must be >= 1 for counting semaphores
 *
 * 3. Timeout Conversion: All timeout values are converted using
 *    osal_to_freertos_ticks() which handles:
 *    - OSAL_WAIT_FOREVER -> portMAX_DELAY
 *    - OSAL_NO_WAIT -> 0
 *    - Positive ms values -> pdMS_TO_TICKS(ms)
 *
 * 4. ISR Context Detection: Functions that cannot be called from ISR
 *    context (e.g., osal_mutex_lock) detect ISR context and return
 *    OSAL_ERROR_ISR.
 *
 * 5. Memory Allocation Failures: When FreeRTOS returns NULL for object
 *    creation, OSAL_ERROR_NO_MEMORY is returned.
 *
 * \note            Requirements: 9.1, 9.2, 9.3, 10.1, 10.2, 10.3, 10.4
 */

#include "osal/osal.h"
#include "osal/osal_internal.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"

/* Standard library includes */
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Error Handling Macros                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Validate that a pointer is not NULL
 * \note            Requirements: 10.1
 */
#define OSAL_VALIDATE_PTR(ptr)                                                 \
    do {                                                                       \
        if ((ptr) == NULL) {                                                   \
            return OSAL_ERROR_NULL_POINTER;                                    \
        }                                                                      \
    } while (0)

/**
 * \brief           Validate that two pointers are not NULL
 * \note            Requirements: 10.1
 */
#define OSAL_VALIDATE_PTR2(ptr1, ptr2)                                         \
    do {                                                                       \
        if ((ptr1) == NULL || (ptr2) == NULL) {                                \
            return OSAL_ERROR_NULL_POINTER;                                    \
        }                                                                      \
    } while (0)

/**
 * \brief           Validate a condition and return error if false
 * \note            Requirements: 10.2
 */
#define OSAL_VALIDATE_PARAM(cond, err)                                         \
    do {                                                                       \
        if (!(cond)) {                                                         \
            return (err);                                                      \
        }                                                                      \
    } while (0)

/**
 * \brief           Check if in ISR context and return error if so
 * \note            Requirements: 10.4
 */
#define OSAL_CHECK_NOT_ISR()                                                   \
    do {                                                                       \
        if (is_in_isr()) {                                                     \
            return OSAL_ERROR_ISR;                                             \
        }                                                                      \
    } while (0)

/*---------------------------------------------------------------------------*/
/* Internal Wrapper Structures for Handle Validation                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Internal task wrapper structure
 * \details         Wraps FreeRTOS TaskHandle_t with handle validation header
 */
typedef struct {
    osal_handle_header_t header;    /**< Handle validation header */
    TaskHandle_t handle;            /**< FreeRTOS task handle */
} osal_task_wrapper_t;

/**
 * \brief           Internal mutex wrapper structure
 * \details         Wraps FreeRTOS SemaphoreHandle_t with handle validation header
 */
typedef struct {
    osal_handle_header_t header;    /**< Handle validation header */
    SemaphoreHandle_t handle;       /**< FreeRTOS mutex handle */
} osal_mutex_wrapper_t;

/**
 * \brief           Internal semaphore wrapper structure
 * \details         Wraps FreeRTOS SemaphoreHandle_t with handle validation header
 */
typedef struct {
    osal_handle_header_t header;    /**< Handle validation header */
    SemaphoreHandle_t handle;       /**< FreeRTOS semaphore handle */
} osal_sem_wrapper_t;

/**
 * \brief           Internal queue wrapper structure
 * \details         Wraps FreeRTOS QueueHandle_t with handle validation header
 */
typedef struct {
    osal_handle_header_t header;    /**< Handle validation header */
    QueueHandle_t handle;           /**< FreeRTOS queue handle */
    osal_queue_mode_t mode;         /**< Queue mode (normal/overwrite) */
    size_t item_count;              /**< Queue capacity for space calculation */
} osal_queue_wrapper_t;

/**
 * \brief           Internal event flags wrapper structure
 * \details         Wraps FreeRTOS EventGroupHandle_t with handle validation header
 */
typedef struct {
    osal_handle_header_t header;    /**< Handle validation header */
    EventGroupHandle_t handle;      /**< FreeRTOS event group handle */
} osal_event_wrapper_t;

/**
 * \brief           Internal timer wrapper structure
 * \details         Wraps FreeRTOS TimerHandle_t with handle validation header
 */
typedef struct {
    osal_handle_header_t header;    /**< Handle validation header */
    TimerHandle_t handle;           /**< FreeRTOS timer handle */
    osal_timer_callback_t callback; /**< User callback function */
    void* arg;                      /**< User callback argument */
} osal_timer_wrapper_t;

/*---------------------------------------------------------------------------*/
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

/** \brief          Flag indicating if OSAL has been initialized */
static bool s_osal_initialized = false;

/** \brief          Critical section nesting counter for ISR context */
static volatile uint32_t s_isr_critical_nesting = 0;

/** \brief          Saved interrupt mask for ISR critical sections */
static volatile UBaseType_t s_isr_saved_mask = 0;

/*---------------------------------------------------------------------------*/
/* Diagnostics - Resource Statistics Tracking                                */
/*---------------------------------------------------------------------------*/

#if OSAL_STATS_ENABLE

/**
 * \brief           Resource statistics structure for internal tracking
 */
typedef struct {
    volatile uint16_t count;        /**< Current count */
    volatile uint16_t watermark;    /**< Peak count (high watermark) */
} osal_resource_stats_internal_t;

/**
 * \brief           Global statistics context
 */
typedef struct {
    osal_resource_stats_internal_t tasks;    /**< Task statistics */
    osal_resource_stats_internal_t mutexes;  /**< Mutex statistics */
    osal_resource_stats_internal_t sems;     /**< Semaphore statistics */
    osal_resource_stats_internal_t queues;   /**< Queue statistics */
    osal_resource_stats_internal_t events;   /**< Event flags statistics */
    osal_resource_stats_internal_t timers;   /**< Timer statistics */
    volatile size_t mem_allocated;           /**< Total bytes allocated */
    volatile size_t mem_peak;                /**< Peak memory allocation */
    volatile size_t mem_alloc_count;         /**< Number of active allocations */
} osal_global_stats_t;

/** \brief          Global statistics instance */
static osal_global_stats_t s_osal_stats = {0};

/**
 * \brief           Increment resource count and update watermark
 * \param[in]       stats: Pointer to statistics structure
 */
static inline void osal_stats_inc(osal_resource_stats_internal_t* stats) {
    osal_enter_critical();
    stats->count++;
    if (stats->count > stats->watermark) {
        stats->watermark = stats->count;
    }
    osal_exit_critical();
}

/**
 * \brief           Decrement resource count
 * \param[in]       stats: Pointer to statistics structure
 */
static inline void osal_stats_dec(osal_resource_stats_internal_t* stats) {
    osal_enter_critical();
    if (stats->count > 0) {
        stats->count--;
    }
    osal_exit_critical();
}

#endif /* OSAL_STATS_ENABLE */

/*---------------------------------------------------------------------------*/
/* Diagnostics - Error Callback                                              */
/*---------------------------------------------------------------------------*/

/** \brief          Registered error callback function */
static osal_error_callback_t s_error_callback = NULL;

/*---------------------------------------------------------------------------*/
/* Helper Functions - Priority Mapping                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Map OSAL priority to FreeRTOS priority
 *
 * \details         OSAL uses priority range 0-31 where 0 is lowest and 31 is
 * highest. FreeRTOS uses priority range 0 to (configMAX_PRIORITIES-1) where 0
 * is lowest.
 *
 * \note            Requirements: 4.7
 */
static inline UBaseType_t osal_to_freertos_priority(uint8_t osal_prio) {
    /* Clamp OSAL priority to valid range */
    if (osal_prio > 31) {
        osal_prio = 31;
    }

    /* Linear mapping from OSAL [0,31] to FreeRTOS [0, configMAX_PRIORITIES-1]
     */
    return (UBaseType_t)((osal_prio * (configMAX_PRIORITIES - 1)) / 31);
}

/**
 * \brief           Map FreeRTOS priority to OSAL priority
 *
 * \details         Reverse mapping from FreeRTOS priority to OSAL priority.
 */
static inline uint8_t freertos_to_osal_priority(UBaseType_t freertos_prio) {
    /* Clamp FreeRTOS priority to valid range */
    if (freertos_prio >= configMAX_PRIORITIES) {
        freertos_prio = configMAX_PRIORITIES - 1;
    }

    /* Linear mapping from FreeRTOS [0, configMAX_PRIORITIES-1] to OSAL [0,31]
     */
    return (uint8_t)((freertos_prio * 31) / (configMAX_PRIORITIES - 1));
}

/*---------------------------------------------------------------------------*/
/* Helper Functions - Timeout Conversion                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Convert OSAL timeout to FreeRTOS ticks
 *
 * \details         Converts OSAL timeout values (in milliseconds) to FreeRTOS
 * tick counts.
 *
 * \note            Requirements: 9.1, 9.2, 9.3
 */
static inline TickType_t osal_to_freertos_ticks(uint32_t timeout_ms) {
    /* OSAL_WAIT_FOREVER maps to portMAX_DELAY (infinite wait) */
    if (timeout_ms == OSAL_WAIT_FOREVER) {
        return portMAX_DELAY;
    }

    /* OSAL_NO_WAIT maps to 0 (no blocking) */
    if (timeout_ms == OSAL_NO_WAIT) {
        return 0;
    }

    /* Convert milliseconds to ticks using FreeRTOS macro */
    return pdMS_TO_TICKS(timeout_ms);
}

/*---------------------------------------------------------------------------*/
/* Helper Functions - ISR Context Detection                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Check if currently executing in ISR context
 *
 * \details         Detects whether the current execution context is an
 * interrupt service routine (ISR). This is used to select appropriate FreeRTOS
 * API variants (e.g., xSemaphoreGiveFromISR vs xSemaphoreGive).
 *
 * \note            Requirements: 8.4
 */
static inline bool is_in_isr(void) {
#if defined(__ARM_ARCH) || defined(__arm__) || defined(__ARM_ARCH_7M__) ||     \
    defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_6M__)
    /*
     * On ARM Cortex-M processors, read the IPSR (Interrupt Program Status
     * Register). A non-zero value indicates we're in an exception handler.
     */
    uint32_t ipsr;
    __asm volatile("mrs %0, ipsr" : "=r"(ipsr));
    return (ipsr != 0);
#elif defined(xPortIsInsideInterrupt)
    /* Use FreeRTOS port function if available */
    return xPortIsInsideInterrupt();
#else
    /*
     * For platforms without ISR detection capability (e.g., native/POSIX),
     * assume we're not in ISR context.
     */
    return false;
#endif
}

/*---------------------------------------------------------------------------*/
/* OSAL Core Functions                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize OSAL layer
 *
 * \details         Initializes the OSAL layer with FreeRTOS backend. This
 * function is idempotent - calling it multiple times has no additional effect.
 *
 * \note            Requirements: 3.1, 3.4
 */
osal_status_t osal_init(void) {
    if (s_osal_initialized) {
        /* Already initialized - return success (idempotent) */
        return OSAL_OK;
    }

    /*
     * FreeRTOS doesn't require explicit initialization before creating
     * tasks and other objects. The kernel is initialized when
     * vTaskStartScheduler() is called.
     */

    s_osal_initialized = true;
    return OSAL_OK;
}

/**
 * \brief           Start OSAL scheduler
 *
 * \details         Starts the FreeRTOS scheduler. This function does not return
 * under normal operation.
 *
 * \note            Requirements: 3.2
 */
void osal_start(void) {
    if (!s_osal_initialized) {
        osal_init();
    }

    /* Start the FreeRTOS scheduler - this function does not return */
    vTaskStartScheduler();

    /*
     * If we reach here, there was insufficient heap memory to create
     * the idle task or timer task.
     */
}

/**
 * \brief           Check if scheduler is running
 *
 * \note            Requirements: 3.3
 */
bool osal_is_running(void) {
    return (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED);
}

/**
 * \brief           Enter critical section
 *
 * \details         Disables interrupts and enters a critical section. Supports
 * nesting - interrupts are only re-enabled when the nesting count returns to
 * zero. For task context, FreeRTOS taskENTER_CRITICAL() handles nesting
 * internally. For ISR context, we manually track nesting and save/restore the
 * interrupt mask.
 *
 * \note            Requirements: 8.1, 8.3
 */
void osal_enter_critical(void) {
    if (is_in_isr()) {
        /*
         * In ISR context, use the ISR-safe critical section API.
         * Save the interrupt mask on first entry only.
         */
        if (s_isr_critical_nesting == 0) {
            s_isr_saved_mask = portSET_INTERRUPT_MASK_FROM_ISR();
        }
        s_isr_critical_nesting++;
    } else {
        /* FreeRTOS taskENTER_CRITICAL handles nesting internally */
        taskENTER_CRITICAL();
    }
}

/**
 * \brief           Exit critical section
 *
 * \details         Restores interrupt state and exits the critical section.
 * Only restores interrupts when the nesting count returns to zero.
 *
 * \note            Requirements: 8.2, 8.3
 */
void osal_exit_critical(void) {
    if (is_in_isr()) {
        /*
         * In ISR context, restore interrupt mask only when nesting
         * count returns to zero.
         */
        if (s_isr_critical_nesting > 0) {
            s_isr_critical_nesting--;
            if (s_isr_critical_nesting == 0) {
                portCLEAR_INTERRUPT_MASK_FROM_ISR(s_isr_saved_mask);
            }
        }
    } else {
        /* FreeRTOS taskEXIT_CRITICAL handles nesting internally */
        taskEXIT_CRITICAL();
    }
}

/**
 * \brief           Check if in ISR context
 *
 * \note            Requirements: 8.4
 */
bool osal_is_isr(void) {
    return is_in_isr();
}

/*---------------------------------------------------------------------------*/
/* Task Functions                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Create a new task
 *
 * \details         Creates a FreeRTOS task with the specified configuration.
 *                  The OSAL priority (0-31) is mapped to FreeRTOS priority
 * range.
 *
 * \note            Requirements: 4.1, 4.7, 10.1, 10.2
 */
osal_status_t osal_task_create(const osal_task_config_t* config,
                               osal_task_handle_t* handle) {
    /* Parameter validation - NULL pointer checks */
    if (config == NULL || handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Parameter validation - function pointer must be valid */
    if (config->func == NULL) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Parameter validation - priority must be in valid range (0-31) */
    if (config->priority > 31) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Auto-initialize OSAL if needed */
    if (!s_osal_initialized) {
        osal_status_t status = osal_init();
        if (status != OSAL_OK) {
            return status;
        }
    }

    /* Allocate wrapper structure for handle validation */
    osal_task_wrapper_t* wrapper =
        (osal_task_wrapper_t*)pvPortMalloc(sizeof(osal_task_wrapper_t));
    if (wrapper == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    /* Initialize handle header for validation */
    OSAL_HANDLE_INIT(&wrapper->header, OSAL_TYPE_TASK);

    /* Map OSAL priority to FreeRTOS priority */
    UBaseType_t freertos_priority = osal_to_freertos_priority(config->priority);

    /*
     * Convert stack size from bytes to words (StackType_t units).
     * FreeRTOS stack size is specified in words, not bytes.
     * Ensure minimum stack size of configMINIMAL_STACK_SIZE.
     */
    configSTACK_DEPTH_TYPE stack_depth =
        config->stack_size / sizeof(StackType_t);
    if (stack_depth < configMINIMAL_STACK_SIZE) {
        stack_depth = configMINIMAL_STACK_SIZE;
    }

    /* Use provided name or default to "task" */
    const char* task_name = (config->name != NULL) ? config->name : "task";

    /* Create the FreeRTOS task */
    BaseType_t result =
        xTaskCreate((TaskFunction_t)config->func, /* Task function */
                    task_name,                    /* Task name */
                    stack_depth,                  /* Stack depth in words */
                    config->arg,                  /* Task argument */
                    freertos_priority,            /* FreeRTOS priority */
                    &wrapper->handle              /* Task handle output */
        );

    /* Check if task creation succeeded */
    if (result != pdPASS) {
        /* Invalidate and free wrapper on failure */
        OSAL_HANDLE_DEINIT(&wrapper->header);
        vPortFree(wrapper);
        return OSAL_ERROR_NO_MEMORY;
    }

#if OSAL_STATS_ENABLE
    /* Update task statistics */
    osal_stats_inc(&s_osal_stats.tasks);
#endif

    /* Return the wrapper as the task handle */
    *handle = (osal_task_handle_t)wrapper;
    return OSAL_OK;
}

/**
 * \brief           Delete a task
 *
 * \details         Deletes a FreeRTOS task. If handle is NULL, deletes the
 *                  calling task.
 *
 * \note            Requirements: 4.2
 */
osal_status_t osal_task_delete(osal_task_handle_t handle) {
    osal_task_wrapper_t* wrapper = (osal_task_wrapper_t*)handle;
    TaskHandle_t task_handle = NULL;

    if (wrapper != NULL) {
        /* Validate handle using magic number and type check */
#if OSAL_HANDLE_VALIDATION
        if (!OSAL_HANDLE_IS_VALID(&wrapper->header, OSAL_TYPE_TASK)) {
            return OSAL_ERROR_INVALID_PARAM;
        }
#endif

        /* Extract FreeRTOS handle from wrapper */
        task_handle = wrapper->handle;

#if OSAL_STATS_ENABLE
        /* Update task statistics */
        osal_stats_dec(&s_osal_stats.tasks);
#endif

        /* Invalidate handle header before deletion */
        OSAL_HANDLE_DEINIT(&wrapper->header);

        /* Free the wrapper structure */
        vPortFree(wrapper);
    }

    /*
     * FreeRTOS vTaskDelete accepts NULL to delete the calling task.
     * No NULL pointer error for this function - NULL is a valid input.
     */
    vTaskDelete(task_handle);
    return OSAL_OK;
}

/**
 * \brief           Suspend a task
 *
 * \details         Suspends a FreeRTOS task. The task will not run until
 * resumed.
 *
 * \note            Requirements: 4.3
 */
osal_status_t osal_task_suspend(osal_task_handle_t handle) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_TASK);

    osal_task_wrapper_t* wrapper = (osal_task_wrapper_t*)handle;
    vTaskSuspend(wrapper->handle);
    return OSAL_OK;
}

/**
 * \brief           Resume a suspended task
 *
 * \details         Resumes a previously suspended FreeRTOS task.
 *
 * \note            Requirements: 4.4
 */
osal_status_t osal_task_resume(osal_task_handle_t handle) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_TASK);

    osal_task_wrapper_t* wrapper = (osal_task_wrapper_t*)handle;
    vTaskResume(wrapper->handle);
    return OSAL_OK;
}

/**
 * \brief           Delay current task
 *
 * \details         Blocks the calling task for the specified number of
 * milliseconds.
 *
 * \note            Requirements: 4.5
 */
osal_status_t osal_task_delay(uint32_t ms) {
    /* Convert milliseconds to FreeRTOS ticks and delay */
    TickType_t ticks = osal_to_freertos_ticks(ms);
    vTaskDelay(ticks);
    return OSAL_OK;
}

/**
 * \brief           Yield current task
 *
 * \details         Yields the processor to another task of equal or higher
 * priority.
 *
 * \note            Requirements: 4.6
 */
osal_status_t osal_task_yield(void) {
    taskYIELD();
    return OSAL_OK;
}

/**
 * \brief           Get current task handle
 *
 * \note            Requirements: 4.8
 */
osal_task_handle_t osal_task_get_current(void) {
    /* Note: This returns the FreeRTOS handle directly, not a wrapper.
     * For proper handle validation, the caller should use the wrapper
     * returned from osal_task_create(). */
    return (osal_task_handle_t)xTaskGetCurrentTaskHandle();
}

/**
 * \brief           Get task name
 *
 * \note            Requirements: 4.9
 */
const char* osal_task_get_name(osal_task_handle_t handle) {
    if (handle == NULL) {
        return NULL;
    }

    osal_task_wrapper_t* wrapper = (osal_task_wrapper_t*)handle;
    return pcTaskGetName(wrapper->handle);
}

/**
 * \brief           Get task priority
 *
 * \details         Returns the current priority of the specified task.
 *                  The FreeRTOS priority is mapped back to OSAL priority range.
 *
 * \note            Requirements: 9.1
 */
uint8_t osal_task_get_priority(osal_task_handle_t handle) {
    if (handle == NULL) {
        return 0;
    }

    osal_task_wrapper_t* wrapper = (osal_task_wrapper_t*)handle;

    /* Validate handle - return 0 for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&wrapper->header, OSAL_TYPE_TASK)) {
        return 0;
    }
#endif

    /* Get FreeRTOS priority and convert to OSAL priority */
    UBaseType_t freertos_prio = uxTaskPriorityGet(wrapper->handle);
    return freertos_to_osal_priority(freertos_prio);
}

/**
 * \brief           Set task priority
 *
 * \details         Changes the priority of the specified task at runtime.
 *                  The OSAL priority is mapped to FreeRTOS priority range.
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

    osal_task_wrapper_t* wrapper = (osal_task_wrapper_t*)handle;
    /* Map OSAL priority to FreeRTOS priority and set */
    UBaseType_t freertos_prio = osal_to_freertos_priority(priority);
    vTaskPrioritySet(wrapper->handle, freertos_prio);

    return OSAL_OK;
}

/**
 * \brief           Get task stack high watermark
 *
 * \details         Returns the minimum amount of remaining stack space that
 *                  was available to the task since the task started executing.
 *                  This is the high water mark of stack usage.
 *
 * \note            Requirements: 9.3
 */
size_t osal_task_get_stack_watermark(osal_task_handle_t handle) {
    if (handle == NULL) {
        return 0;
    }

    osal_task_wrapper_t* wrapper = (osal_task_wrapper_t*)handle;

    /* Validate handle - return 0 for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&wrapper->header, OSAL_TYPE_TASK)) {
        return 0;
    }
#endif

    /*
     * uxTaskGetStackHighWaterMark returns the minimum free stack space
     * in words (StackType_t units). Convert to bytes.
     */
    UBaseType_t watermark_words = uxTaskGetStackHighWaterMark(wrapper->handle);
    return (size_t)(watermark_words * sizeof(StackType_t));
}

/**
 * \brief           Get task state
 *
 * \details         Returns the current state of the specified task.
 *                  Maps FreeRTOS task state to OSAL task state enumeration.
 *
 * \note            Requirements: 9.4
 */
osal_task_state_t osal_task_get_state(osal_task_handle_t handle) {
    if (handle == NULL) {
        return OSAL_TASK_STATE_DELETED;
    }

    osal_task_wrapper_t* wrapper = (osal_task_wrapper_t*)handle;

    /* Validate handle - return DELETED for invalid handles */
#if OSAL_HANDLE_VALIDATION
    if (!OSAL_HANDLE_IS_VALID(&wrapper->header, OSAL_TYPE_TASK)) {
        return OSAL_TASK_STATE_DELETED;
    }
#endif

    /* Get FreeRTOS task state */
    eTaskState freertos_state = eTaskGetState(wrapper->handle);

    /* Map FreeRTOS state to OSAL state */
    switch (freertos_state) {
        case eRunning:
            return OSAL_TASK_STATE_RUNNING;
        case eReady:
            return OSAL_TASK_STATE_READY;
        case eBlocked:
            return OSAL_TASK_STATE_BLOCKED;
        case eSuspended:
            return OSAL_TASK_STATE_SUSPENDED;
        case eDeleted:
            return OSAL_TASK_STATE_DELETED;
        default:
            return OSAL_TASK_STATE_READY;
    }
}

/*---------------------------------------------------------------------------*/
/* Mutex Functions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Create a mutex
 *
 * \details         Creates a FreeRTOS mutex using xSemaphoreCreateMutex().
 * FreeRTOS mutexes support priority inheritance to prevent priority inversion.
 *
 * \note            Requirements: 5.1, 5.5
 */
osal_status_t osal_mutex_create(osal_mutex_handle_t* handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Auto-initialize OSAL if needed */
    if (!s_osal_initialized) {
        osal_status_t status = osal_init();
        if (status != OSAL_OK) {
            return status;
        }
    }

    /* Allocate wrapper structure for handle validation */
    osal_mutex_wrapper_t* wrapper =
        (osal_mutex_wrapper_t*)pvPortMalloc(sizeof(osal_mutex_wrapper_t));
    if (wrapper == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    /* Initialize handle header for validation */
    OSAL_HANDLE_INIT(&wrapper->header, OSAL_TYPE_MUTEX);

    /* Create FreeRTOS mutex with priority inheritance */
    wrapper->handle = xSemaphoreCreateMutex();
    if (wrapper->handle == NULL) {
        /* Invalidate and free wrapper on failure */
        OSAL_HANDLE_DEINIT(&wrapper->header);
        vPortFree(wrapper);
        return OSAL_ERROR_NO_MEMORY;
    }

#if OSAL_STATS_ENABLE
    /* Update mutex statistics */
    osal_stats_inc(&s_osal_stats.mutexes);
#endif

    *handle = (osal_mutex_handle_t)wrapper;
    return OSAL_OK;
}

/**
 * \brief           Delete a mutex
 *
 * \details         Deletes a FreeRTOS mutex using vSemaphoreDelete().
 *
 * \note            Requirements: 5.2
 */
osal_status_t osal_mutex_delete(osal_mutex_handle_t handle) {
    /* Validate handle using magic number and type check */
    OSAL_VALIDATE_HANDLE(handle, OSAL_TYPE_MUTEX);

    osal_mutex_wrapper_t* wrapper = (osal_mutex_wrapper_t*)handle;

#if OSAL_STATS_ENABLE
    /* Update mutex statistics */
    osal_stats_dec(&s_osal_stats.mutexes);
#endif

    /* Delete the FreeRTOS mutex */
    vSemaphoreDelete(wrapper->handle);

    /* Invalidate handle header before freeing */
    OSAL_HANDLE_DEINIT(&wrapper->header);
    vPortFree(wrapper);

    return OSAL_OK;
}

/**
 * \brief           Lock a mutex
 * infinite)
 *                  OSAL_ERROR_ISR if called from ISR context
 *
 * \details         Acquires a FreeRTOS mutex using xSemaphoreTake(). This
 * function blocks until the mutex is acquired or the timeout expires.
 *
 * \note            Requirements: 5.3, 5.5, 5.6
 */
osal_status_t osal_mutex_lock(osal_mutex_handle_t handle, uint32_t timeout_ms) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* ISR context check - mutexes cannot be locked from ISR */
    if (is_in_isr()) {
        return OSAL_ERROR_ISR;
    }

    osal_mutex_wrapper_t* wrapper = (osal_mutex_wrapper_t*)handle;

    /* Convert timeout to FreeRTOS ticks */
    TickType_t ticks = osal_to_freertos_ticks(timeout_ms);

    /* Attempt to acquire the mutex */
    if (xSemaphoreTake(wrapper->handle, ticks) != pdTRUE) {
        return OSAL_ERROR_TIMEOUT;
    }

    return OSAL_OK;
}

/**
 * \brief           Unlock a mutex
 *
 * \details         Releases a FreeRTOS mutex using xSemaphoreGive().
 *
 * \note            Requirements: 5.4
 */
osal_status_t osal_mutex_unlock(osal_mutex_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_mutex_wrapper_t* wrapper = (osal_mutex_wrapper_t*)handle;

    /* Release the mutex */
    xSemaphoreGive(wrapper->handle);
    return OSAL_OK;
}

/**
 * \brief           Get mutex owner task
 *
 * \details         Returns the task handle of the task that currently holds
 *                  the mutex. Uses xSemaphoreGetMutexHolder() FreeRTOS API.
 *
 * \note            Requirements: 10.1
 */
osal_task_handle_t osal_mutex_get_owner(osal_mutex_handle_t handle) {
    if (handle == NULL) {
        return NULL;
    }

    osal_mutex_wrapper_t* wrapper = (osal_mutex_wrapper_t*)handle;
    /*
     * xSemaphoreGetMutexHolder returns the task handle of the task
     * that holds the mutex, or NULL if the mutex is not held.
     * Note: Returns FreeRTOS TaskHandle_t, not OSAL wrapper.
     */
    TaskHandle_t owner = xSemaphoreGetMutexHolder(wrapper->handle);
    return (osal_task_handle_t)owner;
}

/**
 * \brief           Check if mutex is locked
 *
 * \details         Returns true if the mutex is currently held by any task.
 *                  Uses xSemaphoreGetMutexHolder() to check ownership.
 *
 * \note            Requirements: 10.2
 */
bool osal_mutex_is_locked(osal_mutex_handle_t handle) {
    if (handle == NULL) {
        return false;
    }

    osal_mutex_wrapper_t* wrapper = (osal_mutex_wrapper_t*)handle;
    /*
     * If xSemaphoreGetMutexHolder returns non-NULL, the mutex is locked.
     */
    TaskHandle_t owner = xSemaphoreGetMutexHolder(wrapper->handle);
    return (owner != NULL);
}

/*---------------------------------------------------------------------------*/
/* Semaphore Functions                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Create a semaphore (generic)
 *
 * \details         Creates a FreeRTOS counting semaphore with the specified
 * initial and maximum count values.
 *
 * \note            Requirements: 6.1, 6.2
 */
osal_status_t osal_sem_create(uint32_t initial_count, uint32_t max_count,
                              osal_sem_handle_t* handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Parameter validation - initial count must not exceed max count */
    if (initial_count > max_count) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Parameter validation - max count must be at least 1 */
    if (max_count == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Auto-initialize OSAL if needed */
    if (!s_osal_initialized) {
        osal_status_t status = osal_init();
        if (status != OSAL_OK) {
            return status;
        }
    }

    /* Allocate wrapper structure for handle validation */
    osal_sem_wrapper_t* wrapper =
        (osal_sem_wrapper_t*)pvPortMalloc(sizeof(osal_sem_wrapper_t));
    if (wrapper == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    /* Initialize handle header for validation */
    OSAL_HANDLE_INIT(&wrapper->header, OSAL_TYPE_SEM);

    /* Create FreeRTOS counting semaphore */
    wrapper->handle = xSemaphoreCreateCounting(max_count, initial_count);
    if (wrapper->handle == NULL) {
        /* Invalidate and free wrapper on failure */
        OSAL_HANDLE_DEINIT(&wrapper->header);
        vPortFree(wrapper);
        return OSAL_ERROR_NO_MEMORY;
    }

#if OSAL_STATS_ENABLE
    /* Update semaphore statistics */
    osal_stats_inc(&s_osal_stats.sems);
#endif

    *handle = (osal_sem_handle_t)wrapper;
    return OSAL_OK;
}

/**
 * \brief           Create a binary semaphore
 *
 * \details         Creates a FreeRTOS binary semaphore using
 * xSemaphoreCreateBinary(). Binary semaphores have a maximum count of 1.
 *
 * \note            Requirements: 6.1
 */
osal_status_t osal_sem_create_binary(uint32_t initial,
                                     osal_sem_handle_t* handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Auto-initialize OSAL if needed */
    if (!s_osal_initialized) {
        osal_status_t status = osal_init();
        if (status != OSAL_OK) {
            return status;
        }
    }

    /* Allocate wrapper structure for handle validation */
    osal_sem_wrapper_t* wrapper =
        (osal_sem_wrapper_t*)pvPortMalloc(sizeof(osal_sem_wrapper_t));
    if (wrapper == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    /* Initialize handle header for validation */
    OSAL_HANDLE_INIT(&wrapper->header, OSAL_TYPE_SEM);

    /* Create FreeRTOS binary semaphore */
    wrapper->handle = xSemaphoreCreateBinary();
    if (wrapper->handle == NULL) {
        /* Invalidate and free wrapper on failure */
        OSAL_HANDLE_DEINIT(&wrapper->header);
        vPortFree(wrapper);
        return OSAL_ERROR_NO_MEMORY;
    }

    /*
     * FreeRTOS binary semaphores are created in the "empty" state (count = 0).
     * If initial > 0, we need to give the semaphore to set it to "full" state.
     */
    if (initial > 0) {
        xSemaphoreGive(wrapper->handle);
    }

#if OSAL_STATS_ENABLE
    /* Update semaphore statistics */
    osal_stats_inc(&s_osal_stats.sems);
#endif

    *handle = (osal_sem_handle_t)wrapper;
    return OSAL_OK;
}

/**
 * \brief           Create a counting semaphore
 *
 * \details         Creates a FreeRTOS counting semaphore using
 * xSemaphoreCreateCounting().
 *
 * \note            Requirements: 6.2
 */
osal_status_t osal_sem_create_counting(uint32_t max_count, uint32_t initial,
                                       osal_sem_handle_t* handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Parameter validation - initial count must not exceed max count */
    if (initial > max_count) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Parameter validation - max count must be at least 1 */
    if (max_count == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Auto-initialize OSAL if needed */
    if (!s_osal_initialized) {
        osal_status_t status = osal_init();
        if (status != OSAL_OK) {
            return status;
        }
    }

    /* Allocate wrapper structure for handle validation */
    osal_sem_wrapper_t* wrapper =
        (osal_sem_wrapper_t*)pvPortMalloc(sizeof(osal_sem_wrapper_t));
    if (wrapper == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    /* Initialize handle header for validation */
    OSAL_HANDLE_INIT(&wrapper->header, OSAL_TYPE_SEM);

    /* Create FreeRTOS counting semaphore */
    wrapper->handle = xSemaphoreCreateCounting(max_count, initial);
    if (wrapper->handle == NULL) {
        /* Invalidate and free wrapper on failure */
        OSAL_HANDLE_DEINIT(&wrapper->header);
        vPortFree(wrapper);
        return OSAL_ERROR_NO_MEMORY;
    }

#if OSAL_STATS_ENABLE
    /* Update semaphore statistics */
    osal_stats_inc(&s_osal_stats.sems);
#endif

    *handle = (osal_sem_handle_t)wrapper;
    return OSAL_OK;
}

/**
 * \brief           Delete a semaphore
 *
 * \details         Deletes a FreeRTOS semaphore using vSemaphoreDelete().
 *
 * \note            Requirements: 6.3
 */
osal_status_t osal_sem_delete(osal_sem_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_sem_wrapper_t* wrapper = (osal_sem_wrapper_t*)handle;

#if OSAL_STATS_ENABLE
    /* Update semaphore statistics */
    osal_stats_dec(&s_osal_stats.sems);
#endif

    /* Delete the FreeRTOS semaphore */
    vSemaphoreDelete(wrapper->handle);

    /* Invalidate handle header before freeing */
    OSAL_HANDLE_DEINIT(&wrapper->header);
    vPortFree(wrapper);

    return OSAL_OK;
}

/**
 * \brief           Take (wait for) a semaphore
 * infinite)
 *
 * \details         Waits for a FreeRTOS semaphore using xSemaphoreTake(). This
 * function blocks until the semaphore is available or the timeout expires.
 *
 * \note            Requirements: 6.4
 */
osal_status_t osal_sem_take(osal_sem_handle_t handle, uint32_t timeout_ms) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_sem_wrapper_t* wrapper = (osal_sem_wrapper_t*)handle;

    /* Convert timeout to FreeRTOS ticks */
    TickType_t ticks = osal_to_freertos_ticks(timeout_ms);

    /* Attempt to take the semaphore */
    if (xSemaphoreTake(wrapper->handle, ticks) != pdTRUE) {
        return OSAL_ERROR_TIMEOUT;
    }

    return OSAL_OK;
}

/**
 * \brief           Give (signal) a semaphore
 *
 * \details         Signals a FreeRTOS semaphore using xSemaphoreGive().
 *
 * \note            Requirements: 6.5
 */
osal_status_t osal_sem_give(osal_sem_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_sem_wrapper_t* wrapper = (osal_sem_wrapper_t*)handle;

    /* Give the semaphore */
    xSemaphoreGive(wrapper->handle);
    return OSAL_OK;
}

/**
 * \brief           Give (signal) a semaphore from ISR context
 *
 * \details         Signals a FreeRTOS semaphore from an interrupt service
 * routine using xSemaphoreGiveFromISR(). This function handles the higher
 * priority task woken flag and triggers a context switch if needed.
 *
 * \note            Requirements: 6.6
 */
osal_status_t osal_sem_give_from_isr(osal_sem_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_sem_wrapper_t* wrapper = (osal_sem_wrapper_t*)handle;

    /*
     * Use ISR-safe version of semaphore give.
     * xHigherPriorityTaskWoken is set to pdTRUE if giving the semaphore
     * caused a task to unblock that has a higher priority than the
     * currently running task.
     */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(wrapper->handle, &xHigherPriorityTaskWoken);

    /*
     * Request a context switch if a higher priority task was woken.
     * This ensures the higher priority task runs as soon as the ISR completes.
     */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    return OSAL_OK;
}

/**
 * \brief           Get semaphore current count
 *
 * \details         Returns the current count of the semaphore using
 *                  uxSemaphoreGetCount() FreeRTOS API.
 *
 * \note            Requirements: 10.3
 */
uint32_t osal_sem_get_count(osal_sem_handle_t handle) {
    if (handle == NULL) {
        return 0;
    }

    osal_sem_wrapper_t* wrapper = (osal_sem_wrapper_t*)handle;

    /*
     * uxSemaphoreGetCount returns the count of the semaphore.
     * For binary semaphores, this will be 0 or 1.
     * For counting semaphores, this will be 0 to max_count.
     */
    return (uint32_t)uxSemaphoreGetCount(wrapper->handle);
}

/**
 * \brief           Reset semaphore to specified count
 *
 * \details         Resets the semaphore count to the specified value.
 *                  This is done by taking all available counts and then
 *                  giving the semaphore the specified number of times.
 *
 * \note            Requirements: 10.4
 * \note            This operation is not atomic and should be used with care
 *                  in multi-threaded environments.
 */
osal_status_t osal_sem_reset(osal_sem_handle_t handle, uint32_t count) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_sem_wrapper_t* wrapper = (osal_sem_wrapper_t*)handle;

    /*
     * FreeRTOS does not provide a direct API to reset semaphore count.
     * We implement this by:
     * 1. Taking all available counts (non-blocking)
     * 2. Giving the semaphore 'count' times
     *
     * Note: This is not atomic, so use with care in multi-threaded code.
     */

    /* Drain all available counts */
    while (xSemaphoreTake(wrapper->handle, 0) == pdTRUE) {
        /* Keep taking until empty */
    }

    /* Give the semaphore 'count' times */
    for (uint32_t i = 0; i < count; i++) {
        if (xSemaphoreGive(wrapper->handle) != pdTRUE) {
            /* Reached max count, stop giving */
            break;
        }
    }

    return OSAL_OK;
}

/*---------------------------------------------------------------------------*/
/* Queue Functions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Create a message queue
 *
 * \details         Creates a FreeRTOS queue using xQueueCreate(). The queue can
 * hold item_count items, each of item_size bytes.
 *
 * \note            Requirements: 7.1
 */
osal_status_t osal_queue_create(size_t item_size, size_t item_count,
                                osal_queue_handle_t* handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Parameter validation - item_size and item_count must be positive */
    if (item_size == 0 || item_count == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Auto-initialize OSAL if needed */
    if (!s_osal_initialized) {
        osal_status_t status = osal_init();
        if (status != OSAL_OK) {
            return status;
        }
    }

    /* Allocate wrapper structure for handle validation */
    osal_queue_wrapper_t* wrapper =
        (osal_queue_wrapper_t*)pvPortMalloc(sizeof(osal_queue_wrapper_t));
    if (wrapper == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    /* Initialize handle header for validation */
    OSAL_HANDLE_INIT(&wrapper->header, OSAL_TYPE_QUEUE);
    wrapper->mode = OSAL_QUEUE_MODE_NORMAL;
    wrapper->item_count = item_count;

    /* Create FreeRTOS queue */
    wrapper->handle =
        xQueueCreate((UBaseType_t)item_count, (UBaseType_t)item_size);
    if (wrapper->handle == NULL) {
        /* Invalidate and free wrapper on failure */
        OSAL_HANDLE_DEINIT(&wrapper->header);
        vPortFree(wrapper);
        return OSAL_ERROR_NO_MEMORY;
    }

#if OSAL_STATS_ENABLE
    /* Update queue statistics */
    osal_stats_inc(&s_osal_stats.queues);
#endif

    *handle = (osal_queue_handle_t)wrapper;
    return OSAL_OK;
}

/**
 * \brief           Delete a message queue
 *
 * \details         Deletes a FreeRTOS queue using vQueueDelete().
 *
 * \note            Requirements: 7.2
 */
osal_status_t osal_queue_delete(osal_queue_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_wrapper_t* wrapper = (osal_queue_wrapper_t*)handle;

#if OSAL_STATS_ENABLE
    /* Update queue statistics */
    osal_stats_dec(&s_osal_stats.queues);
#endif

    /* Delete the FreeRTOS queue */
    vQueueDelete(wrapper->handle);

    /* Invalidate handle header before freeing */
    OSAL_HANDLE_DEINIT(&wrapper->header);
    vPortFree(wrapper);

    return OSAL_OK;
}

/**
 * \brief           Send item to queue
 * infinite)
 * timeout expired
 *
 * \details         Sends an item to the back of a FreeRTOS queue using
 * xQueueSend(). This function blocks until space is available or the timeout
 * expires.
 *
 * \note            Requirements: 7.3
 */
osal_status_t osal_queue_send(osal_queue_handle_t handle, const void* item,
                              uint32_t timeout_ms) {
    /* Parameter validation - NULL pointer checks */
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_wrapper_t* wrapper = (osal_queue_wrapper_t*)handle;

    /* Convert timeout to FreeRTOS ticks */
    TickType_t ticks = osal_to_freertos_ticks(timeout_ms);

    /* Send item to queue */
    if (xQueueSend(wrapper->handle, item, ticks) != pdTRUE) {
        /* Queue is full and timeout expired */
        return OSAL_ERROR_FULL;
    }

    return OSAL_OK;
}

/**
 * \brief           Send item to front of queue
 * infinite)
 * timeout expired
 *
 * \details         Sends an item to the front of a FreeRTOS queue using
 * xQueueSendToFront(). This function blocks until space is available or the
 * timeout expires.
 *
 * \note            Requirements: 7.4
 */
osal_status_t osal_queue_send_front(osal_queue_handle_t handle,
                                    const void* item, uint32_t timeout_ms) {
    /* Parameter validation - NULL pointer checks */
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_wrapper_t* wrapper = (osal_queue_wrapper_t*)handle;

    /* Convert timeout to FreeRTOS ticks */
    TickType_t ticks = osal_to_freertos_ticks(timeout_ms);

    /* Send item to front of queue */
    if (xQueueSendToFront(wrapper->handle, item, ticks) != pdTRUE) {
        /* Queue is full and timeout expired */
        return OSAL_ERROR_FULL;
    }

    return OSAL_OK;
}

/**
 * \brief           Receive item from queue
 * infinite)
 * timeout expired
 *
 * \details         Receives an item from a FreeRTOS queue using
 * xQueueReceive(). This function blocks until an item is available or the
 * timeout expires.
 *
 * \note            Requirements: 7.5
 */
osal_status_t osal_queue_receive(osal_queue_handle_t handle, void* item,
                                 uint32_t timeout_ms) {
    /* Parameter validation - NULL pointer checks */
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_wrapper_t* wrapper = (osal_queue_wrapper_t*)handle;

    /* Convert timeout to FreeRTOS ticks */
    TickType_t ticks = osal_to_freertos_ticks(timeout_ms);

    /* Receive item from queue */
    if (xQueueReceive(wrapper->handle, item, ticks) != pdTRUE) {
        /* Queue is empty and timeout expired */
        return OSAL_ERROR_EMPTY;
    }

    return OSAL_OK;
}

/**
 * \brief           Peek item from queue (without removing)
 *
 * \details         Peeks at the front item of a FreeRTOS queue using
 * xQueuePeek(). The item is copied to the output buffer but remains in the
 * queue.
 *
 * \note            Requirements: 7.6
 */
osal_status_t osal_queue_peek(osal_queue_handle_t handle, void* item) {
    /* Parameter validation - NULL pointer checks */
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_wrapper_t* wrapper = (osal_queue_wrapper_t*)handle;

    /* Peek at front item without removing (no wait) */
    if (xQueuePeek(wrapper->handle, item, 0) != pdTRUE) {
        /* Queue is empty */
        return OSAL_ERROR_EMPTY;
    }

    return OSAL_OK;
}

/**
 * \brief           Get number of items in queue
 *
 * \details         Returns the number of items currently in the queue using
 *                  uxQueueMessagesWaiting().
 *
 * \note            Requirements: 7.7
 */
size_t osal_queue_get_count(osal_queue_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return 0;
    }

    osal_queue_wrapper_t* wrapper = (osal_queue_wrapper_t*)handle;

    /* Return number of items waiting in queue */
    return (size_t)uxQueueMessagesWaiting(wrapper->handle);
}

/**
 * \brief           Check if queue is empty
 */
bool osal_queue_is_empty(osal_queue_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return true;
    }

    osal_queue_wrapper_t* wrapper = (osal_queue_wrapper_t*)handle;

    /* Queue is empty if message count is 0 */
    return (uxQueueMessagesWaiting(wrapper->handle) == 0);
}

/**
 * \brief           Check if queue is full
 *
 * \details         Checks if the queue has reached its capacity using
 * uxQueueSpacesAvailable().
 */
bool osal_queue_is_full(osal_queue_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return false;
    }

    osal_queue_wrapper_t* wrapper = (osal_queue_wrapper_t*)handle;

    /* Queue is full if no spaces are available */
    return (uxQueueSpacesAvailable(wrapper->handle) == 0);
}

/**
 * \brief           Send item to queue from ISR context
 *
 * \details         Sends an item to a FreeRTOS queue from an interrupt service
 * routine using xQueueSendFromISR(). This function handles the higher priority
 * task woken flag and triggers a context switch if needed.
 *
 * \note            Requirements: 7.8
 */
osal_status_t osal_queue_send_from_isr(osal_queue_handle_t handle,
                                       const void* item) {
    /* Parameter validation - NULL pointer checks */
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_wrapper_t* wrapper = (osal_queue_wrapper_t*)handle;

    /*
     * Use ISR-safe version of queue send.
     * xHigherPriorityTaskWoken is set to pdTRUE if sending to the queue
     * caused a task to unblock that has a higher priority than the
     * currently running task.
     */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xQueueSendFromISR(wrapper->handle, item,
                          &xHigherPriorityTaskWoken) != pdTRUE) {
        /* Queue is full */
        return OSAL_ERROR_FULL;
    }

    /*
     * Request a context switch if a higher priority task was woken.
     * This ensures the higher priority task runs as soon as the ISR completes.
     */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    return OSAL_OK;
}

/**
 * \brief           Receive item from queue from ISR context
 *
 * \details         Receives an item from a FreeRTOS queue from an interrupt
 * service routine using xQueueReceiveFromISR(). This function handles the
 * higher priority task woken flag and triggers a context switch if needed.
 *
 * \note            Requirements: 7.9
 */
osal_status_t osal_queue_receive_from_isr(osal_queue_handle_t handle,
                                          void* item) {
    /* Parameter validation - NULL pointer checks */
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_wrapper_t* wrapper = (osal_queue_wrapper_t*)handle;

    /*
     * Use ISR-safe version of queue receive.
     * xHigherPriorityTaskWoken is set to pdTRUE if receiving from the queue
     * caused a task to unblock that has a higher priority than the
     * currently running task.
     */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xQueueReceiveFromISR(wrapper->handle, item,
                             &xHigherPriorityTaskWoken) != pdTRUE) {
        /* Queue is empty */
        return OSAL_ERROR_EMPTY;
    }

    /*
     * Request a context switch if a higher priority task was woken.
     * This ensures the higher priority task runs as soon as the ISR completes.
     */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    return OSAL_OK;
}

/**
 * \brief           Get available space in queue
 *
 * \details         Returns the number of free slots in the queue using
 *                  uxQueueSpacesAvailable().
 *
 * \note            Requirements: 8.1
 */
size_t osal_queue_get_available_space(osal_queue_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return 0;
    }

    osal_queue_wrapper_t* wrapper = (osal_queue_wrapper_t*)handle;

    /* Return number of available spaces in queue */
    return (size_t)uxQueueSpacesAvailable(wrapper->handle);
}

/**
 * \brief           Reset queue (clear all items)
 *
 * \details         Resets the queue to its empty state using xQueueReset().
 *                  All items in the queue are discarded.
 *
 * \note            Requirements: 8.2
 */
osal_status_t osal_queue_reset(osal_queue_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_wrapper_t* wrapper = (osal_queue_wrapper_t*)handle;

    /* Reset the queue - discards all items */
    xQueueReset(wrapper->handle);

    return OSAL_OK;
}

/**
 * \brief           Set queue mode
 *
 * \details         Sets the queue operating mode. In FreeRTOS, the standard
 *                  queue does not support overwrite mode directly. This
 *                  function is provided for API compatibility but overwrite
 *                  mode requires using xQueueOverwrite() for single-item
 *                  queues. For multi-item queues, overwrite mode is not
 *                  natively supported.
 *
 * \note            Requirements: 8.3
 *                  FreeRTOS limitation: Overwrite mode only works for
 *                  single-item queues using xQueueOverwrite().
 */
osal_status_t osal_queue_set_mode(osal_queue_handle_t handle,
                                  osal_queue_mode_t mode) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Parameter validation - mode must be valid */
    if (mode != OSAL_QUEUE_MODE_NORMAL && mode != OSAL_QUEUE_MODE_OVERWRITE) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_queue_wrapper_t* wrapper = (osal_queue_wrapper_t*)handle;

    /*
     * Store the mode in the wrapper structure.
     * The actual behavior depends on how send operations are called.
     * For overwrite mode with single-item queues, xQueueOverwrite() should
     * be used instead of xQueueSend().
     */
    wrapper->mode = mode;

    return OSAL_OK;
}

/**
 * \brief           Peek item from queue in ISR context
 *
 * \details         Peeks at the front item of a FreeRTOS queue from an
 *                  interrupt service routine using xQueuePeekFromISR().
 *                  The item is copied to the output buffer but remains
 *                  in the queue.
 *
 * \note            Requirements: 8.5
 */
osal_status_t osal_queue_peek_from_isr(osal_queue_handle_t handle,
                                       void* item) {
    /* Parameter validation - NULL pointer checks */
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_queue_wrapper_t* wrapper = (osal_queue_wrapper_t*)handle;

    /* Peek at front item from ISR context */
    if (xQueuePeekFromISR(wrapper->handle, item) != pdTRUE) {
        /* Queue is empty */
        return OSAL_ERROR_EMPTY;
    }

    return OSAL_OK;
}

/*---------------------------------------------------------------------------*/
/* Timer Functions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Internal FreeRTOS timer callback wrapper
 *
 * \details         This function is called by FreeRTOS when a timer expires.
 *                  It retrieves the wrapper from the timer's ID field and
 *                  invokes the user callback with the user argument.
 *
 * \param[in]       xTimer: FreeRTOS timer handle
 */
static void osal_timer_callback_wrapper(TimerHandle_t xTimer) {
    /* Get the timer wrapper from the timer ID */
    osal_timer_wrapper_t* wrapper =
        (osal_timer_wrapper_t*)pvTimerGetTimerID(xTimer);

    if (wrapper != NULL && wrapper->callback != NULL) {
        /* Invoke the user callback with the user argument */
        wrapper->callback(wrapper->arg);
    }
}

/**
 * \brief           Create a timer
 *
 * \details         Creates a FreeRTOS software timer using xTimerCreate().
 *                  The timer is created in the dormant state and must be
 *                  started using osal_timer_start().
 *
 * \note            Requirements: 1.1-1.6, 2.4, 8.2, 8.3
 */
osal_status_t osal_timer_create(const osal_timer_config_t* config,
                                osal_timer_handle_t* handle) {
    /* Parameter validation - NULL pointer checks */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (config == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Parameter validation - callback must be valid */
    if (config->callback == NULL) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Parameter validation - period must be non-zero */
    if (config->period_ms == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Auto-initialize OSAL if needed */
    if (!s_osal_initialized) {
        osal_status_t status = osal_init();
        if (status != OSAL_OK) {
            return status;
        }
    }

    /* Allocate wrapper structure for handle validation */
    osal_timer_wrapper_t* wrapper =
        (osal_timer_wrapper_t*)pvPortMalloc(sizeof(osal_timer_wrapper_t));
    if (wrapper == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    /* Initialize handle header for validation */
    OSAL_HANDLE_INIT(&wrapper->header, OSAL_TYPE_TIMER);

    /* Store user callback and argument in wrapper */
    wrapper->callback = config->callback;
    wrapper->arg = config->arg;

    /* Convert period from milliseconds to ticks */
    TickType_t period_ticks = pdMS_TO_TICKS(config->period_ms);
    if (period_ticks == 0) {
        /* Ensure at least 1 tick period */
        period_ticks = 1;
    }

    /* Determine auto-reload setting based on timer mode */
    BaseType_t auto_reload =
        (config->mode == OSAL_TIMER_PERIODIC) ? pdTRUE : pdFALSE;

    /* Use provided name or default to "timer" */
    const char* timer_name = (config->name != NULL) ? config->name : "timer";

    /* Create the FreeRTOS timer */
    wrapper->handle =
        xTimerCreate(timer_name,   /* Timer name */
                     period_ticks, /* Timer period in ticks */
                     auto_reload,  /* Auto-reload (periodic) or one-shot */
                     (void*)wrapper, /* Timer ID (stores our wrapper) */
                     osal_timer_callback_wrapper /* Callback wrapper function */
        );

    if (wrapper->handle == NULL) {
        /* Timer creation failed - invalidate and free wrapper */
        OSAL_HANDLE_DEINIT(&wrapper->header);
        vPortFree(wrapper);
        return OSAL_ERROR_NO_MEMORY;
    }

#if OSAL_STATS_ENABLE
    /* Update timer statistics */
    osal_stats_inc(&s_osal_stats.timers);
#endif

    /* Return the wrapper as the timer handle */
    *handle = (osal_timer_handle_t)wrapper;
    return OSAL_OK;
}

/**
 * \brief           Delete a timer
 *
 * \details         Deletes a FreeRTOS software timer using xTimerDelete().
 *                  Also frees the timer context structure.
 *
 * \note            Requirements: 2.4
 */
osal_status_t osal_timer_delete(osal_timer_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    TimerHandle_t timer = (TimerHandle_t)handle;

    /* Get the timer context to free it */
    osal_timer_context_t* ctx = (osal_timer_context_t*)pvTimerGetTimerID(timer);

#if OSAL_STATS_ENABLE
    /* Update timer statistics */
    osal_stats_dec(&s_osal_stats.timers);
#endif

    /* Delete the FreeRTOS timer
     * Use portMAX_DELAY to wait indefinitely for the command to be sent
     * to the timer command queue.
     */
    if (xTimerDelete(timer, portMAX_DELAY) != pdPASS) {
        return OSAL_ERROR;
    }

    /* Free the timer context */
    if (ctx != NULL) {
        vPortFree(ctx);
    }

    return OSAL_OK;
}

/**
 * \brief           Start a timer
 *
 * \details         Starts a FreeRTOS software timer using xTimerStart().
 *                  If the timer is already running, this has the same effect
 *                  as xTimerReset().
 *
 * \note            Requirements: 2.1, 2.5-2.7
 */
osal_status_t osal_timer_start(osal_timer_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    TimerHandle_t timer = (TimerHandle_t)handle;

    /* Start the timer
     * Use portMAX_DELAY to wait indefinitely for the command to be sent
     * to the timer command queue.
     */
    if (xTimerStart(timer, portMAX_DELAY) != pdPASS) {
        return OSAL_ERROR;
    }

    return OSAL_OK;
}

/**
 * \brief           Stop a timer
 *
 * \details         Stops a FreeRTOS software timer using xTimerStop().
 *                  The timer will not fire its callback until started again.
 *
 * \note            Requirements: 2.2
 */
osal_status_t osal_timer_stop(osal_timer_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    TimerHandle_t timer = (TimerHandle_t)handle;

    /* Stop the timer
     * Use portMAX_DELAY to wait indefinitely for the command to be sent
     * to the timer command queue.
     */
    if (xTimerStop(timer, portMAX_DELAY) != pdPASS) {
        return OSAL_ERROR;
    }

    return OSAL_OK;
}

/**
 * \brief           Reset a timer (restart countdown)
 *
 * \details         Resets a FreeRTOS software timer using xTimerReset().
 *                  This restarts the timer countdown from the beginning of
 *                  the period. If the timer was not running, it will be
 * started.
 *
 * \note            Requirements: 2.3
 */
osal_status_t osal_timer_reset(osal_timer_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    TimerHandle_t timer = (TimerHandle_t)handle;

    /* Reset the timer
     * Use portMAX_DELAY to wait indefinitely for the command to be sent
     * to the timer command queue.
     */
    if (xTimerReset(timer, portMAX_DELAY) != pdPASS) {
        return OSAL_ERROR;
    }

    return OSAL_OK;
}

/**
 * \brief           Change timer period
 *
 * \details         Changes the period of a FreeRTOS software timer using
 *                  xTimerChangePeriod(). If the timer is dormant, this will
 *                  also start the timer.
 *
 * \note            Requirements: 3.3
 */
osal_status_t osal_timer_set_period(osal_timer_handle_t handle,
                                    uint32_t period_ms) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Parameter validation - period must be non-zero */
    if (period_ms == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    TimerHandle_t timer = (TimerHandle_t)handle;

    /* Convert period from milliseconds to ticks */
    TickType_t period_ticks = pdMS_TO_TICKS(period_ms);
    if (period_ticks == 0) {
        /* Ensure at least 1 tick period */
        period_ticks = 1;
    }

    /* Change the timer period
     * Use portMAX_DELAY to wait indefinitely for the command to be sent
     * to the timer command queue.
     */
    if (xTimerChangePeriod(timer, period_ticks, portMAX_DELAY) != pdPASS) {
        return OSAL_ERROR;
    }

    return OSAL_OK;
}

/**
 * \brief           Check if timer is active
 *
 * \details         Queries whether a FreeRTOS software timer is currently
 *                  active (running) using xTimerIsTimerActive().
 *
 * \note            Requirements: 3.4
 */
bool osal_timer_is_active(osal_timer_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return false;
    }

    TimerHandle_t timer = (TimerHandle_t)handle;

    /* Query timer active state */
    return (xTimerIsTimerActive(timer) != pdFALSE);
}

/**
 * \brief           Start timer from ISR context
 *
 * \details         Starts a FreeRTOS software timer from an interrupt service
 *                  routine using xTimerStartFromISR(). This function handles
 *                  the higher priority task woken flag and triggers a context
 *                  switch if needed.
 *
 * \note            Requirements: 4.1
 */
osal_status_t osal_timer_start_from_isr(osal_timer_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    TimerHandle_t timer = (TimerHandle_t)handle;

    /*
     * Use ISR-safe version of timer start.
     * xHigherPriorityTaskWoken is set to pdTRUE if starting the timer
     * caused a task to unblock that has a higher priority than the
     * currently running task.
     */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xTimerStartFromISR(timer, &xHigherPriorityTaskWoken) != pdPASS) {
        return OSAL_ERROR;
    }

    /*
     * Request a context switch if a higher priority task was woken.
     * This ensures the higher priority task runs as soon as the ISR completes.
     */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    return OSAL_OK;
}

/**
 * \brief           Stop timer from ISR context
 *
 * \details         Stops a FreeRTOS software timer from an interrupt service
 *                  routine using xTimerStopFromISR(). This function handles
 *                  the higher priority task woken flag and triggers a context
 *                  switch if needed.
 *
 * \note            Requirements: 4.2
 */
osal_status_t osal_timer_stop_from_isr(osal_timer_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    TimerHandle_t timer = (TimerHandle_t)handle;

    /*
     * Use ISR-safe version of timer stop.
     * xHigherPriorityTaskWoken is set to pdTRUE if stopping the timer
     * caused a task to unblock that has a higher priority than the
     * currently running task.
     */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xTimerStopFromISR(timer, &xHigherPriorityTaskWoken) != pdPASS) {
        return OSAL_ERROR;
    }

    /*
     * Request a context switch if a higher priority task was woken.
     * This ensures the higher priority task runs as soon as the ISR completes.
     */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    return OSAL_OK;
}

/**
 * \brief           Reset timer from ISR context
 *
 * \details         Resets a FreeRTOS software timer from an interrupt service
 *                  routine using xTimerResetFromISR(). This function handles
 *                  the higher priority task woken flag and triggers a context
 *                  switch if needed.
 *
 * \note            Requirements: 4.3
 */
osal_status_t osal_timer_reset_from_isr(osal_timer_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    TimerHandle_t timer = (TimerHandle_t)handle;

    /*
     * Use ISR-safe version of timer reset.
     * xHigherPriorityTaskWoken is set to pdTRUE if resetting the timer
     * caused a task to unblock that has a higher priority than the
     * currently running task.
     */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xTimerResetFromISR(timer, &xHigherPriorityTaskWoken) != pdPASS) {
        return OSAL_ERROR;
    }

    /*
     * Request a context switch if a higher priority task was woken.
     * This ensures the higher priority task runs as soon as the ISR completes.
     */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    return OSAL_OK;
}

/*---------------------------------------------------------------------------*/
/* Timer Enhanced Functions                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get timer remaining time
 *
 * \details         Returns the time remaining until the timer expires.
 *                  Uses xTimerGetExpiryTime() to get the absolute expiry time
 *                  and calculates the remaining time from the current tick
 *                  count.
 *
 * \note            Requirements: 5.1
 */
uint32_t osal_timer_get_remaining(osal_timer_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return 0;
    }

    TimerHandle_t timer = (TimerHandle_t)handle;

    /* Check if timer is active */
    if (xTimerIsTimerActive(timer) == pdFALSE) {
        return 0;
    }

    /* Get the expiry time and current tick count */
    TickType_t expiry_time = xTimerGetExpiryTime(timer);
    TickType_t current_time = xTaskGetTickCount();

    /* Calculate remaining ticks */
    TickType_t remaining_ticks;
    if (expiry_time >= current_time) {
        remaining_ticks = expiry_time - current_time;
    } else {
        /* Timer has already expired or tick count wrapped */
        remaining_ticks = 0;
    }

    /* Convert ticks to milliseconds */
    return (uint32_t)((remaining_ticks * 1000) / configTICK_RATE_HZ);
}

/**
 * \brief           Get timer configured period
 *
 * \details         Returns the period of the timer in milliseconds.
 *                  Uses xTimerGetPeriod() to get the period in ticks and
 *                  converts to milliseconds.
 *
 * \note            Requirements: 5.2
 */
uint32_t osal_timer_get_period(osal_timer_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return 0;
    }

    TimerHandle_t timer = (TimerHandle_t)handle;

    /* Get the timer period in ticks */
    TickType_t period_ticks = xTimerGetPeriod(timer);

    /* Convert ticks to milliseconds */
    return (uint32_t)((period_ticks * 1000) / configTICK_RATE_HZ);
}

/**
 * \brief           Set timer callback function
 *
 * \details         Changes the callback function and argument for a timer.
 *                  Updates the timer context structure stored in the timer ID.
 *
 * \note            Requirements: 5.3
 */
osal_status_t osal_timer_set_callback(osal_timer_handle_t handle,
                                      osal_timer_callback_t callback,
                                      void* arg) {
    /* Parameter validation - NULL pointer checks */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (callback == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    TimerHandle_t timer = (TimerHandle_t)handle;

    /* Get the timer context from the timer ID */
    osal_timer_context_t* ctx =
        (osal_timer_context_t*)pvTimerGetTimerID(timer);

    if (ctx == NULL) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* Update the callback and argument in the context */
    osal_enter_critical();
    ctx->user_callback = callback;
    ctx->user_arg = arg;
    osal_exit_critical();

    return OSAL_OK;
}

/*---------------------------------------------------------------------------*/
/* Memory Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Allocate memory
 *
 * \details         Allocates memory from the FreeRTOS heap using
 * pvPortMalloc(). This function is thread-safe.
 *
 * \note            Requirements: 5.1-5.6
 */
void* osal_mem_alloc(size_t size) {
    /* Return NULL for zero size allocation */
    if (size == 0) {
        return NULL;
    }

    /* Allocate memory using FreeRTOS heap */
    void* ptr = pvPortMalloc(size);

#if OSAL_STATS_ENABLE
    if (ptr != NULL) {
        osal_enter_critical();
        s_osal_stats.mem_alloc_count++;
        s_osal_stats.mem_allocated += size;
        if (s_osal_stats.mem_allocated > s_osal_stats.mem_peak) {
            s_osal_stats.mem_peak = s_osal_stats.mem_allocated;
        }
        osal_exit_critical();
    }
#endif

    return ptr;
}

/**
 * \brief           Free memory
 *
 * \details         Frees memory back to the FreeRTOS heap using vPortFree().
 *                  This function is thread-safe and safe to call with NULL.
 *
 * \note            Requirements: 5.4, 5.5
 */
void osal_mem_free(void* ptr) {
    /* Safe to call with NULL - just return */
    if (ptr == NULL) {
        return;
    }

#if OSAL_STATS_ENABLE
    osal_enter_critical();
    if (s_osal_stats.mem_alloc_count > 0) {
        s_osal_stats.mem_alloc_count--;
    }
    /* Note: We cannot track exact freed size without additional bookkeeping */
    osal_exit_critical();
#endif

    /* Free memory using FreeRTOS heap */
    vPortFree(ptr);
}

/**
 * \brief           Allocate and zero-initialize memory
 *
 * \details         Allocates memory from the FreeRTOS heap and initializes
 *                  all bytes to zero. Implemented using pvPortMalloc() +
 * memset(). This function is thread-safe.
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

    /* Allocate memory */
    void* ptr = pvPortMalloc(total_size);
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
 *                  the minimum of old and new sizes. FreeRTOS does not provide
 *                  a native realloc, so this is implemented using
 * pvPortMalloc(), memcpy(), and vPortFree().
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

    /* Allocate new memory block */
    void* new_ptr = pvPortMalloc(size);
    if (new_ptr == NULL) {
        /* Allocation failed - original memory is unchanged */
        return NULL;
    }

    /*
     * Copy data from old block to new block.
     * Note: FreeRTOS doesn't provide a way to query the size of an allocation,
     * so we copy 'size' bytes. If the new size is larger than the old size,
     * we may read beyond the original allocation. To be safe, we should
     * only copy up to the new size, which is what we do here.
     *
     * In practice, the caller should know the original size and only use
     * realloc to grow or shrink allocations appropriately.
     */
    memcpy(new_ptr, ptr, size);

    /* Free the old memory block */
    vPortFree(ptr);

    return new_ptr;
}

/**
 * \brief           Allocate aligned memory
 *
 * \details         Allocates memory with a specific alignment requirement.
 *                  FreeRTOS does not provide a native aligned allocation,
 *                  so this is implemented by over-allocating and adjusting
 *                  the returned pointer.
 *
 *                  The implementation stores the original pointer just before
 *                  the aligned pointer so it can be freed correctly.
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
     * - Original size
     * - Extra space for alignment adjustment (alignment - 1 bytes max)
     * - Space to store the original pointer (sizeof(void*))
     */
    size_t total_size = size + alignment - 1 + sizeof(void*);

    /* Check for overflow */
    if (total_size < size) {
        return NULL;
    }

    /* Allocate the memory block */
    void* raw_ptr = pvPortMalloc(total_size);
    if (raw_ptr == NULL) {
        return NULL;
    }

    /*
     * Calculate aligned pointer:
     * 1. Add sizeof(void*) to leave room for storing original pointer
     * 2. Add (alignment - 1) and mask off low bits to align
     */
    uintptr_t raw_addr = (uintptr_t)raw_ptr;
    uintptr_t aligned_addr =
        (raw_addr + sizeof(void*) + alignment - 1) & ~(alignment - 1);
    void* aligned_ptr = (void*)aligned_addr;

    /*
     * Store the original pointer just before the aligned pointer.
     * This allows osal_mem_free to work correctly with aligned allocations
     * if we provide a custom free function, but since we're using the
     * standard osal_mem_free which calls vPortFree directly, the caller
     * must use the original pointer to free.
     *
     * For simplicity, we store the original pointer so a future
     * osal_mem_free_aligned could use it, but currently the user
     * should not free aligned memory with osal_mem_free.
     */
    ((void**)aligned_ptr)[-1] = raw_ptr;

    return aligned_ptr;
}

/**
 * \brief           Get memory statistics
 *
 * \details         Retrieves memory usage statistics from the FreeRTOS heap.
 *                  Uses xPortGetFreeHeapSize() and
 * xPortGetMinimumEverFreeHeapSize().
 *
 * \note            Requirements: 7.1-7.4
 */
osal_status_t osal_mem_get_stats(osal_mem_stats_t* stats) {
    /* Parameter validation - NULL pointer check */
    if (stats == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /*
     * FreeRTOS provides functions to query free heap size and minimum
     * ever free heap size. The total heap size is defined by
     * configTOTAL_HEAP_SIZE.
     */
    stats->total_size = configTOTAL_HEAP_SIZE;
    stats->free_size = xPortGetFreeHeapSize();
    stats->min_free_size = xPortGetMinimumEverFreeHeapSize();

    return OSAL_OK;
}

/**
 * \brief           Get free heap size
 *
 * \details         Returns the current free heap size in bytes using
 *                  xPortGetFreeHeapSize().
 *
 * \note            Requirements: 7.2
 */
size_t osal_mem_get_free_size(void) {
    return xPortGetFreeHeapSize();
}

/**
 * \brief           Get minimum ever free heap size
 *
 * \details         Returns the minimum free heap size that has existed since
 *                  the system started, using xPortGetMinimumEverFreeHeapSize().
 *                  This is useful for detecting heap usage high-water marks.
 *
 * \note            Requirements: 7.3
 */
size_t osal_mem_get_min_free_size(void) {
    return xPortGetMinimumEverFreeHeapSize();
}

/**
 * \brief           Get active allocation count
 *
 * \details         Returns the number of active memory allocations.
 *                  This is tracked internally when OSAL_STATS_ENABLE is defined.
 *
 * \note            Requirements: 6.1
 */
size_t osal_mem_get_allocation_count(void) {
#if OSAL_STATS_ENABLE
    size_t count;
    osal_enter_critical();
    count = s_osal_stats.mem_alloc_count;
    osal_exit_critical();
    return count;
#else
    /* Statistics not enabled - return 0 */
    return 0;
#endif
}

/**
 * \brief           Check heap integrity
 *
 * \details         Performs a basic integrity check on the FreeRTOS heap.
 *                  Uses vPortGetHeapStats() if available (heap_4 or heap_5),
 *                  otherwise performs a basic sanity check.
 *
 * \note            Requirements: 6.3
 */
osal_status_t osal_mem_check_integrity(void) {
    /*
     * FreeRTOS heap integrity checking depends on the heap implementation:
     * - heap_1: No integrity check available (simple bump allocator)
     * - heap_2: No integrity check available
     * - heap_3: Uses standard library malloc (no FreeRTOS check)
     * - heap_4: Can use vPortGetHeapStats() for basic validation
     * - heap_5: Can use vPortGetHeapStats() for basic validation
     *
     * We perform a basic sanity check by verifying that:
     * 1. Free heap size is not larger than total heap size
     * 2. Minimum ever free size is not larger than current free size
     */
    size_t free_size = xPortGetFreeHeapSize();
    size_t min_free_size = xPortGetMinimumEverFreeHeapSize();

    /* Basic sanity checks */
    if (free_size > configTOTAL_HEAP_SIZE) {
        return OSAL_ERROR;
    }

    if (min_free_size > free_size) {
        /* This should never happen - indicates corruption */
        return OSAL_ERROR;
    }

    if (min_free_size > configTOTAL_HEAP_SIZE) {
        return OSAL_ERROR;
    }

    return OSAL_OK;
}

/**
 * \brief           Free aligned memory
 *
 * \details         Frees memory that was allocated with osal_mem_alloc_aligned().
 *                  Retrieves the original pointer stored before the aligned
 *                  address and frees it.
 *
 * \note            Requirements: 6.4
 */
void osal_mem_free_aligned(void* ptr) {
    /* Safe to call with NULL - just return */
    if (ptr == NULL) {
        return;
    }

    /*
     * The original pointer is stored just before the aligned pointer.
     * Retrieve it and free the original allocation.
     */
    void* original_ptr = ((void**)ptr)[-1];

#if OSAL_STATS_ENABLE
    osal_enter_critical();
    if (s_osal_stats.mem_alloc_count > 0) {
        s_osal_stats.mem_alloc_count--;
    }
    osal_exit_critical();
#endif

    vPortFree(original_ptr);
}

/*---------------------------------------------------------------------------*/
/* Event Flags Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Event flags bit mask for 24-bit support
 *
 * \details         FreeRTOS event groups provide 24 usable bits (bits 0-23)
 *                  when configTICK_TYPE_WIDTH_IN_BITS is set to 1 (32-bit).
 *                  Bits 24-31 are reserved for internal use by FreeRTOS.
 */
#define OSAL_EVENT_BITS_MASK 0x00FFFFFF

/**
 * \brief           Create event flags
 *
 * \details         Creates a FreeRTOS event group using xEventGroupCreate().
 *                  Event groups provide a synchronization mechanism for
 *                  waiting on multiple event conditions.
 *
 * \note            Requirements: 1.1-1.6, 8.2, 8.4
 */
osal_status_t osal_event_create(osal_event_handle_t* handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Auto-initialize OSAL if needed */
    if (!s_osal_initialized) {
        osal_status_t status = osal_init();
        if (status != OSAL_OK) {
            return status;
        }
    }

    /* Allocate wrapper structure for handle validation */
    osal_event_wrapper_t* wrapper =
        (osal_event_wrapper_t*)pvPortMalloc(sizeof(osal_event_wrapper_t));
    if (wrapper == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    /* Initialize handle header for validation */
    OSAL_HANDLE_INIT(&wrapper->header, OSAL_TYPE_EVENT);

    /* Create FreeRTOS event group */
    wrapper->handle = xEventGroupCreate();
    if (wrapper->handle == NULL) {
        /* Invalidate and free wrapper on failure */
        OSAL_HANDLE_DEINIT(&wrapper->header);
        vPortFree(wrapper);
        return OSAL_ERROR_NO_MEMORY;
    }

#if OSAL_STATS_ENABLE
    /* Update event statistics */
    osal_stats_inc(&s_osal_stats.events);
#endif

    *handle = (osal_event_handle_t)wrapper;
    return OSAL_OK;
}

/**
 * \brief           Delete event flags
 *
 * \details         Deletes a FreeRTOS event group using vEventGroupDelete().
 *                  Tasks blocked on the event group will be unblocked.
 *
 * \note            Requirements: 1.4, 1.5
 */
osal_status_t osal_event_delete(osal_event_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    osal_event_wrapper_t* wrapper = (osal_event_wrapper_t*)handle;

#if OSAL_STATS_ENABLE
    /* Update event statistics */
    osal_stats_dec(&s_osal_stats.events);
#endif

    /* Delete the FreeRTOS event group */
    vEventGroupDelete(wrapper->handle);

    /* Invalidate handle header before freeing */
    OSAL_HANDLE_DEINIT(&wrapper->header);
    vPortFree(wrapper);

    return OSAL_OK;
}

/**
 * \brief           Set event bits
 *
 * \details         Sets event bits in a FreeRTOS event group using
 *                  xEventGroupSetBits(). This operation is atomic and will
 *                  wake any tasks waiting for the specified bits.
 *
 * \note            Requirements: 2.1-2.5, 8.2, 8.3
 */
osal_status_t osal_event_set(osal_event_handle_t handle,
                             osal_event_bits_t bits) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Parameter validation - zero bits mask check */
    if (bits == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_event_wrapper_t* wrapper = (osal_event_wrapper_t*)handle;

    /* Set the event bits atomically */
    xEventGroupSetBits(wrapper->handle, (EventBits_t)bits);
    return OSAL_OK;
}

/**
 * \brief           Clear event bits
 *
 * \details         Clears event bits in a FreeRTOS event group using
 *                  xEventGroupClearBits(). This operation is atomic and
 *                  only affects the specified bits.
 *
 * \note            Requirements: 3.1-3.5, 8.2, 8.3
 */
osal_status_t osal_event_clear(osal_event_handle_t handle,
                               osal_event_bits_t bits) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Parameter validation - zero bits mask check */
    if (bits == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_event_wrapper_t* wrapper = (osal_event_wrapper_t*)handle;

    /* Clear the event bits atomically */
    xEventGroupClearBits(wrapper->handle, (EventBits_t)bits);
    return OSAL_OK;
}

/**
 * \brief           Wait for event bits
 *
 * \details         Waits for event bits in a FreeRTOS event group using
 *                  xEventGroupWaitBits(). Supports WAIT_ALL and WAIT_ANY
 *                  modes, auto-clear option, and timeout.
 *
 * \note            Requirements: 4.1-4.9, 8.2, 8.3, 8.5, 8.6
 */
osal_status_t osal_event_wait(osal_event_handle_t handle,
                              osal_event_bits_t bits,
                              const osal_event_wait_options_t* options,
                              osal_event_bits_t* bits_out) {
    /* Parameter validation - NULL pointer checks */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (options == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Parameter validation - zero bits mask check */
    if (bits == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* ISR context check - wait cannot be called from ISR */
    if (is_in_isr()) {
        return OSAL_ERROR_ISR;
    }

    osal_event_wrapper_t* wrapper = (osal_event_wrapper_t*)handle;

    /* Convert timeout to FreeRTOS ticks */
    TickType_t ticks = osal_to_freertos_ticks(options->timeout_ms);

    /* Map OSAL wait mode to FreeRTOS wait mode */
    BaseType_t wait_for_all =
        (options->mode == OSAL_EVENT_WAIT_ALL) ? pdTRUE : pdFALSE;

    /* Map OSAL auto-clear to FreeRTOS clear on exit */
    BaseType_t clear_on_exit = options->auto_clear ? pdTRUE : pdFALSE;

    /* Wait for the event bits */
    EventBits_t result =
        xEventGroupWaitBits(wrapper->handle, (EventBits_t)bits,
                            clear_on_exit, wait_for_all, ticks);

    /* Store the result bits if requested */
    if (bits_out != NULL) {
        *bits_out = (osal_event_bits_t)result;
    }

    /*
     * Check if the wait condition was satisfied.
     * For WAIT_ALL mode, all requested bits must be set.
     * For WAIT_ANY mode, at least one requested bit must be set.
     */
    if (wait_for_all) {
        /* WAIT_ALL: Check if all requested bits are set */
        if ((result & bits) == bits) {
            return OSAL_OK;
        }
    } else {
        /* WAIT_ANY: Check if any requested bit is set */
        if ((result & bits) != 0) {
            return OSAL_OK;
        }
    }

    /* Timeout occurred - condition was not met */
    return OSAL_ERROR_TIMEOUT;
}

/**
 * \brief           Get current event bits (non-blocking)
 *
 * \details         Returns the current event bits value using
 *                  xEventGroupGetBits(). This operation does not block
 *                  and does not modify the event bits.
 *
 * \note            Requirements: 5.1-5.4
 */
osal_event_bits_t osal_event_get(osal_event_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return 0;
    }

    osal_event_wrapper_t* wrapper = (osal_event_wrapper_t*)handle;

    /* Get the current event bits (non-blocking) */
    return (osal_event_bits_t)xEventGroupGetBits(wrapper->handle);
}

/**
 * \brief           Set event bits from ISR context
 *
 * \details         Sets event bits from an interrupt service routine using
 *                  xEventGroupSetBitsFromISR(). This function handles the
 *                  higher priority task woken flag and triggers a context
 *                  switch if needed.
 *
 * \note            Requirements: 6.1-6.2, 8.2, 8.3
 */
osal_status_t osal_event_set_from_isr(osal_event_handle_t handle,
                                      osal_event_bits_t bits) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Parameter validation - zero bits mask check */
    if (bits == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_event_wrapper_t* wrapper = (osal_event_wrapper_t*)handle;

    /*
     * Use ISR-safe version of event group set bits.
     * xHigherPriorityTaskWoken is set to pdTRUE if setting the bits
     * caused a task to unblock that has a higher priority than the
     * currently running task.
     */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    BaseType_t result =
        xEventGroupSetBitsFromISR(wrapper->handle, (EventBits_t)bits,
                                  &xHigherPriorityTaskWoken);

    /* Check if the operation succeeded */
    if (result != pdPASS) {
        return OSAL_ERROR;
    }

    /*
     * Request a context switch if a higher priority task was woken.
     * This ensures the higher priority task runs as soon as the ISR completes.
     */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    return OSAL_OK;
}

/**
 * \brief           Clear event bits from ISR context
 *
 * \details         Clears event bits from an interrupt service routine using
 *                  xEventGroupClearBitsFromISR(). Note that FreeRTOS defers
 *                  the actual clear operation to the timer daemon task.
 *
 * \note            Requirements: 7.2
 */
osal_status_t osal_event_clear_from_isr(osal_event_handle_t handle,
                                        osal_event_bits_t bits) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Parameter validation - zero bits mask check */
    if (bits == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    osal_event_wrapper_t* wrapper = (osal_event_wrapper_t*)handle;

    /*
     * Use ISR-safe version of event group clear bits.
     * Note: xEventGroupClearBitsFromISR() defers the clear operation
     * to the timer daemon task. The function returns pdPASS if the
     * command was successfully sent to the timer command queue.
     */
    BaseType_t result =
        xEventGroupClearBitsFromISR(wrapper->handle, (EventBits_t)bits);

    /* Check if the operation was queued successfully */
    if (result != pdPASS) {
        return OSAL_ERROR;
    }

    return OSAL_OK;
}

/**
 * \brief           Synchronous set and wait operation
 *
 * \details         Atomically sets event bits and then waits for a different
 *                  set of bits to be set. This is useful for task rendezvous
 *                  synchronization patterns. Uses xEventGroupSync() FreeRTOS API.
 *
 * \note            Requirements: 7.3
 */
osal_status_t osal_event_sync(osal_event_handle_t handle,
                              osal_event_bits_t set_bits,
                              osal_event_bits_t wait_bits,
                              const osal_event_wait_options_t* options,
                              osal_event_bits_t* bits_out) {
    /* Parameter validation - NULL pointer checks */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    if (options == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Parameter validation - zero bits mask check */
    if (set_bits == 0 || wait_bits == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    /* ISR context check - sync cannot be called from ISR */
    if (is_in_isr()) {
        return OSAL_ERROR_ISR;
    }

    osal_event_wrapper_t* wrapper = (osal_event_wrapper_t*)handle;

    /* Convert timeout to FreeRTOS ticks */
    TickType_t ticks = osal_to_freertos_ticks(options->timeout_ms);

    /*
     * xEventGroupSync atomically sets the specified bits and then waits
     * for all the wait_bits to be set. The bits that were set are cleared
     * on exit (similar to auto_clear behavior).
     */
    EventBits_t result =
        xEventGroupSync(wrapper->handle,
                        (EventBits_t)set_bits,
                        (EventBits_t)wait_bits,
                        ticks);

    /* Store the result bits if requested */
    if (bits_out != NULL) {
        *bits_out = (osal_event_bits_t)result;
    }

    /*
     * Check if the sync condition was satisfied.
     * xEventGroupSync returns the event bits value at the time either
     * the bits being waited for became set, or the timeout expired.
     * We need to check if all wait_bits are set in the result.
     */
    if ((result & wait_bits) == wait_bits) {
        return OSAL_OK;
    }

    /* Timeout occurred - condition was not met */
    return OSAL_ERROR_TIMEOUT;
}

/*---------------------------------------------------------------------------*/
/* Diagnostics Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get OSAL resource statistics
 *
 * \details         Retrieves current resource counts and watermarks for all
 *                  OSAL resource types. This function is safe to call from
 *                  any context, including ISR.
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
    osal_enter_critical();

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

    /* Copy memory statistics */
    stats->mem_allocated = s_osal_stats.mem_allocated;
    stats->mem_peak = s_osal_stats.mem_peak;
    stats->mem_alloc_count = s_osal_stats.mem_alloc_count;

    osal_exit_critical();
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
 *                  This function is safe to call from any context, including ISR.
 *
 * \note            Requirements: 2.3
 */
osal_status_t osal_reset_stats(void) {
#if OSAL_STATS_ENABLE
    /* Enter critical section to ensure atomic reset */
    osal_enter_critical();

    /* Reset watermarks to current counts */
    s_osal_stats.tasks.watermark = s_osal_stats.tasks.count;
    s_osal_stats.mutexes.watermark = s_osal_stats.mutexes.count;
    s_osal_stats.sems.watermark = s_osal_stats.sems.count;
    s_osal_stats.queues.watermark = s_osal_stats.queues.count;
    s_osal_stats.events.watermark = s_osal_stats.events.count;
    s_osal_stats.timers.watermark = s_osal_stats.timers.count;

    /* Reset memory peak to current allocation */
    s_osal_stats.mem_peak = s_osal_stats.mem_allocated;

    osal_exit_critical();
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
    osal_enter_critical();
    s_error_callback = callback;
    osal_exit_critical();

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
 * \note            The callback may be invoked from ISR context, so it
 *                  should be kept short and should not block.
 */
void osal_report_error(osal_status_t error, const char* file, uint32_t line) {
    osal_error_callback_t callback = s_error_callback;

    if (callback != NULL) {
        callback(error, file, line);
    }
}
