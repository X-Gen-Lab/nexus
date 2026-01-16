/**
 * \file            osal_mutex.h
 * \brief           OSAL Mutex Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef OSAL_MUTEX_H
#define OSAL_MUTEX_H

#include "osal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        OSAL_MUTEX Mutex
 * \brief           Mutex interface for synchronization
 * \{
 */

/**
 * \brief           Forward declaration of task handle type
 */
typedef void* osal_task_handle_t;

/**
 * \brief           Mutex handle type
 */
typedef void* osal_mutex_handle_t;

/**
 * \brief           Create a mutex
 * \param[out]      handle: Pointer to store mutex handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Mutex created successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_NO_MEMORY Memory allocation failed
 */
osal_status_t osal_mutex_create(osal_mutex_handle_t* handle);

/**
 * \brief           Delete a mutex
 * \param[in]       handle: Mutex handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Mutex deleted successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid mutex handle
 * \retval          OSAL_ERROR_BUSY Mutex is currently locked
 */
osal_status_t osal_mutex_delete(osal_mutex_handle_t handle);

/**
 * \brief           Lock a mutex
 * \param[in]       handle: Mutex handle
 * \param[in]       timeout_ms: Timeout in milliseconds (OSAL_WAIT_FOREVER
 *                              for infinite wait, OSAL_NO_WAIT for no wait)
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Mutex locked successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid mutex handle
 * \retval          OSAL_ERROR_TIMEOUT Lock operation timed out
 * \retval          OSAL_ERROR_ISR Called from ISR context
 */
osal_status_t osal_mutex_lock(osal_mutex_handle_t handle, uint32_t timeout_ms);

/**
 * \brief           Unlock a mutex
 * \param[in]       handle: Mutex handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Mutex unlocked successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid mutex handle
 * \retval          OSAL_ERROR Mutex not owned by current task
 */
osal_status_t osal_mutex_unlock(osal_mutex_handle_t handle);

/**
 * \brief           Get mutex owner task
 * \param[in]       handle: Mutex handle
 * \return          Owner task handle, or NULL if not locked or invalid handle
 * \retval          NULL Mutex is not locked or handle is invalid
 * \retval          non-NULL Task handle of the mutex owner
 * \note            Requirements: 10.1
 */
osal_task_handle_t osal_mutex_get_owner(osal_mutex_handle_t handle);

/**
 * \brief           Check if mutex is locked
 * \param[in]       handle: Mutex handle
 * \return          true if locked, false otherwise
 * \retval          true Mutex is currently locked
 * \retval          false Mutex is not locked or handle is invalid
 * \note            Requirements: 10.2
 */
bool osal_mutex_is_locked(osal_mutex_handle_t handle);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_MUTEX_H */
