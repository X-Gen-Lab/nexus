/**
 * \file            nx_can.h
 * \brief           CAN bus interface definition
 * \author          Nexus Team
 *
 * This file defines the CAN bus interface with Handle acquisition pattern
 * for device isolation. Supports multiple CAN IDs on the same bus.
 */

#ifndef NX_CAN_H
#define NX_CAN_H

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
/* CAN Configuration Types                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           CAN frame type enumeration
 */
typedef enum nx_can_frame_type_e {
    NX_CAN_FRAME_STANDARD = 0, /**< Standard frame (11-bit ID) */
    NX_CAN_FRAME_EXTENDED,     /**< Extended frame (29-bit ID) */
} nx_can_frame_type_t;

/**
 * \brief           CAN bus mode enumeration
 */
typedef enum nx_can_mode_e {
    NX_CAN_MODE_NORMAL = 0,      /**< Normal operation mode */
    NX_CAN_MODE_LOOPBACK,        /**< Loopback mode for testing */
    NX_CAN_MODE_SILENT,          /**< Silent mode (receive only) */
    NX_CAN_MODE_SILENT_LOOPBACK, /**< Silent loopback mode */
} nx_can_mode_t;

/**
 * \brief           CAN statistics structure
 */
typedef struct nx_can_stats_s {
    uint32_t tx_count;       /**< Total frames transmitted */
    uint32_t rx_count;       /**< Total frames received */
    uint32_t error_count;    /**< Total error count */
    uint16_t tx_error_count; /**< TX error counter */
    uint16_t rx_error_count; /**< RX error counter */
    bool bus_off;            /**< Bus-off state flag */
} nx_can_stats_t;

/*---------------------------------------------------------------------------*/
/* CAN Bus Interface                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           CAN bus interface
 *
 * Provides access to CAN bus through Handle acquisition pattern.
 * Supports multiple CAN IDs with message filtering.
 */
typedef struct nx_can_bus_s nx_can_bus_t;
struct nx_can_bus_s {
    /*-----------------------------------------------------------------------*/
    /* Handle Acquisition Methods                                            */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           Get TX handle for a specific CAN ID
     * \param[in]       self: CAN bus pointer
     * \param[in]       can_id: CAN message ID
     * \return          Async TX interface pointer, NULL on error
     */
    nx_tx_async_t* (*get_tx_handle)(nx_can_bus_t* self, uint16_t can_id);

    /**
     * \brief           Get RX handle for a specific CAN ID
     * \param[in]       self: CAN bus pointer
     * \param[in]       can_id: CAN message ID to receive
     * \param[in]       buffer_size: Receive buffer size in frames
     * \return          Async RX interface pointer, NULL on error
     */
    nx_rx_async_t* (*get_rx_handle)(nx_can_bus_t* self, uint16_t can_id,
                                    size_t buffer_size);

    /*-----------------------------------------------------------------------*/
    /* Error and Filter Methods                                              */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           Get TX and RX error counters
     * \param[in]       self: CAN bus pointer
     * \param[out]      tx_count: TX error counter output
     * \param[out]      rx_count: RX error counter output
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*get_error_count)(nx_can_bus_t* self, uint16_t* tx_count,
                                   uint16_t* rx_count);

    /**
     * \brief           Set message acceptance filter
     * \param[in]       self: CAN bus pointer
     * \param[in]       filter_id: Filter ID value
     * \param[in]       filter_mask: Filter mask (1=must match, 0=don't care)
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*set_filter)(nx_can_bus_t* self, uint16_t filter_id,
                              uint16_t filter_mask);

    /*-----------------------------------------------------------------------*/
    /* Base Interface Getters                                                */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: CAN bus pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_can_bus_t* self);

    /**
     * \brief           Get power interface
     * \param[in]       self: CAN bus pointer
     * \return          Power interface pointer
     */
    nx_power_t* (*get_power)(nx_can_bus_t* self);

    /**
     * \brief           Get diagnostic interface
     * \param[in]       self: CAN bus pointer
     * \return          Diagnostic interface pointer
     */
    nx_diagnostic_t* (*get_diagnostic)(nx_can_bus_t* self);
};

/*---------------------------------------------------------------------------*/
/* CAN Bus Initialization Macro                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize CAN bus interface
 * \param[in]       p: Pointer to nx_can_bus_t structure
 * \param[in]       _get_tx_handle: Get TX handle function pointer
 * \param[in]       _get_rx_handle: Get RX handle function pointer
 * \param[in]       _get_error_count: Get error count function pointer
 * \param[in]       _set_filter: Set filter function pointer
 * \param[in]       _get_lifecycle: Get lifecycle function pointer
 * \param[in]       _get_power: Get power function pointer
 * \param[in]       _get_diagnostic: Get diagnostic function pointer
 */
#define NX_INIT_CAN_BUS(p, _get_tx_handle, _get_rx_handle, _get_error_count,   \
                        _set_filter, _get_lifecycle, _get_power,               \
                        _get_diagnostic)                                       \
    do {                                                                       \
        (p)->get_tx_handle = (_get_tx_handle);                                 \
        (p)->get_rx_handle = (_get_rx_handle);                                 \
        (p)->get_error_count = (_get_error_count);                             \
        (p)->set_filter = (_set_filter);                                       \
        (p)->get_lifecycle = (_get_lifecycle);                                 \
        (p)->get_power = (_get_power);                                         \
        (p)->get_diagnostic = (_get_diagnostic);                               \
        NX_ASSERT((p)->get_tx_handle != NULL);                                 \
        NX_ASSERT((p)->get_rx_handle != NULL);                                 \
        NX_ASSERT((p)->get_error_count != NULL);                               \
        NX_ASSERT((p)->set_filter != NULL);                                    \
        NX_ASSERT((p)->get_lifecycle != NULL);                                 \
    } while (0)

/**
 * \brief           Create default CAN bus configuration
 * \deprecated      Use Kconfig for compile-time configuration instead.
 *                  This macro is kept for backward compatibility only.
 * \param[in]       _baudrate: Baud rate in bps
 * \return          nx_can_config_t structure
 */
#define NX_CAN_CONFIG_DEFAULT(_baudrate)                                       \
    (nx_can_config_t) {                                                        \
        .baudrate = (_baudrate), .mode = NX_CAN_MODE_NORMAL,                   \
        .auto_retransmit = true, .auto_bus_off = true, .sjw = 1, .bs1 = 4,     \
        .bs2 = 3,                                                              \
    }

#ifdef __cplusplus
}
#endif

#endif /* NX_CAN_H */
