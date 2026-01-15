/**
 * \file            nx_lifecycle.h
 * \brief           Device lifecycle interface
 * \author          Nexus Team
 *
 * This file defines the lifecycle interface that all devices must implement.
 * It provides a consistent way to manage device initialization,
 * deinitialization, suspend, and resume operations.
 */

#ifndef NX_LIFECYCLE_H
#define NX_LIFECYCLE_H

#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Device state enumeration
 *
 * Defines the possible states a device can be in during its lifecycle.
 * State transitions follow these rules:
 * - UNINITIALIZED -> INITIALIZED (via init)
 * - INITIALIZED -> RUNNING (automatic after init, or via resume)
 * - RUNNING -> SUSPENDED (via suspend)
 * - SUSPENDED -> RUNNING (via resume)
 * - Any state -> UNINITIALIZED (via deinit)
 * - Any state -> ERROR (on error)
 */
typedef enum nx_device_state_e {
    NX_DEV_STATE_UNINITIALIZED = 0, /**< Device not initialized */
    NX_DEV_STATE_INITIALIZED,       /**< Device initialized but not running */
    NX_DEV_STATE_RUNNING,           /**< Device running normally */
    NX_DEV_STATE_SUSPENDED,         /**< Device suspended (low power) */
    NX_DEV_STATE_ERROR,             /**< Device in error state */
} nx_device_state_t;

/**
 * \brief           Lifecycle interface structure
 *
 * All devices must implement this interface to support lifecycle management.
 * The interface provides methods for initialization, deinitialization,
 * suspend, and resume operations.
 */
typedef struct nx_lifecycle_s nx_lifecycle_t;
struct nx_lifecycle_s {
    /**
     * \brief       Initialize the device
     * \param[in]   self: Pointer to lifecycle interface
     * \return      NX_OK on success, error code otherwise
     *
     * This function initializes the device hardware and prepares it for use.
     * After successful initialization, the device state should be RUNNING.
     */
    nx_status_t (*init)(nx_lifecycle_t* self);

    /**
     * \brief       Deinitialize the device
     * \param[in]   self: Pointer to lifecycle interface
     * \return      NX_OK on success, error code otherwise
     *
     * This function releases all resources held by the device and returns
     * it to the UNINITIALIZED state. After deinitialization, the device
     * can be reinitialized with init().
     */
    nx_status_t (*deinit)(nx_lifecycle_t* self);

    /**
     * \brief       Suspend the device
     * \param[in]   self: Pointer to lifecycle interface
     * \return      NX_OK on success, error code otherwise
     *
     * This function puts the device into a low-power suspended state.
     * The device configuration is preserved and can be restored with resume().
     */
    nx_status_t (*suspend)(nx_lifecycle_t* self);

    /**
     * \brief       Resume the device from suspended state
     * \param[in]   self: Pointer to lifecycle interface
     * \return      NX_OK on success, error code otherwise
     *
     * This function restores the device from suspended state to running state.
     * The device configuration is restored to what it was before suspend().
     */
    nx_status_t (*resume)(nx_lifecycle_t* self);

    /**
     * \brief       Get current device state
     * \param[in]   self: Pointer to lifecycle interface
     * \return      Current device state
     */
    nx_device_state_t (*get_state)(nx_lifecycle_t* self);
};

/**
 * \brief           Convert device state to string
 * \param[in]       state: Device state
 * \return          String representation of the state
 */
NX_INLINE const char* nx_device_state_to_string(nx_device_state_t state) {
    switch (state) {
        case NX_DEV_STATE_UNINITIALIZED:
            return "UNINITIALIZED";
        case NX_DEV_STATE_INITIALIZED:
            return "INITIALIZED";
        case NX_DEV_STATE_RUNNING:
            return "RUNNING";
        case NX_DEV_STATE_SUSPENDED:
            return "SUSPENDED";
        case NX_DEV_STATE_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* NX_LIFECYCLE_H */
