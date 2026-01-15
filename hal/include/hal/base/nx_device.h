/**
 * \file            nx_device.h
 * \brief           Device base class definition
 * \author          Nexus Team
 *
 * This file defines the device base class that all HAL devices inherit from.
 * It provides common functionality including reference counting, state
 * management, and lifecycle operations.
 */

#ifndef NX_DEVICE_H
#define NX_DEVICE_H

#include "hal/interface/nx_lifecycle.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Maximum reference count value
 */
#define NX_DEVICE_MAX_REF_COUNT 15

/**
 * \brief           Forward declaration of device structure
 */
typedef struct nx_device_s nx_device_t;

/**
 * \brief           Device initialization function type
 * \param[in]       dev: Device descriptor
 * \return          Device interface pointer, NULL on failure
 *
 * This function is called to initialize the device and return
 * a pointer to the device-specific interface structure.
 */
typedef void* (*nx_device_init_fn)(const nx_device_t* dev);

/**
 * \brief           Device deinitialization function type
 * \param[in]       dev: Device descriptor
 * \return          NX_OK on success, error code otherwise
 */
typedef nx_status_t (*nx_device_deinit_fn)(const nx_device_t* dev);

/**
 * \brief           Device suspend function type
 * \param[in]       dev: Device descriptor
 * \return          NX_OK on success, error code otherwise
 */
typedef nx_status_t (*nx_device_suspend_fn)(const nx_device_t* dev);

/**
 * \brief           Device resume function type
 * \param[in]       dev: Device descriptor
 * \return          NX_OK on success, error code otherwise
 */
typedef nx_status_t (*nx_device_resume_fn)(const nx_device_t* dev);

/**
 * \brief           Device state structure (packed for memory efficiency)
 */
typedef struct nx_device_state_s {
    uint8_t init_result;     /**< Initialization result (nx_status_t) */
    uint8_t initialized : 1; /**< Initialization flag */
    uint8_t state       : 3; /**< Device state (nx_device_state_t) */
    uint8_t ref_count   : 4; /**< Reference count (max 15) */
} nx_device_state_info_t;

/**
 * \brief           Device descriptor structure
 *
 * This structure describes a device and provides the necessary
 * information and function pointers for device management.
 * Device descriptors are typically defined statically.
 */
struct nx_device_s {
    const char* name;           /**< Device name (unique identifier) */
    const void* default_config; /**< Default configuration (ROM) */
    void* runtime_config;       /**< Runtime configuration (RAM) */
    size_t config_size;         /**< Configuration structure size */

    nx_device_state_info_t state; /**< Device state information */

    /* Lifecycle function pointers */
    nx_device_init_fn device_init;     /**< Device initialization function */
    nx_device_deinit_fn device_deinit; /**< Device deinitialization function */
    nx_device_suspend_fn device_suspend; /**< Device suspend function */
    nx_device_resume_fn device_resume;   /**< Device resume function */

    void* priv; /**< Private data pointer */
};

/**
 * \brief           Get device interface (increment reference count)
 * \param[in]       name: Device name
 * \return          Device interface pointer, NULL if not found or error
 *
 * This function looks up a device by name, initializes it if necessary,
 * and increments the reference count. The returned pointer should be
 * released with nx_device_put() when no longer needed.
 *
 * If the device is already initialized, this function returns the
 * existing interface and increments the reference count.
 */
void* nx_device_get(const char* name);

/**
 * \brief           Release device (decrement reference count)
 * \param[in]       dev_intf: Device interface pointer
 * \return          NX_OK on success, error code otherwise
 *
 * This function decrements the reference count for the device.
 * When the reference count reaches zero, the device is deinitialized
 * and its resources are released.
 */
nx_status_t nx_device_put(void* dev_intf);

/**
 * \brief           Find device descriptor (does not increment reference count)
 * \param[in]       name: Device name
 * \return          Device descriptor pointer, NULL if not found
 *
 * This function looks up a device by name without initializing it
 * or modifying the reference count. Useful for checking if a device
 * exists or querying its state.
 */
const nx_device_t* nx_device_find(const char* name);

/**
 * \brief           Reinitialize device with new configuration
 * \param[in]       dev: Device descriptor
 * \param[in]       new_config: New configuration (NULL to use default)
 * \return          NX_OK on success, error code otherwise
 *
 * This function deinitializes the device, updates its configuration,
 * and reinitializes it. The reference count is preserved.
 */
nx_status_t nx_device_reinit(const nx_device_t* dev, const void* new_config);

/**
 * \brief           Get device reference count
 * \param[in]       dev: Device descriptor
 * \return          Current reference count
 */
uint8_t nx_device_get_ref_count(const nx_device_t* dev);

/**
 * \brief           Get device state
 * \param[in]       dev: Device descriptor
 * \return          Current device state
 */
nx_device_state_t nx_device_get_state(const nx_device_t* dev);

/**
 * \brief           Check if device is initialized
 * \param[in]       dev: Device descriptor
 * \return          true if initialized, false otherwise
 */
bool nx_device_is_initialized(const nx_device_t* dev);

/**
 * \brief           Register a device with the device manager
 * \param[in]       dev: Device descriptor to register
 * \return          NX_OK on success, error code otherwise
 *
 * This function registers a device descriptor with the device manager.
 * Devices must be registered before they can be accessed via nx_device_get().
 */
nx_status_t nx_device_register(nx_device_t* dev);

/**
 * \brief           Unregister a device from the device manager
 * \param[in]       dev: Device descriptor to unregister
 * \return          NX_OK on success, error code otherwise
 *
 * This function removes a device from the device manager.
 * The device must have a reference count of zero.
 */
nx_status_t nx_device_unregister(nx_device_t* dev);

/**
 * \brief           Device table entry macro for static device registration
 * \param[in]       _name: Device name string
 * \param[in]       _default_cfg: Pointer to default configuration
 * \param[in]       _runtime_cfg: Pointer to runtime configuration buffer
 * \param[in]       _cfg_size: Size of configuration structure
 * \param[in]       _init: Initialization function
 * \param[in]       _deinit: Deinitialization function
 * \param[in]       _suspend: Suspend function (can be NULL)
 * \param[in]       _resume: Resume function (can be NULL)
 */
#define NX_DEVICE_DEFINE(_name, _default_cfg, _runtime_cfg, _cfg_size, _init,  \
                         _deinit, _suspend, _resume)                           \
    {                                                                          \
        .name = (_name),                                                       \
        .default_config = (_default_cfg),                                      \
        .runtime_config = (_runtime_cfg),                                      \
        .config_size = (_cfg_size),                                            \
        .state =                                                               \
            {                                                                  \
                .init_result = NX_OK,                                          \
                .initialized = 0,                                              \
                .state = NX_DEV_STATE_UNINITIALIZED,                           \
                .ref_count = 0,                                                \
            },                                                                 \
        .device_init = (_init),                                                \
        .device_deinit = (_deinit),                                            \
        .device_suspend = (_suspend),                                          \
        .device_resume = (_resume),                                            \
        .priv = NULL,                                                          \
    }

#ifdef __cplusplus
}
#endif

#endif /* NX_DEVICE_H */
