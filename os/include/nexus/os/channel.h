/**
 * \file            channel.h
 * \brief           Portable close-aware channels with caller-owned waiters
 * \author          Nexus Team
 */
#ifndef NEXUS_OS_CHANNEL_H
#define NEXUS_OS_CHANNEL_H
#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Static close-aware byte queue; ordinary queues remain separate. */
typedef struct nx_channel nx_channel_t;
/** \brief One exclusive caller-owned latch per simultaneous channel call. */
typedef struct nx_channel_waiter nx_channel_waiter_t;
/**
 * \brief           Initialize an unused operation waiter
 * \param[in,out]   waiter: Zeroed storage retained until waiter destroy
 * \return          SUCCESS or admission/initialization error
 * \note            Task/bootstrap only; never share one active waiter.
 */
nx_result_t nx_channel_waiter_init(nx_channel_waiter_t* waiter);
/**
 * \brief           Release an inactive waiter after its operation has returned
 * \param[in,out]   waiter: Live exclusive storage with no future operation
 * \return          SUCCESS or BUSY/error retaining resources
 * \note            One task lifecycle owner; failure never permits reuse.
 */
nx_result_t nx_channel_waiter_destroy(nx_channel_waiter_t* waiter);
/**
 * \brief           Initialize a channel with explicit broadcast capacity
 * \param[in,out]   channel: Unused zeroed storage retained until destroy
 * \param[in]       storage: Payload buffer retained until destroy
 * \param[in]       storage_bytes: Actual capacity, at least depth * item_size
 * \param[in]       depth: Positive number of copied items
 * \param[in]       item_size: Positive bytes per item
 * \param[in]       maximum_waiters: Positive maximum concurrent operations
 * \return          SUCCESS or admission/initialization error
 * \note            Task/bootstrap only. Complete control/payload spans must
 *                  not overlap or wrap. No hidden waiter pool or worker.
 */
nx_result_t nx_channel_init(nx_channel_t* channel, void* storage,
                            size_t storage_bytes, size_t depth,
                            size_t item_size, size_t maximum_waiters);
/**
 * \brief           Close send admission and wake registered blockers
 * \param[in,out]   channel: Live channel retained through all users
 * \return          SUCCESS (idempotent) or admission/context error
 * \note            Task lifecycle owner. Existing items remain receivable.
 *                  Close does not join users, destroy storage or cancel I/O.
 */
nx_result_t nx_channel_close(nx_channel_t* channel);
/**
 * \brief           Copy an item or observe close without periodic polling
 * \param[in,out]   channel: Live channel retained throughout the call
 * \param[in]       item: item_size readable bytes, borrowed until return
 * \param[in]       deadline: Absolute OS deadline or NX_DEADLINE_NEVER
 * \param[in,out]   waiter: Live exclusive waiter retained through this call
 * \return          SUCCESS, CANCELLED after close, TIMEOUT, BUSY or error
 * \note            Running task only. Closed sends never retain the item.
 */
nx_result_t nx_channel_send_until(nx_channel_t* channel, const void* item,
                                  nx_time_us_t deadline,
                                  nx_channel_waiter_t* waiter);
/**
 * \brief           Receive and drain data queued before close
 * \param[in,out]   channel: Live channel retained throughout the call
 * \param[out]      item: item_size writable bytes, changed only on SUCCESS
 * \param[in]       deadline: Absolute OS deadline; zero means try
 * \param[in,out]   waiter: Live exclusive waiter retained through this call
 * \return          SUCCESS; CANCELLED when closed and empty; TIMEOUT/BUSY/error
 * \note            Running task only. Immediate readiness wins expiration.
 */
nx_result_t nx_channel_receive_until(nx_channel_t* channel, void* item,
                                     nx_time_us_t deadline,
                                     nx_channel_waiter_t* waiter);
/**
 * \brief           Destroy a closed, drained and externally quiesced channel
 * \param[in,out]   channel: Live control and payload with no future users
 * \return          SUCCESS or BUSY/error retaining every outstanding resource
 * \note            Task lifecycle owner. Join publishers and waiters before
 *                  destroy; completion or a users count alone is insufficient.
 */
nx_result_t nx_channel_destroy(nx_channel_t* channel);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_OS_CHANNEL_H */
