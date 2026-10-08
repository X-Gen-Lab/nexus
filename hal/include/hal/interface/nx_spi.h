/**
 * \file            nx_spi.h
 * \brief           SPI bus interface definition
 * \author          Nexus Team
 *
 * This file defines the SPI bus interface with Handle acquisition pattern
 * for device isolation. Supports both async and sync communication modes.
 */

#ifndef NX_SPI_H
#define NX_SPI_H

#include "hal/base/nx_comm.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* SPI Configuration Types                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SPI mode enumeration
 */
typedef enum nx_spi_mode_e {
    NX_SPI_MODE_0 = 0, /**< CPOL=0, CPHA=0 */
    NX_SPI_MODE_1,     /**< CPOL=0, CPHA=1 */
    NX_SPI_MODE_2,     /**< CPOL=1, CPHA=0 */
    NX_SPI_MODE_3,     /**< CPOL=1, CPHA=1 */
} nx_spi_mode_t;

/**
 * \brief           SPI bit order enumeration
 */
typedef enum nx_spi_bit_order_e {
    NX_SPI_BIT_ORDER_MSB = 0, /**< MSB first */
    NX_SPI_BIT_ORDER_LSB,     /**< LSB first */
} nx_spi_bit_order_t;

/**
 * \brief           SPI device configuration structure
 *
 * Device-specific runtime parameters for SPI communication.
 * These parameters are device-specific and must be specified at runtime
 * because the same SPI bus can have multiple devices, each with different
 * requirements (CS pin, speed, mode, bit order).
 *
 * Bus-level configuration (clock frequency, pin mapping) is handled through
 * Kconfig at compile-time.
 *
 * \note            This structure is retained for runtime device parameters
 * \note            Used when acquiring communication handles
 */
typedef struct nx_spi_device_config_s {
    uint8_t cs_pin; /**< Board-defined logical CS identifier (device-specific) */
    uint32_t speed; /**< SPI speed in Hz (device-specific) */
    uint8_t mode;   /**< SPI mode (0-3), see nx_spi_mode_t (device-specific) */
    uint8_t bit_order; /**< Bit order: 0=MSB, 1=LSB (device-specific) */
} nx_spi_device_config_t;

/**
 * Immutable device handles and explicit transactions.
 *
 * A transaction is task-context only. timeout_ms is one total budget starting
 * before lock acquisition (UINT32_MAX = unlimited; 0 = do not start). Buffers
 * remain caller-owned and must be valid for the whole call. On EVERY return,
 * DMA and callbacks have relinquished the buffers. For an admitted transaction, a terminal callback, when
 * supplied, runs once in the calling task after bus unlock and hardware drain.
 * cancel() requests cancellation; it does not release buffer ownership. Only
 * return from transfer() releases ownership. A backend may reject cancellation
 * of a polling transfer with NX_ERR_NOT_SUPPORTED.
 *
 * open_device()/close_device() are task-context operations. The caller owns
 * the output handle value and may copy it. Successful close or bus deinit
 * invalidates all copies; stale copies cannot affect a reused device slot.
 * Generation exhaustion fails closed and generations survive reinitialization.
 * Config is copied
 * into the device slot and cannot be changed by another handle acquisition. Close
 * returns BUSY while that handle is in use; lifecycle changes reject in-flight
 * and waiting transactions, queued work and terminal callbacks. Device pools
 * are bounded by the backend.
 */
typedef void (*nx_spi_terminal_callback_t)(void* user_data, nx_status_t status);
typedef struct nx_spi_transaction_s {
    const uint8_t* tx_data;
    uint8_t* rx_data; /* NULL for TX-only. TX must be present for clock generation. */
    size_t length;
    uint32_t timeout_ms;
    nx_spi_terminal_callback_t callback;
    void* user_data;
} nx_spi_transaction_t;

typedef struct nx_spi_bus_s nx_spi_bus_t;
typedef struct nx_spi_device_s nx_spi_device_t;
struct nx_spi_device_s {
    /* Caller-owned value. Copies are valid until close/deinit; generation
     * tokens never repeat for a bus. Do not edit owner/token manually. */
    nx_spi_bus_t* owner;
    uint64_t token;
    nx_status_t (*transfer)(nx_spi_device_t* self,
                            const nx_spi_transaction_t* transaction);
    /** Bounded async queue. callback required; finite nonzero timeout only.
     * Buffers stay valid until callback. A task must call bus.service(). */
    nx_status_t (*submit)(nx_spi_device_t* self,
                          const nx_spi_transaction_t* transaction);
    nx_status_t (*cancel)(nx_spi_device_t* self);
};

/*---------------------------------------------------------------------------*/
/* SPI Bus Interface                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SPI bus interface
 *
 * Provides access to SPI bus through Handle acquisition pattern.
 * Supports multiple devices with different configurations on the same bus.
 */
struct nx_spi_bus_s {
    /** Optional explicit device API; NULL means backend not implemented. */
    nx_status_t (*open_device)(nx_spi_bus_t* self,
                              const nx_spi_device_config_t* config,
                              nx_spi_device_t* device);
    nx_status_t (*close_device)(nx_spi_bus_t* self, nx_spi_device_t* device);
    /** Process one pending transaction in task context; NO_DATA if idle.
     * A single dedicated worker drives service; it runs callbacks in that task.
     * The queued transaction's original deadline includes queue delay. */
    nx_status_t (*service)(nx_spi_bus_t* self);

