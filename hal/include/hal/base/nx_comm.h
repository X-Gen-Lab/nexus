/**
 * \file            nx_comm.h
 * \brief           Base communication interfaces for async/sync operations
 * \author          Nexus Team
 *
 * This file defines the base communication interfaces that separate
 * async (non-blocking) and sync (blocking) operations for HAL peripherals.
 */

#ifndef NX_COMM_H
#define NX_COMM_H

#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Type definitions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Communication callback function type
 * \param[in]       user_data: User-provided context pointer
 * \param[in]       data: Received data buffer
 * \param[in]       len: Received data length
 */
typedef void (*nx_comm_callback_t)(void* user_data, const uint8_t* data,
                                   size_t len);

/*---------------------------------------------------------------------------*/
/* Async Transmit Interface                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Async transmit interface
 *
 * Provides non-blocking transmit operations. The send() method returns
 * immediately without waiting for transmission to complete.
 */
typedef struct nx_tx_async_s nx_tx_async_t;
struct nx_tx_async_s {
    /**
     * \brief           Async send data (non-blocking)
     * \param[in]       self: Interface pointer
     * \param[in]       data: Transmit data buffer
     * \param[in]       len: Transmit data length
     * \return          NX_OK on success, NX_ERR_BUSY if device busy,
     *              NX_ERR_FULL if buffer full
     */
    nx_status_t (*send)(nx_tx_async_t* self, const uint8_t* data, size_t len);

    /**
     * \brief           Get transmit state
     * \param[in]       self: Interface pointer
     * \return          NX_OK if idle, NX_ERR_BUSY if transmitting
     */
    nx_status_t (*get_state)(nx_tx_async_t* self);
};

/*---------------------------------------------------------------------------*/
/* Async Receive Interface                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Async receive interface
 *
 * Provides non-blocking receive operations. The receive() method returns
 * immediately with available data or NX_ERR_NO_DATA if buffer is empty.
 */
typedef struct nx_rx_async_s nx_rx_async_t;
struct nx_rx_async_s {
    /**
     * \brief           Async receive data (non-blocking)
     * \param[in]       self: Interface pointer
     * \param[out]      data: Receive data buffer
     * \param[in,out]   len: Input is buffer size, output is actual received
     * length
     * \return          NX_OK on success, NX_ERR_NO_DATA if no data available
     */
    nx_status_t (*receive)(nx_rx_async_t* self, uint8_t* data, size_t* len);
};

/*---------------------------------------------------------------------------*/
/* Sync Transmit Interface                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Sync transmit interface
 *
 * Provides blocking transmit operations. The send() method blocks until
 * transmission completes or timeout expires.
 */
typedef struct nx_tx_sync_s nx_tx_sync_t;
struct nx_tx_sync_s {
    /**
     * \brief           Sync send data (blocking)
     * \param[in]       self: Interface pointer
     * \param[in]       data: Transmit data buffer
     * \param[in]       len: Transmit data length
     * \param[in]       timeout_ms: Timeout in milliseconds
     * \return          NX_OK on success, NX_ERR_TIMEOUT on timeout
     */
    nx_status_t (*send)(nx_tx_sync_t* self, const uint8_t* data, size_t len,
                        uint32_t timeout_ms);
};

/*---------------------------------------------------------------------------*/
/* Sync Receive Interface                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Sync receive interface
 *
 * Provides blocking receive operations. The receive() method blocks until
 * data is available or timeout expires.
 */
typedef struct nx_rx_sync_s nx_rx_sync_t;
struct nx_rx_sync_s {
    /**
     * \brief           Sync receive data (blocking)
     * \param[in]       self: Interface pointer
     * \param[out]      data: Receive data buffer
     * \param[in,out]   len: Input is buffer size, output is actual received
     * length
     * \param[in]       timeout_ms: Timeout in milliseconds
     * \return          NX_OK on success, NX_ERR_TIMEOUT on timeout
     */
    nx_status_t (*receive)(nx_rx_sync_t* self, uint8_t* data, size_t* len,
                           uint32_t timeout_ms);

    /**
     * \brief           Sync receive specified length data (blocking)
     * \param[in]       self: Interface pointer
     * \param[out]      data: Receive data buffer
     * \param[in,out]   len: Input is expected length, output is actual received
     * length
     * \param[in]       timeout_ms: Timeout in milliseconds
     * \return          NX_OK on success, NX_ERR_TIMEOUT on timeout
     */
    nx_status_t (*receive_all)(nx_rx_sync_t* self, uint8_t* data, size_t* len,
                               uint32_t timeout_ms);
};

/*---------------------------------------------------------------------------*/
/* Async Transceive Interface                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Async transceive interface
 *
 * Provides non-blocking simultaneous transmit and receive operations,
 * typically used for SPI/I2C scenarios. Received data is returned via callback.
 */
