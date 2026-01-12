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
 * \brief           Mutex handle type
 */
typedef void* osal_mutex_handle_t;

/**
 * \brief           Create a mutex
 * \param[out]      handle: Pointer to store mutex handle
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_mutex_create(osal_mutex_handle_t* handle);

/**
 * \brief           Delete a mutex
 * \param[in]       handle: Mutex handle
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_mutex_delete(osal_mutex_handle_t handle);

/**
 * \brief           Lock a mutex
 * \param[in]       handle: Mutex handle
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_mutex_lock(osal_mutex_handle_t handle, uint32_t timeout_ms);

/**
 * \brief           Unlock a mutex
 * \param[in]       handle: Mutex handle
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_mutex_unlock(osal_mutex_handle_t handle);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_MUTEX_H */
