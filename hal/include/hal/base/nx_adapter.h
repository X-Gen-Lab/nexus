/**
 * \file            nx_adapter.h
 * \brief           Interface conversion adapters for async/sync operations
 * \author          Nexus Team
 *
 * This file provides adapter functions to convert between async and sync
 * communication interfaces, enabling flexible usage patterns.
 */

#ifndef NX_ADAPTER_H
#define NX_ADAPTER_H

#include "hal/base/nx_comm.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Async to Sync Adapters                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Convert async TX interface to sync TX interface
 * \param[in]       tx_async: Async TX interface pointer
 * \return          Sync TX interface pointer, or NULL on failure
 * \note            Internally uses os_yield polling until completion or timeout
 */
nx_tx_sync_t* nx_tx_async_to_sync(nx_tx_async_t* tx_async);

/**
 * \brief           Convert async RX interface to sync RX interface
 * \param[in]       rx_async: Async RX interface pointer
 * \return          Sync RX interface pointer, or NULL on failure
 * \note            Internally uses os_yield polling until data available or
 *                  timeout
 */
nx_rx_sync_t* nx_rx_async_to_sync(nx_rx_async_t* rx_async);

/*---------------------------------------------------------------------------*/
/* Sync to Async Adapters                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Convert sync TX interface to async TX interface
 * \param[in]       tx_sync: Sync TX interface pointer
 * \param[in]       timeout_ms: Default timeout value for sync operations
 * \return          Async TX interface pointer, or NULL on failure
 * \note            Uses configured timeout and maps timeout to NX_ERR_BUSY
 */
nx_tx_async_t* nx_tx_sync_to_async(nx_tx_sync_t* tx_sync, uint32_t timeout_ms);

/**
 * \brief           Convert sync RX interface to async RX interface
 * \param[in]       rx_sync: Sync RX interface pointer
 * \param[in]       timeout_ms: Default timeout value for sync operations
 * \return          Async RX interface pointer, or NULL on failure
 * \note            Uses configured timeout and maps timeout to NX_ERR_NO_DATA
 */
nx_rx_async_t* nx_rx_sync_to_async(nx_rx_sync_t* rx_sync, uint32_t timeout_ms);

/*---------------------------------------------------------------------------*/
/* Adapter Release Functions                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Release async-to-sync TX adapter
 * \param[in]       adapter: Sync TX interface pointer returned by
 *                  nx_tx_async_to_sync
 */
void nx_tx_async_to_sync_release(nx_tx_sync_t* adapter);

/**
 * \brief           Release async-to-sync RX adapter
 * \param[in]       adapter: Sync RX interface pointer returned by
 *                  nx_rx_async_to_sync
 */
void nx_rx_async_to_sync_release(nx_rx_sync_t* adapter);

/**
 * \brief           Release sync-to-async TX adapter
 * \param[in]       adapter: Async TX interface pointer returned by
 *                  nx_tx_sync_to_async
 */
void nx_tx_sync_to_async_release(nx_tx_async_t* adapter);

/**
 * \brief           Release sync-to-async RX adapter
 * \param[in]       adapter: Async RX interface pointer returned by
 *                  nx_rx_sync_to_async
 */
void nx_rx_sync_to_async_release(nx_rx_async_t* adapter);

#ifdef __cplusplus
}
#endif

#endif /* NX_ADAPTER_H */
