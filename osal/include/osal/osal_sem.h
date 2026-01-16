/**
 * \file            osal_sem.h
 * \brief           OSAL Semaphore Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef OSAL_SEM_H
#define OSAL_SEM_H

#include "osal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        OSAL_SEM Semaphore
 * \brief           Semaphore interface for synchronization
 * \{
 */

/**
 * \brief           Semaphore handle type
 */
typedef void* osal_sem_handle_t;

/**
 * \brief           Create a semaphore
 * \param[in]       initial_count: Initial count value
 * \param[in]       max_count: Maximum count value
 * \param[out]      handle: Pointer to store semaphore handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Semaphore created successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM initial_count > max_count or
 *                                           max_count is zero
 * \retval          OSAL_ERROR_NO_MEMORY Memory allocation failed
 */
osal_status_t osal_sem_create(uint32_t initial_count, uint32_t max_count,
                              osal_sem_handle_t* handle);

/**
 * \brief           Create a binary semaphore
 * \param[in]       initial: Initial value (0 or 1)
 * \param[out]      handle: Pointer to store semaphore handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Semaphore created successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM initial > 1
 * \retval          OSAL_ERROR_NO_MEMORY Memory allocation failed
 */
osal_status_t osal_sem_create_binary(uint32_t initial,
                                     osal_sem_handle_t* handle);

/**
 * \brief           Create a counting semaphore
 * \param[in]       max_count: Maximum count value
 * \param[in]       initial: Initial count value
 * \param[out]      handle: Pointer to store semaphore handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Semaphore created successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM initial > max_count or
 *                                           max_count is zero
 * \retval          OSAL_ERROR_NO_MEMORY Memory allocation failed
 */
osal_status_t osal_sem_create_counting(uint32_t max_count, uint32_t initial,
                                       osal_sem_handle_t* handle);

/**
 * \brief           Delete a semaphore
 * \param[in]       handle: Semaphore handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Semaphore deleted successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid semaphore handle
 */
osal_status_t osal_sem_delete(osal_sem_handle_t handle);

/**
 * \brief           Take (wait for) a semaphore
 * \param[in]       handle: Semaphore handle
 * \param[in]       timeout_ms: Timeout in milliseconds (OSAL_WAIT_FOREVER
 *                              for infinite wait, OSAL_NO_WAIT for no wait)
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Semaphore taken successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid semaphore handle
 * \retval          OSAL_ERROR_TIMEOUT Take operation timed out
 * \retval          OSAL_ERROR_ISR Called from ISR context with blocking timeout
 */
osal_status_t osal_sem_take(osal_sem_handle_t handle, uint32_t timeout_ms);

/**
 * \brief           Give (signal) a semaphore
 * \param[in]       handle: Semaphore handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Semaphore given successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid semaphore handle
 * \retval          OSAL_ERROR_FULL Semaphore count at maximum
 */
osal_status_t osal_sem_give(osal_sem_handle_t handle);

/**
 * \brief           Give (signal) a semaphore from ISR
 * \param[in]       handle: Semaphore handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Semaphore given successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid semaphore handle
 * \retval          OSAL_ERROR_FULL Semaphore count at maximum
 * \note            This function is safe to call from interrupt context
 */
osal_status_t osal_sem_give_from_isr(osal_sem_handle_t handle);

/**
 * \brief           Get semaphore current count
 * \param[in]       handle: Semaphore handle
 * \return          Current count, or 0 if handle is invalid
 * \note            Requirements: 10.3
 */
uint32_t osal_sem_get_count(osal_sem_handle_t handle);

/**
 * \brief           Reset semaphore to specified count
 * \param[in]       handle: Semaphore handle
 * \param[in]       count: New count value
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Count reset successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM count exceeds max_count or
 *                                           invalid handle
 * \note            Requirements: 10.4
 */
osal_status_t osal_sem_reset(osal_sem_handle_t handle, uint32_t count);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_SEM_H */
