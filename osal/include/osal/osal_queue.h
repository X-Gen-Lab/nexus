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
 * \brief           Create a message queue
 * \param[in]       item_size: Size of each item in bytes
 * \param[in]       item_count: Maximum number of items
 * \param[out]      handle: Pointer to store queue handle
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_queue_create(size_t item_size, size_t item_count,
                                osal_queue_handle_t* handle);

/**
 * \brief           Delete a message queue
 * \param[in]       handle: Queue handle
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_queue_delete(osal_queue_handle_t handle);

/**
 * \brief           Send item to queue
 * \param[in]       handle: Queue handle
 * \param[in]       item: Pointer to item to send
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_queue_send(osal_queue_handle_t handle, const void* item,
                              uint32_t timeout_ms);

/**
 * \brief           Send item to front of queue
 * \param[in]       handle: Queue handle
 * \param[in]       item: Pointer to item to send
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_queue_send_front(osal_queue_handle_t handle,
                                    const void* item, uint32_t timeout_ms);

/**
 * \brief           Receive item from queue
 * \param[in]       handle: Queue handle
 * \param[out]      item: Pointer to store received item
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_queue_receive(osal_queue_handle_t handle, void* item,
                                 uint32_t timeout_ms);

/**
 * \brief           Peek item from queue (without removing)
 * \param[in]       handle: Queue handle
 * \param[out]      item: Pointer to store peeked item
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_queue_peek(osal_queue_handle_t handle, void* item);

/**
 * \brief           Get number of items in queue
 * \param[in]       handle: Queue handle
 * \return          Number of items in queue
 */
size_t osal_queue_get_count(osal_queue_handle_t handle);

/**
 * \brief           Check if queue is empty
 * \param[in]       handle: Queue handle
 * \return          true if empty, false otherwise
 */
bool osal_queue_is_empty(osal_queue_handle_t handle);

/**
 * \brief           Check if queue is full
 * \param[in]       handle: Queue handle
 * \return          true if full, false otherwise
 */
bool osal_queue_is_full(osal_queue_handle_t handle);

/**
 * \brief           Send item to queue from ISR
 * \param[in]       handle: Queue handle
 * \param[in]       item: Pointer to item to send
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_queue_send_from_isr(osal_queue_handle_t handle,
                                       const void* item);

/**
 * \brief           Receive item from queue from ISR
 * \param[in]       handle: Queue handle
 * \param[out]      item: Pointer to store received item
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_queue_receive_from_isr(osal_queue_handle_t handle,
                                          void* item);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_QUEUE_H */
