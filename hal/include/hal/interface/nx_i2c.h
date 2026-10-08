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
 * Pointer getters are the existing bounded Native model interface. Each
 * address/callback/context key is immutable for the bus object's lifetime;
 * pool exhaustion returns NULL. They have no close operation. New applications
 * should use open_device/close_device for reclaimable, generation-safe values.
 * Legacy Native async requires service(), copies at most 256 TX bytes, and uses
 * get_state() for terminal errors because nx_comm_callback_t carries RX only.
 */
typedef struct nx_i2c_bus_s nx_i2c_bus_t;
typedef struct nx_i2c_device_s nx_i2c_device_t;
typedef void (*nx_i2c_terminal_callback_t)(void* user_data, nx_status_t status);

/** Task-context transaction. Addresses are unshifted 7-bit values (0..127).
 * timeout_ms is the total lock + operation budget; 0 does not start, UINT32_MAX
 * is permitted only for synchronous transfer. NULL TX is valid when tx_length
 * is zero. RX uses an explicit capacity and an admitted transaction writes
 * received_length on return (zero on failure).
 * An admitted transaction delivers its optional terminal callback once after
 * bus unlock. Async requires a callback and a finite nonzero deadline: all
 * buffers and received_length stay valid until that callback. A dedicated task
 * must call bus.service(). cancel requests cancellation; the transfer return or
 * terminal callback returns buffer ownership. Native responses are explicitly
 * injected; no echo response is manufactured when a device has no data.
 */
typedef struct nx_i2c_transaction_s {
    const uint8_t* tx_data;
    size_t tx_length;
    uint8_t* rx_data;
    size_t rx_capacity;
    size_t* received_length;
    uint32_t timeout_ms;
    nx_i2c_terminal_callback_t callback;
    void* user_data;
} nx_i2c_transaction_t;

/** Caller-owned value. Copies are invalid after close/deinit and cannot access
 * a reused slot. Generation exhaustion fails closed. Device address is copied
 * at open and immutable. Close/lifecycle changes return BUSY while a transaction
 * is queued, waiting, active, or delivering its callback. */
struct nx_i2c_device_s {
    nx_i2c_bus_t* owner;
    uint64_t token;
    nx_status_t (*transfer)(nx_i2c_device_t*, const nx_i2c_transaction_t*);
    nx_status_t (*submit)(nx_i2c_device_t*, const nx_i2c_transaction_t*);
    nx_status_t (*cancel)(nx_i2c_device_t*);
};
struct nx_i2c_bus_s {
    /** Optional modern API. NULL means that backend does not implement it. */
    nx_status_t (*open_device)(nx_i2c_bus_t*, uint8_t dev_addr,
                              nx_i2c_device_t* device);
    nx_status_t (*close_device)(nx_i2c_bus_t*, nx_i2c_device_t* device);
    nx_status_t (*service)(nx_i2c_bus_t*);
    /*-----------------------------------------------------------------------*/
    /* Sync Interface Getters                                                */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           Get sync TX handle for a specific device address
     * \param[in]       self: I2C bus pointer
     * \param[in]       dev_addr: Unshifted 7-bit device address - runtime
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
     * \param[in]       dev_addr: Unshifted 7-bit device address - runtime
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
     * \param[in]       dev_addr: Unshifted 7-bit device address - runtime
     *              parameter
     * \return          Async TX interface pointer, NULL on error
     * \note            Device address is a runtime parameter, allowing multiple
     *              devices with different addresses on the same I2C bus.
     */
    nx_tx_async_t* (*get_tx_async_handle)(nx_i2c_bus_t* self, uint8_t dev_addr);

    /**
     * \brief           Get async TX/RX handle for a specific device address
     * \param[in]       self: I2C bus pointer
     * \param[in]       dev_addr: Unshifted 7-bit device address - runtime
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
        (p)->open_device = NULL;                                               \
        (p)->close_device = NULL;                                              \
        (p)->service = NULL;                                                   \
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
