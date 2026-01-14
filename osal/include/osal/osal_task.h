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
 */
osal_status_t osal_task_create(const osal_task_config_t* config,
                               osal_task_handle_t* handle);

/**
 * \brief           Delete a task
 * \param[in]       handle: Task handle (NULL for current task)
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_task_delete(osal_task_handle_t handle);

/**
 * \brief           Suspend a task
 * \param[in]       handle: Task handle
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_task_suspend(osal_task_handle_t handle);

/**
 * \brief           Resume a task
 * \param[in]       handle: Task handle
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_task_resume(osal_task_handle_t handle);

/**
 * \brief           Delay current task
 * \param[in]       ms: Delay time in milliseconds
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_task_delay(uint32_t ms);

/**
 * \brief           Yield current task
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_task_yield(void);

/**
 * \brief           Get current task handle
 * \return          Current task handle
 */
osal_task_handle_t osal_task_get_current(void);

/**
 * \brief           Get task name
 * \param[in]       handle: Task handle
 * \return          Task name string
 */
const char* osal_task_get_name(osal_task_handle_t handle);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_TASK_H */
