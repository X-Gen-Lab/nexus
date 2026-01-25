/**
 * \file            osal_queue.h
 * \brief           OSAL Message Queue Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef OSAL_QUEUE_H
#define OSAL_QUEUE_H

#include "osal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        OSAL_QUEUE Message Queue
 * \brief           Message queue interface for inter-task communication
 * \{
 */

/**
 * \brief           Queue handle type
 */
typedef void* osal_queue_handle_t;

/**
 * \brief           Queue mode enumeration
 */
typedef enum {
    OSAL_QUEUE_MODE_NORMAL = 0,   /**< Normal mode - block when full */
    OSAL_QUEUE_MODE_OVERWRITE = 1 /**< Overwrite mode - overwrite oldest */
} osal_queue_mode_t;

/**
 * \brief           Create a message queue
 * \param[in]       item_size: Size of each item in bytes
 * \param[in]       item_count: Maximum number of items
 * \param[out]      handle: Pointer to store queue handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Queue created successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM item_size or item_count is zero
 * \retval          OSAL_ERROR_NO_MEMORY Memory allocation failed
 */
osal_status_t osal_queue_create(size_t item_size, size_t item_count,
                                osal_queue_handle_t* handle);

/**
 * \brief           Delete a message queue
 * \param[in]       handle: Queue handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Queue deleted successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid queue handle
 */
osal_status_t osal_queue_delete(osal_queue_handle_t handle);

/**
 * \brief           Send item to queue
 * \param[in]       handle: Queue handle
 * \param[in]       item: Pointer to item to send
 * \param[in]       timeout_ms: Timeout in milliseconds (OSAL_WAIT_FOREVER
 *                              for infinite wait, OSAL_NO_WAIT for no wait)
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Item sent successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle or item is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid queue handle
 * \retval          OSAL_ERROR_TIMEOUT Send operation timed out
 * \retval          OSAL_ERROR_FULL Queue is full (with OSAL_NO_WAIT)
 */
osal_status_t osal_queue_send(osal_queue_handle_t handle, const void* item,
                              uint32_t timeout_ms);

/**
 * \brief           Send item to front of queue
 * \param[in]       handle: Queue handle
 * \param[in]       item: Pointer to item to send
 * \param[in]       timeout_ms: Timeout in milliseconds (OSAL_WAIT_FOREVER
 *                              for infinite wait, OSAL_NO_WAIT for no wait)
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Item sent successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle or item is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid queue handle
 * \retval          OSAL_ERROR_TIMEOUT Send operation timed out
 * \retval          OSAL_ERROR_FULL Queue is full (with OSAL_NO_WAIT)
 */
osal_status_t osal_queue_send_front(osal_queue_handle_t handle,
                                    const void* item, uint32_t timeout_ms);

/**
 * \brief           Receive item from queue
 * \param[in]       handle: Queue handle
 * \param[out]      item: Pointer to store received item
 * \param[in]       timeout_ms: Timeout in milliseconds (OSAL_WAIT_FOREVER
 *                              for infinite wait, OSAL_NO_WAIT for no wait)
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Item received successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle or item is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid queue handle
 * \retval          OSAL_ERROR_TIMEOUT Receive operation timed out
 * \retval          OSAL_ERROR_EMPTY Queue is empty (with OSAL_NO_WAIT)
 */
osal_status_t osal_queue_receive(osal_queue_handle_t handle, void* item,
                                 uint32_t timeout_ms);

/**
 * \brief           Peek item from queue (without removing)
 * \param[in]       handle: Queue handle
 * \param[out]      item: Pointer to store peeked item
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Item peeked successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle or item is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid queue handle
 * \retval          OSAL_ERROR_EMPTY Queue is empty
 */
osal_status_t osal_queue_peek(osal_queue_handle_t handle, void* item);

/**
 * \brief           Get number of items in queue
 * \param[in]       handle: Queue handle
 * \return          Number of items in queue, or 0 if handle is invalid
 */
size_t osal_queue_get_count(osal_queue_handle_t handle);

/**
 * \brief           Check if queue is empty
 * \param[in]       handle: Queue handle
 * \return          true if empty, false otherwise
 * \retval          true Queue is empty or handle is invalid
 * \retval          false Queue contains items
 */
bool osal_queue_is_empty(osal_queue_handle_t handle);

/**
 * \brief           Check if queue is full
 * \param[in]       handle: Queue handle
 * \return          true if full, false otherwise
 * \retval          true Queue is full
 * \retval          false Queue has available space or handle is invalid
 */
bool osal_queue_is_full(osal_queue_handle_t handle);

/**
 * \brief           Send item to queue from ISR
 * \param[in]       handle: Queue handle
 * \param[in]       item: Pointer to item to send
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Item sent successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle or item is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid queue handle
 * \retval          OSAL_ERROR_FULL Queue is full
 * \note            This function is safe to call from interrupt context
 */
osal_status_t osal_queue_send_from_isr(osal_queue_handle_t handle,
                                       const void* item);

/**
 * \brief           Receive item from queue from ISR
 * \param[in]       handle: Queue handle
 * \param[out]      item: Pointer to store received item
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Item received successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle or item is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid queue handle
 * \retval          OSAL_ERROR_EMPTY Queue is empty
 * \note            This function is safe to call from interrupt context
 */
osal_status_t osal_queue_receive_from_isr(osal_queue_handle_t handle,
                                          void* item);

/**
 * \brief           Get available space in queue
 * \param[in]       handle: Queue handle
 * \return          Number of free slots, or 0 if handle is invalid
 * \note            Requirements: 8.1
 */
size_t osal_queue_get_available_space(osal_queue_handle_t handle);

/**
 * \brief           Reset queue (clear all items)
 * \param[in]       handle: Queue handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Queue reset successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid queue handle
 * \note            Requirements: 8.2
 */
osal_status_t osal_queue_reset(osal_queue_handle_t handle);

/**
 * \brief           Set queue mode
 * \param[in]       handle: Queue handle
 * \param[in]       mode: Queue mode (normal or overwrite)
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Mode set successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid queue handle or mode
 * \note            Requirements: 8.3
 */
osal_status_t osal_queue_set_mode(osal_queue_handle_t handle,
                                  osal_queue_mode_t mode);

/**
 * \brief           Peek item from queue in ISR context
 * \param[in]       handle: Queue handle
 * \param[out]      item: Pointer to store peeked item
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Item peeked successfully
 * \retval          OSAL_ERROR_NULL_POINTER handle or item is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM Invalid queue handle
 * \retval          OSAL_ERROR_EMPTY Queue is empty
 * \note            This function is safe to call from interrupt context
 * \note            Requirements: 8.5
 */
osal_status_t osal_queue_peek_from_isr(osal_queue_handle_t handle, void* item);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_QUEUE_H */