typedef struct nx_tx_rx_async_s nx_tx_rx_async_t;
struct nx_tx_rx_async_s {
    /**
     * \brief           Async transceive data (non-blocking)
     * \param[in]       self: Interface pointer
     * \param[in]       tx_data: Transmit data buffer
     * \param[in]       tx_len: Transmit data length
     * \param[in]       timeout_ms: Timeout in milliseconds
     * \return          NX_OK on success, NX_ERR_BUSY if device busy
     * \note            Received data is returned via registered callback
     */
    nx_status_t (*tx_rx)(nx_tx_rx_async_t* self, const uint8_t* tx_data,
                         size_t tx_len, uint32_t timeout_ms);

    /**
     * \brief           Get transceive state
     * \param[in]       self: Interface pointer
     * \return          NX_OK if idle, NX_ERR_BUSY if transceiving
     */
    nx_status_t (*get_state)(nx_tx_rx_async_t* self);
};

/*---------------------------------------------------------------------------*/
/* Sync Transceive Interface                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Sync transceive interface
 *
 * Provides blocking simultaneous transmit and receive operations.
 */
typedef struct nx_tx_rx_sync_s nx_tx_rx_sync_t;
struct nx_tx_rx_sync_s {
    /**
     * \brief           Sync transceive data (blocking)
     * \param[in]       self: Interface pointer
     * \param[in]       tx_data: Transmit data buffer
     * \param[in]       tx_len: Transmit data length
     * \param[out]      rx_data: Receive data buffer
     * \param[in,out]   rx_len: Input is buffer size, output is actual received
     * length
     * \param[in]       timeout_ms: Timeout in milliseconds
     * \return          NX_OK on success, NX_ERR_TIMEOUT on timeout
     */
    nx_status_t (*tx_rx)(nx_tx_rx_sync_t* self, const uint8_t* tx_data,
                         size_t tx_len, uint8_t* rx_data, size_t* rx_len,
                         uint32_t timeout_ms);
};

/*---------------------------------------------------------------------------*/
/* Interface Initialization Macros                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize async transmit interface
 * \param[in]       p: Pointer to nx_tx_async_t structure
 * \param[in]       _send: Send function pointer
 * \param[in]       _get_state: Get state function pointer
 */
#define NX_INIT_TX_ASYNC(p, _send, _get_state)                                 \
    do {                                                                       \
        (p)->send = (_send);                                                   \
        (p)->get_state = (_get_state);                                         \
        NX_ASSERT((p)->send != NULL);                                          \
        NX_ASSERT((p)->get_state != NULL);                                     \
    } while (0)

/**
 * \brief           Initialize async receive interface
 * \param[in]       p: Pointer to nx_rx_async_t structure
 * \param[in]       _receive: Receive function pointer
 */
#define NX_INIT_RX_ASYNC(p, _receive)                                          \
    do {                                                                       \
        (p)->receive = (_receive);                                             \
        NX_ASSERT((p)->receive != NULL);                                       \
    } while (0)

/**
 * \brief           Initialize sync transmit interface
 * \param[in]       p: Pointer to nx_tx_sync_t structure
 * \param[in]       _send: Send function pointer
 */
#define NX_INIT_TX_SYNC(p, _send)                                              \
    do {                                                                       \
        (p)->send = (_send);                                                   \
        NX_ASSERT((p)->send != NULL);                                          \
    } while (0)

/**
 * \brief           Initialize sync receive interface
 * \param[in]       p: Pointer to nx_rx_sync_t structure
 * \param[in]       _receive: Receive function pointer
 * \param[in]       _receive_all: Receive all function pointer
 */
#define NX_INIT_RX_SYNC(p, _receive, _receive_all)                             \
    do {                                                                       \
        (p)->receive = (_receive);                                             \
        (p)->receive_all = (_receive_all);                                     \
        NX_ASSERT((p)->receive != NULL);                                       \
        NX_ASSERT((p)->receive_all != NULL);                                   \
    } while (0)

/**
 * \brief           Initialize async transceive interface
 * \param[in]       p: Pointer to nx_tx_rx_async_t structure
 * \param[in]       _tx_rx: Transceive function pointer
 * \param[in]       _get_state: Get state function pointer
 */
#define NX_INIT_TX_RX_ASYNC(p, _tx_rx, _get_state)                             \
    do {                                                                       \
        (p)->tx_rx = (_tx_rx);                                                 \
        (p)->get_state = (_get_state);                                         \
        NX_ASSERT((p)->tx_rx != NULL);                                         \
        NX_ASSERT((p)->get_state != NULL);                                     \
    } while (0)

/**
 * \brief           Initialize sync transceive interface
 * \param[in]       p: Pointer to nx_tx_rx_sync_t structure
 * \param[in]       _tx_rx: Transceive function pointer
 */
#define NX_INIT_TX_RX_SYNC(p, _tx_rx)                                          \
    do {                                                                       \
        (p)->tx_rx = (_tx_rx);                                                 \
        NX_ASSERT((p)->tx_rx != NULL);                                         \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* NX_COMM_H */
