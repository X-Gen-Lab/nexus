/**
 * \file            queue.h
 * \brief           Portable bounded byte-copy queues without close broadcasts
 * \author          Nexus Team
 */
#ifndef NEXUS_OS_QUEUE_H
#define NEXUS_OS_QUEUE_H
#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Caller-owned control; payload storage is supplied separately. */
typedef struct nx_queue nx_queue_t;
/**
 * \brief           Initialize an exact logical depth and item size
 * \param[in,out]   queue: Unused zeroed control, retained until destroy
 * \param[in]       storage: Caller payload buffer, retained until destroy
 * \param[in]       storage_bytes: Actual capacity, at least depth * item_size
 * \param[in]       depth: Positive number of items
 * \param[in]       item_size: Positive number of bytes per item
 * \return          SUCCESS or admission/initialization error
 * \note            Task/bootstrap only. Reject multiplication/span overflow
 *                  and complete control/payload overlap before kernel effects.
 *                  No allocator or global pool. Partial cleanup retains
 * storage.
 */
nx_result_t nx_queue_init(nx_queue_t* queue, void* storage,
                          size_t storage_bytes, size_t depth, size_t item_size);
/**
 * \brief           Copy an immediately available item before expiry
 * \param[in,out]   queue: Live queue retained throughout the operation
 * \param[in]       item: item_size readable bytes, borrowed until return only
 * \param[in]       deadline: Absolute OS deadline or NX_DEADLINE_NEVER
 * \return          SUCCESS after copy; TIMEOUT or error retains no new item
 * \note            Running task only, including deadline zero (try). Copying a
 *                  pointer does not transfer its payload ownership. No ISR API.
 */
nx_result_t nx_queue_send_until(nx_queue_t* queue, const void* item,
                                nx_time_us_t deadline);
/**
 * \brief           Receive an available item before checking expiry
 * \param[in,out]   queue: Live queue retained throughout the operation
 * \param[out]      item: item_size writable bytes, changed only on SUCCESS
 * \param[in]       deadline: Absolute OS deadline; zero means try
 * \return          SUCCESS, TIMEOUT or an admission/context/kernel error
 * \note            Running task only. Spurious wakes never renew the budget.
 */
nx_result_t nx_queue_receive_until(nx_queue_t* queue, void* item,
                                   nx_time_us_t deadline);
/**
 * \brief           Destroy a drained queue after every user quiesces
 * \param[in,out]   queue: Live control and payload, with no future calls
 * \return          SUCCESS or BUSY/error retaining remaining resources
 * \note            One task/bootstrap lifecycle owner. No close broadcast is
 *                  implied; applications must arrange caller termination.
 */
nx_result_t nx_queue_destroy(nx_queue_t* queue);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_OS_QUEUE_H */
