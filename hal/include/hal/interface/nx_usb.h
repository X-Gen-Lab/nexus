/**
 * \file            nx_usb.h
 * \brief           USB device interface definition
 * \author          Nexus Team
 */

#ifndef NX_USB_H
#define NX_USB_H

#include "hal/base/nx_comm.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* USB Device Interface                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           USB device interface
 *
 * Provides access to USB communication through async/sync interfaces,
 * connection status, and base lifecycle/power interfaces.
 * Supports CDC/HID communication modes.
 */
typedef struct nx_usb_s nx_usb_t;
struct nx_usb_s {
    /**
     * \brief           Get async transmit interface
     * \param[in]       self: USB device pointer
     * \return          Async TX interface pointer
     */
    nx_tx_async_t* (*get_tx_async)(nx_usb_t* self);

    /**
     * \brief           Get async receive interface
     * \param[in]       self: USB device pointer
     * \return          Async RX interface pointer
     */
    nx_rx_async_t* (*get_rx_async)(nx_usb_t* self);

    /**
     * \brief           Get sync transmit interface
     * \param[in]       self: USB device pointer
     * \return          Sync TX interface pointer
     */
    nx_tx_sync_t* (*get_tx_sync)(nx_usb_t* self);

    /**
     * \brief           Get sync receive interface
     * \param[in]       self: USB device pointer
     * \return          Sync RX interface pointer
     */
    nx_rx_sync_t* (*get_rx_sync)(nx_usb_t* self);

    /**
     * \brief           Check USB connection status
     * \param[in]       self: USB device pointer
     * \return          true if connected, false otherwise
     */
    bool (*is_connected)(nx_usb_t* self);

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: USB device pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_usb_t* self);

    /**
     * \brief           Get power interface
     * \param[in]       self: USB device pointer
     * \return          Power interface pointer
     */
    nx_power_t* (*get_power)(nx_usb_t* self);
};

/*---------------------------------------------------------------------------*/
/* USB Initialization Macro                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize USB device interface
 * \param[in]       p: Pointer to nx_usb_t structure
 * \param[in]       _get_tx_async: Get TX async function pointer
 * \param[in]       _get_rx_async: Get RX async function pointer
 * \param[in]       _get_tx_sync: Get TX sync function pointer
 * \param[in]       _get_rx_sync: Get RX sync function pointer
 * \param[in]       _is_connected: Is connected function pointer
 * \param[in]       _get_lifecycle: Get lifecycle function pointer
 * \param[in]       _get_power: Get power function pointer
 */
#define NX_INIT_USB(p, _get_tx_async, _get_rx_async, _get_tx_sync,             \
                    _get_rx_sync, _is_connected, _get_lifecycle, _get_power)   \
    do {                                                                       \
        (p)->get_tx_async = (_get_tx_async);                                   \
        (p)->get_rx_async = (_get_rx_async);                                   \
        (p)->get_tx_sync = (_get_tx_sync);                                     \
        (p)->get_rx_sync = (_get_rx_sync);                                     \
        (p)->is_connected = (_is_connected);                                   \
        (p)->get_lifecycle = (_get_lifecycle);                                 \
        (p)->get_power = (_get_power);                                         \
        NX_ASSERT((p)->get_tx_async != NULL);                                  \
        NX_ASSERT((p)->get_rx_async != NULL);                                  \
        NX_ASSERT((p)->get_tx_sync != NULL);                                   \
        NX_ASSERT((p)->get_rx_sync != NULL);                                   \
        NX_ASSERT((p)->is_connected != NULL);                                  \
        NX_ASSERT((p)->get_lifecycle != NULL);                                 \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* NX_USB_H */
