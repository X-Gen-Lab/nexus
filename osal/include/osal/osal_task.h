/**
 * \file            osal_task.h
 * \brief           OSAL Task Management Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef OSAL_TASK_H
#define OSAL_TASK_H

#include "osal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        OSAL_TASK Task Management
 * \brief           Task management interface
 * \{
 */

/**
 * \brief           Task handle type
 */
typedef void* osal_task_handle_t;

/**
 * \brief           Task function type
 * \param[in]       arg: Task argument
 */
typedef void (*osal_task_func_t)(void* arg);

/**
 * \brief           Task priority levels
 * \note            Priority range is 0-31, where 0 is lowest and 31 is highest
 */
typedef enum {
    OSAL_TASK_PRIORITY_IDLE = 0,     /**< Idle priority (lowest) */
    OSAL_TASK_PRIORITY_LOW = 8,      /**< Low priority */
    OSAL_TASK_PRIORITY_NORMAL = 16,  /**< Normal priority */
    OSAL_TASK_PRIORITY_HIGH = 24,    /**< High priority */
    OSAL_TASK_PRIORITY_REALTIME = 31 /**< Real-time priority (highest) */
} osal_task_priority_t;

/**
 * \brief           Task state enumeration
 */
typedef enum {
    OSAL_TASK_STATE_READY = 0,       /**< Task is ready to run */
    OSAL_TASK_STATE_RUNNING = 1,     /**< Task is currently running */
    OSAL_TASK_STATE_BLOCKED = 2,     /**< Task is blocked */
    OSAL_TASK_STATE_SUSPENDED = 3,   /**< Task is suspended */
    OSAL_TASK_STATE_DELETED = 4      /**< Task has been deleted */
} osal_task_state_t;

/**
 * \brief           Task configuration structure
 */
typedef struct {
    const char* name;      /**< Task name */
    osal_task_func_t func; /**< Task function */
    void* arg;             /**< Task argument */
    uint8_t priority;      /**< Task priority (0-31) */
    size_t stack_size;     /**< Stack size in bytes */
} osal_task_config_t;

/**
 * \brief           Create a new task
 * \param[in]       config: Task configuration
 * \param[out]      handle: Pointer to store task handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Task created successfully
 * \retval          OSAL_ERROR_NULL_POINTER config or handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid configuration parameters
 * \retval          OSAL_ERROR_NO_MEMORY Memory allocation failed
 */
osal_status_t osal_task_create(const osal_task_config_t* config,
                               osal_task_handle_t* handle);

/**
 * \brief           Delete a task
 * \param[in]       handle: Task handle (NULL for current task)
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Task deleted successfully
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid task handle
 */
osal_status_t osal_task_delete(osal_task_handle_t handle);

/**
 * \brief           Suspend a task
 * \param[in]       handle: Task handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Task suspended successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid task handle
 */
osal_status_t osal_task_suspend(osal_task_handle_t handle);

/**
 * \brief           Resume a task
 * \param[in]       handle: Task handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Task resumed successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid task handle
 */
osal_status_t osal_task_resume(osal_task_handle_t handle);

/**
 * \brief           Delay current task
 * \param[in]       ms: Delay time in milliseconds
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Delay completed successfully
 * \retval          OSAL_ERROR_ISR Called from ISR context
 */
osal_status_t osal_task_delay(uint32_t ms);

/**
 * \brief           Yield current task
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Yield completed successfully
 * \retval          OSAL_ERROR_ISR Called from ISR context
 */
osal_status_t osal_task_yield(void);

/**
 * \brief           Get current task handle
 * \return          Current task handle, or NULL if scheduler not running
 */
osal_task_handle_t osal_task_get_current(void);

/**
 * \brief           Get task name
 * \param[in]       handle: Task handle
 * \return          Task name string, or NULL if handle is invalid
 */
const char* osal_task_get_name(osal_task_handle_t handle);

/**
 * \brief           Get task priority
 * \param[in]       handle: Task handle
 * \return          Task priority (0-31), or 0 if handle is invalid
 * \note            Returns 0 for invalid handles, which is also a valid
 *                  priority value (OSAL_TASK_PRIORITY_IDLE)
 */
uint8_t osal_task_get_priority(osal_task_handle_t handle);

/**
 * \brief           Set task priority
 * \param[in]       handle: Task handle
 * \param[in]       priority: New priority (0-31)
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Priority set successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid priority value or handle
 */
osal_status_t osal_task_set_priority(osal_task_handle_t handle,
                                     uint8_t priority);

/**
 * \brief           Get task stack high watermark
 * \param[in]       handle: Task handle
 * \return          Minimum free stack space in bytes, or 0 if invalid
 * \note            Returns the minimum amount of remaining stack space
 *                  that has existed since the task was created
 */
size_t osal_task_get_stack_watermark(osal_task_handle_t handle);

/**
 * \brief           Get task state
 * \param[in]       handle: Task handle
 * \return          Task state enumeration value
 * \retval          OSAL_TASK_STATE_READY Task is ready to run
 * \retval          OSAL_TASK_STATE_RUNNING Task is currently running
 * \retval          OSAL_TASK_STATE_BLOCKED Task is blocked
 * \retval          OSAL_TASK_STATE_SUSPENDED Task is suspended
 * \retval          OSAL_TASK_STATE_DELETED Task has been deleted or invalid
 */
osal_task_state_t osal_task_get_state(osal_task_handle_t handle);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_TASK_H */
