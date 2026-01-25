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
#include "hal/interface/nx_diagnostic.h"
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
    uint8_t cs_pin; /**< CS pin number (device-specific) */
    uint32_t speed; /**< SPI speed in Hz (device-specific) */
    uint8_t mode;   /**< SPI mode (0-3), see nx_spi_mode_t (device-specific) */
    uint8_t bit_order; /**< Bit order: 0=MSB, 1=LSB (device-specific) */
} nx_spi_device_config_t;

/**
 * \brief           SPI statistics structure
 */
typedef struct nx_spi_stats_s {
    bool busy;            /**< Busy flag */
    uint32_t tx_count;    /**< Total bytes transmitted */
    uint32_t rx_count;    /**< Total bytes received */
    uint32_t error_count; /**< Error count */
} nx_spi_stats_t;

/*---------------------------------------------------------------------------*/
/* SPI Bus Interface                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SPI bus interface
 *
 * Provides access to SPI bus through Handle acquisition pattern.
 * Supports multiple devices with different configurations on the same bus.
 */
typedef struct nx_spi_bus_s nx_spi_bus_t;
struct nx_spi_bus_s {
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

    /**
     * \brief           Get diagnostic interface
     * \param[in]       self: SPI bus pointer
     * \return          Diagnostic interface pointer
     */
    nx_diagnostic_t* (*get_diagnostic)(nx_spi_bus_t* self);
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
 * \param[in]       _get_diagnostic: Get diagnostic function pointer
 */
#define NX_INIT_SPI_BUS(p, _get_tx_async_handle, _get_tx_rx_async_handle,      \
                        _get_tx_sync_handle, _get_tx_rx_sync_handle,           \
                        _get_lifecycle, _get_power, _get_diagnostic)           \
    do {                                                                       \
        (p)->get_tx_async_handle = (_get_tx_async_handle);                     \
        (p)->get_tx_rx_async_handle = (_get_tx_rx_async_handle);               \
        (p)->get_tx_sync_handle = (_get_tx_sync_handle);                       \
        (p)->get_tx_rx_sync_handle = (_get_tx_rx_sync_handle);                 \
        (p)->get_lifecycle = (_get_lifecycle);                                 \
        (p)->get_power = (_get_power);                                         \
        (p)->get_diagnostic = (_get_diagnostic);                               \
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
