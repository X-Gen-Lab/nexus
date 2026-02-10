/**
 * \file            nx_i2c.h
 * \brief           I2C bus interface definition
 * \author          Nexus Team
 *
 * This file defines the I2C bus interface with Handle acquisition pattern
 * for device isolation. Supports both async and sync communication modes.
 */

#ifndef NX_I2C_H
#define NX_I2C_H

#include "hal/base/nx_comm.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* I2C Bus Interface                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           I2C bus interface
 *
 * Provides access to I2C bus through Handle acquisition pattern.
 * Supports multiple devices with different addresses on the same bus.
 */
typedef struct nx_i2c_bus_s nx_i2c_bus_t;
struct nx_i2c_bus_s {
    /*-----------------------------------------------------------------------*/
    /* Sync Interface Getters                                                */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           Get sync TX handle for a specific device address
     * \param[in]       self: I2C bus pointer
     * \param[in]       dev_addr: Device address (7-bit or 10-bit) - runtime
     *              parameter
     * \return          Sync TX interface pointer, NULL on error
     * \note            Device address is a runtime parameter, allowing multiple
     *              devices with different addresses on the same I2C bus.
     *              Bus-level configuration (speed, pins) is done at
     * compile-time.
     */
    nx_tx_sync_t* (*get_tx_sync_handle)(nx_i2c_bus_t* self, uint8_t dev_addr);

    /**
     * \brief           Get sync TX/RX handle for a specific device address
     * \param[in]       self: I2C bus pointer
     * \param[in]       dev_addr: Device address (7-bit or 10-bit) - runtime
     *              parameter
     * \return          Sync TX/RX interface pointer, NULL on error
     * \note            Device address is a runtime parameter, allowing multiple
     *              devices with different addresses on the same I2C bus.
     *              Bus-level configuration (speed, pins) is done at
     * compile-time.
     */
    nx_tx_rx_sync_t* (*get_tx_rx_sync_handle)(nx_i2c_bus_t* self,
                                              uint8_t dev_addr);

    /*-----------------------------------------------------------------------*/
    /* Async Interface Getters                                               */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           Get async TX handle for a specific device address
     * \param[in]       self: I2C bus pointer
     * \param[in]       dev_addr: Device address (7-bit or 10-bit) - runtime
     *              parameter
     * \return          Async TX interface pointer, NULL on error
     * \note            Device address is a runtime parameter, allowing multiple
     *              devices with different addresses on the same I2C bus.
     */
    nx_tx_async_t* (*get_tx_async_handle)(nx_i2c_bus_t* self, uint8_t dev_addr);

    /**
     * \brief           Get async TX/RX handle for a specific device address
     * \param[in]       self: I2C bus pointer
     * \param[in]       dev_addr: Device address (7-bit or 10-bit) - runtime
     *              parameter
     * \param[in]       callback: Callback for received data
     * \param[in]       user_data: User data for callback
     * \return          Async TX/RX interface pointer, NULL on error
     * \note            Device address is a runtime parameter, allowing multiple
     *              devices with different addresses on the same I2C bus.
     */
    nx_tx_rx_async_t* (*get_tx_rx_async_handle)(nx_i2c_bus_t* self,
                                                uint8_t dev_addr,
                                                nx_comm_callback_t callback,
                                                void* user_data);

    /*-----------------------------------------------------------------------*/
    /* Base Interface Getters                                                */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: I2C bus pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_i2c_bus_t* self);

    /**
     * \brief           Get power interface
     * \param[in]       self: I2C bus pointer
     * \return          Power interface pointer
     */
    nx_power_t* (*get_power)(nx_i2c_bus_t* self);
};

/*---------------------------------------------------------------------------*/
/* I2C Bus Initialization Macro                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize I2C bus interface
 * \param[in]       p: Pointer to nx_i2c_bus_t structure
 * \param[in]       _get_tx_sync_handle: Get TX sync handle function pointer
 * \param[in]       _get_tx_rx_sync_handle: Get TX/RX sync handle function
 * \param[in]       _get_tx_async_handle: Get TX async handle function pointer
 * \param[in]       _get_tx_rx_async_handle: Get TX/RX async handle function
 * \param[in]       _get_lifecycle: Get lifecycle function pointer
 * \param[in]       _get_power: Get power function pointer
 */
#define NX_INIT_I2C_BUS(p, _get_tx_sync_handle, _get_tx_rx_sync_handle,        \
                        _get_tx_async_handle, _get_tx_rx_async_handle,         \
                        _get_lifecycle, _get_power)                            \
    do {                                                                       \
        (p)->get_tx_sync_handle = (_get_tx_sync_handle);                       \
        (p)->get_tx_rx_sync_handle = (_get_tx_rx_sync_handle);                 \
        (p)->get_tx_async_handle = (_get_tx_async_handle);                     \
        (p)->get_tx_rx_async_handle = (_get_tx_rx_async_handle);               \
        (p)->get_lifecycle = (_get_lifecycle);                                 \
        (p)->get_power = (_get_power);                                         \
        NX_ASSERT((p)->get_tx_sync_handle != NULL);                            \
        NX_ASSERT((p)->get_tx_rx_sync_handle != NULL);                         \
        NX_ASSERT((p)->get_tx_async_handle != NULL);                           \
        NX_ASSERT((p)->get_tx_rx_async_handle != NULL);                        \
        NX_ASSERT((p)->get_lifecycle != NULL);                                 \
    } while (0)

/*---------------------------------------------------------------------------*/
/* Backward Compatibility                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Backward compatibility type alias
 *
 * Legacy code uses nx_i2c_t, which now maps to nx_i2c_bus_t.
 */
typedef nx_i2c_bus_t nx_i2c_t;

#ifdef __cplusplus
}
#endif

#endif /* NX_I2C_H */