    /*-----------------------------------------------------------------------*/
    /* Async Interface Getters                                               */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           Get async TX handle for a specific device configuration
     * \param[in]       self: SPI bus pointer
     * \param[in]       config: Device configuration
     * \return          Async TX interface pointer, NULL on error
     */
    nx_tx_async_t* (*get_tx_async_handle)(nx_spi_bus_t* self,
                                          nx_spi_device_config_t config);

    /**
     * \brief           Get async TX/RX handle for a specific device
     *                  configuration
     * \param[in]       self: SPI bus pointer
     * \param[in]       config: Device configuration
     * \param[in]       callback: Callback for received data
     * \param[in]       user_data: User data for callback
     * \return          Async TX/RX interface pointer, NULL on error
     */
    nx_tx_rx_async_t* (*get_tx_rx_async_handle)(nx_spi_bus_t* self,
                                                nx_spi_device_config_t config,
                                                nx_comm_callback_t callback,
                                                void* user_data);

    /*-----------------------------------------------------------------------*/
    /* Sync Interface Getters                                                */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           Get sync TX handle for a specific device configuration
     * \param[in]       self: SPI bus pointer
     * \param[in]       config: Device configuration
     * \return          Sync TX interface pointer, NULL on error
     */
    nx_tx_sync_t* (*get_tx_sync_handle)(nx_spi_bus_t* self,
                                        nx_spi_device_config_t config);

    /**
     * \brief           Get sync TX/RX handle for a specific device
     *                  configuration
     * \param[in]       self: SPI bus pointer
     * \param[in]       config: Device configuration
     * \return          Sync TX/RX interface pointer, NULL on error
     */
    nx_tx_rx_sync_t* (*get_tx_rx_sync_handle)(nx_spi_bus_t* self,
                                              nx_spi_device_config_t config);

    /*-----------------------------------------------------------------------*/
    /* Base Interface Getters                                                */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: SPI bus pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_spi_bus_t* self);

    /**
     * \brief           Get power interface
     * \param[in]       self: SPI bus pointer
     * \return          Power interface pointer
     */
    nx_power_t* (*get_power)(nx_spi_bus_t* self);
};
/*---------------------------------------------------------------------------*/
/* SPI Bus Initialization Macro                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize SPI bus interface
 * \param[in]       p: Pointer to nx_spi_bus_t structure
 * \param[in]       _get_tx_async_handle: Get TX async handle function pointer
 * \param[in]       _get_tx_rx_async_handle: Get TX/RX async handle function
 * \param[in]       _get_tx_sync_handle: Get TX sync handle function pointer
 * \param[in]       _get_tx_rx_sync_handle: Get TX/RX sync handle function
 * \param[in]       _get_lifecycle: Get lifecycle function pointer
 * \param[in]       _get_power: Get power function pointer
 */
#define NX_INIT_SPI_BUS(p, _get_tx_async_handle, _get_tx_rx_async_handle,      \
                        _get_tx_sync_handle, _get_tx_rx_sync_handle,           \
                        _get_lifecycle, _get_power)                            \
    do {                                                                       \
        (p)->open_device = NULL;                                               \
        (p)->service = NULL;                                                   \
        (p)->close_device = NULL;                                              \
        (p)->get_tx_async_handle = (_get_tx_async_handle);                     \
        (p)->get_tx_rx_async_handle = (_get_tx_rx_async_handle);               \
        (p)->get_tx_sync_handle = (_get_tx_sync_handle);                       \
        (p)->get_tx_rx_sync_handle = (_get_tx_rx_sync_handle);                 \
        (p)->get_lifecycle = (_get_lifecycle);                                 \
        (p)->get_power = (_get_power);                                         \
        NX_ASSERT((p)->get_tx_async_handle != NULL);                           \
        NX_ASSERT((p)->get_tx_rx_async_handle != NULL);                        \
        NX_ASSERT((p)->get_tx_sync_handle != NULL);                            \
        NX_ASSERT((p)->get_tx_rx_sync_handle != NULL);                         \
        NX_ASSERT((p)->get_lifecycle != NULL);                                 \
    } while (0)

/**
 * \brief           Create default SPI device configuration
 * \param[in]       _cs_pin: CS pin number
 * \param[in]       _speed: SPI speed in Hz
 * \return          nx_spi_device_config_t structure
 */
#define NX_SPI_DEVICE_CONFIG_DEFAULT(_cs_pin, _speed)                          \
    (nx_spi_device_config_t) {                                                 \
        .cs_pin = (_cs_pin), .speed = (_speed), .mode = NX_SPI_MODE_0,         \
        .bit_order = NX_SPI_BIT_ORDER_MSB,                                     \
    }

/*---------------------------------------------------------------------------*/
/* Type Aliases for Backward Compatibility                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SPI interface type alias (for backward compatibility)
 * \note            New code should use nx_spi_bus_t with Handle acquisition
 */
typedef nx_spi_bus_t nx_spi_t;

#ifdef __cplusplus
}
#endif

#endif /* NX_SPI_H */
