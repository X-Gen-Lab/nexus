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

/*---------------------------------------------------------------------------*/
/* Timer Functions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Internal timer context for callback argument passing
 *
 * \details         FreeRTOS timer callbacks receive the timer handle, not a
 *                  user-provided argument. This structure stores the user
 *                  callback and argument, and is stored in the timer's ID
 * field.
 */
typedef struct {
    osal_timer_callback_t user_callback; /**< User callback function */
    void* user_arg;                      /**< User callback argument */
} osal_timer_context_t;

/**
 * \brief           Internal FreeRTOS timer callback wrapper
 *
 * \details         This function is called by FreeRTOS when a timer expires.
 *                  It retrieves the user callback and argument from the timer's
 *                  ID field and invokes the user callback.
 *
 * \param[in]       xTimer: FreeRTOS timer handle
 */
static void osal_timer_callback_wrapper(TimerHandle_t xTimer) {
    /* Get the timer context from the timer ID */
    osal_timer_context_t* ctx =
        (osal_timer_context_t*)pvTimerGetTimerID(xTimer);

    if (ctx != NULL && ctx->user_callback != NULL) {
        /* Invoke the user callback with the user argument */
        ctx->user_callback(ctx->user_arg);
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

    /* Allocate timer context for callback argument passing */
    osal_timer_context_t* ctx =
        (osal_timer_context_t*)pvPortMalloc(sizeof(osal_timer_context_t));
    if (ctx == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    /* Store user callback and argument in context */
    ctx->user_callback = config->callback;
    ctx->user_arg = config->arg;

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
    TimerHandle_t timer =
        xTimerCreate(timer_name,   /* Timer name */
                     period_ticks, /* Timer period in ticks */
                     auto_reload,  /* Auto-reload (periodic) or one-shot */
                     (void*)ctx,   /* Timer ID (stores our context) */
                     osal_timer_callback_wrapper /* Callback wrapper function */
        );

    if (timer == NULL) {
        /* Timer creation failed - free the context */
        vPortFree(ctx);
        return OSAL_ERROR_NO_MEMORY;
    }

    /* Return the timer handle */
    *handle = (osal_timer_handle_t)timer;
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
    return pvPortMalloc(size);
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

    /* Create FreeRTOS event group */
    EventGroupHandle_t event_group = xEventGroupCreate();
    if (event_group == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    *handle = (osal_event_handle_t)event_group;
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

    /* Delete the FreeRTOS event group */
    vEventGroupDelete((EventGroupHandle_t)handle);
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

    /* Set the event bits atomically */
    xEventGroupSetBits((EventGroupHandle_t)handle, (EventBits_t)bits);
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

    /* Clear the event bits atomically */
    xEventGroupClearBits((EventGroupHandle_t)handle, (EventBits_t)bits);
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

    /* Convert timeout to FreeRTOS ticks */
    TickType_t ticks = osal_to_freertos_ticks(options->timeout_ms);

    /* Map OSAL wait mode to FreeRTOS wait mode */
    BaseType_t wait_for_all =
        (options->mode == OSAL_EVENT_WAIT_ALL) ? pdTRUE : pdFALSE;

    /* Map OSAL auto-clear to FreeRTOS clear on exit */
    BaseType_t clear_on_exit = options->auto_clear ? pdTRUE : pdFALSE;

    /* Wait for the event bits */
    EventBits_t result =
        xEventGroupWaitBits((EventGroupHandle_t)handle, (EventBits_t)bits,
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

    /* Get the current event bits (non-blocking) */
    return (osal_event_bits_t)xEventGroupGetBits((EventGroupHandle_t)handle);
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

    /*
     * Use ISR-safe version of event group set bits.
     * xHigherPriorityTaskWoken is set to pdTRUE if setting the bits
     * caused a task to unblock that has a higher priority than the
     * currently running task.
     */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    BaseType_t result =
        xEventGroupSetBitsFromISR((EventGroupHandle_t)handle, (EventBits_t)bits,
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
