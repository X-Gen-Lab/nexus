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

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

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
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

/** \brief          Flag indicating if OSAL has been initialized */
static bool s_osal_initialized = false;

/** \brief          Critical section nesting counter for ISR context */
static volatile uint32_t s_isr_critical_nesting = 0;

/** \brief          Saved interrupt mask for ISR critical sections */
static volatile UBaseType_t s_isr_saved_mask = 0;

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

    TaskHandle_t task_handle = NULL;

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
                    &task_handle                  /* Task handle output */
        );

    /* Check if task creation succeeded */
    if (result != pdPASS) {
        return OSAL_ERROR_NO_MEMORY;
    }

    /* Return the task handle */
    *handle = (osal_task_handle_t)task_handle;
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
    /*
     * FreeRTOS vTaskDelete accepts NULL to delete the calling task.
     * No NULL pointer error for this function - NULL is a valid input.
     */
    vTaskDelete((TaskHandle_t)handle);
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
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    vTaskSuspend((TaskHandle_t)handle);
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
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    vTaskResume((TaskHandle_t)handle);
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

    return pcTaskGetName((TaskHandle_t)handle);
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

    /* Create FreeRTOS mutex with priority inheritance */
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    if (mutex == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    *handle = (osal_mutex_handle_t)mutex;
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
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    /* Delete the FreeRTOS mutex */
    vSemaphoreDelete((SemaphoreHandle_t)handle);
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

    /* Convert timeout to FreeRTOS ticks */
    TickType_t ticks = osal_to_freertos_ticks(timeout_ms);

    /* Attempt to acquire the mutex */
    if (xSemaphoreTake((SemaphoreHandle_t)handle, ticks) != pdTRUE) {
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

    /* Release the mutex */
    xSemaphoreGive((SemaphoreHandle_t)handle);
    return OSAL_OK;
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

    /* Create FreeRTOS counting semaphore */
    SemaphoreHandle_t sem = xSemaphoreCreateCounting(max_count, initial_count);
    if (sem == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    *handle = (osal_sem_handle_t)sem;
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

    /* Create FreeRTOS binary semaphore */
    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    if (sem == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    /*
     * FreeRTOS binary semaphores are created in the "empty" state (count = 0).
     * If initial > 0, we need to give the semaphore to set it to "full" state.
     */
    if (initial > 0) {
        xSemaphoreGive(sem);
    }

    *handle = (osal_sem_handle_t)sem;
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

    /* Create FreeRTOS counting semaphore */
    SemaphoreHandle_t sem = xSemaphoreCreateCounting(max_count, initial);
    if (sem == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    *handle = (osal_sem_handle_t)sem;
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

    /* Delete the FreeRTOS semaphore */
    vSemaphoreDelete((SemaphoreHandle_t)handle);
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

    /* Convert timeout to FreeRTOS ticks */
    TickType_t ticks = osal_to_freertos_ticks(timeout_ms);

    /* Attempt to take the semaphore */
    if (xSemaphoreTake((SemaphoreHandle_t)handle, ticks) != pdTRUE) {
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

    /* Give the semaphore */
    xSemaphoreGive((SemaphoreHandle_t)handle);
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

    /*
     * Use ISR-safe version of semaphore give.
     * xHigherPriorityTaskWoken is set to pdTRUE if giving the semaphore
     * caused a task to unblock that has a higher priority than the
     * currently running task.
     */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)handle, &xHigherPriorityTaskWoken);

    /*
     * Request a context switch if a higher priority task was woken.
     * This ensures the higher priority task runs as soon as the ISR completes.
     */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

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

    /* Create FreeRTOS queue */
    QueueHandle_t queue =
        xQueueCreate((UBaseType_t)item_count, (UBaseType_t)item_size);
    if (queue == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    *handle = (osal_queue_handle_t)queue;
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

    /* Delete the FreeRTOS queue */
    vQueueDelete((QueueHandle_t)handle);
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

    /* Convert timeout to FreeRTOS ticks */
    TickType_t ticks = osal_to_freertos_ticks(timeout_ms);

    /* Send item to queue */
    if (xQueueSend((QueueHandle_t)handle, item, ticks) != pdTRUE) {
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

    /* Convert timeout to FreeRTOS ticks */
    TickType_t ticks = osal_to_freertos_ticks(timeout_ms);

    /* Send item to front of queue */
    if (xQueueSendToFront((QueueHandle_t)handle, item, ticks) != pdTRUE) {
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

    /* Convert timeout to FreeRTOS ticks */
    TickType_t ticks = osal_to_freertos_ticks(timeout_ms);

    /* Receive item from queue */
    if (xQueueReceive((QueueHandle_t)handle, item, ticks) != pdTRUE) {
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

    /* Peek at front item without removing (no wait) */
    if (xQueuePeek((QueueHandle_t)handle, item, 0) != pdTRUE) {
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

    /* Return number of items waiting in queue */
    return (size_t)uxQueueMessagesWaiting((QueueHandle_t)handle);
}

/**
 * \brief           Check if queue is empty
 */
bool osal_queue_is_empty(osal_queue_handle_t handle) {
    /* Parameter validation - NULL pointer check */
    if (handle == NULL) {
        return true;
    }

    /* Queue is empty if message count is 0 */
    return (uxQueueMessagesWaiting((QueueHandle_t)handle) == 0);
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

    /* Queue is full if no spaces are available */
    return (uxQueueSpacesAvailable((QueueHandle_t)handle) == 0);
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

    /*
     * Use ISR-safe version of queue send.
     * xHigherPriorityTaskWoken is set to pdTRUE if sending to the queue
     * caused a task to unblock that has a higher priority than the
     * currently running task.
     */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xQueueSendFromISR((QueueHandle_t)handle, item,
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

    /*
     * Use ISR-safe version of queue receive.
     * xHigherPriorityTaskWoken is set to pdTRUE if receiving from the queue
     * caused a task to unblock that has a higher priority than the
     * currently running task.
     */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xQueueReceiveFromISR((QueueHandle_t)handle, item,
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
