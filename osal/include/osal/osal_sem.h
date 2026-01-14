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
 */
osal_status_t osal_sem_create(uint32_t initial_count, uint32_t max_count,
                              osal_sem_handle_t* handle);

/**
 * \brief           Create a binary semaphore
 * \param[in]       initial: Initial value (0 or 1)
 * \param[out]      handle: Pointer to store semaphore handle
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_sem_create_binary(uint32_t initial,
                                     osal_sem_handle_t* handle);

/**
 * \brief           Create a counting semaphore
 * \param[in]       max_count: Maximum count value
 * \param[in]       initial: Initial count value
 * \param[out]      handle: Pointer to store semaphore handle
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_sem_create_counting(uint32_t max_count, uint32_t initial,
                                       osal_sem_handle_t* handle);

/**
 * \brief           Delete a semaphore
 * \param[in]       handle: Semaphore handle
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_sem_delete(osal_sem_handle_t handle);

/**
 * \brief           Take (wait for) a semaphore
 * \param[in]       handle: Semaphore handle
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_sem_take(osal_sem_handle_t handle, uint32_t timeout_ms);

/**
 * \brief           Give (signal) a semaphore
 * \param[in]       handle: Semaphore handle
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_sem_give(osal_sem_handle_t handle);

/**
 * \brief           Give (signal) a semaphore from ISR
 * \param[in]       handle: Semaphore handle
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_sem_give_from_isr(osal_sem_handle_t handle);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_SEM_H */
