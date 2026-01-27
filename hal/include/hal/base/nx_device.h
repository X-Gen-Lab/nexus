/**
 * \file            nx_device.h
 * \brief           Kconfig-driven device registration mechanism
 * \author          Nexus Team
 *
 * \details         This file provides a simplified device registration
 *                  mechanism driven by Kconfig.Devices are registered at
 *                  compile time using macros that traverse Kconfig-enabled
 *                  instances.
 */

#ifndef NX_DEVICE_H
#define NX_DEVICE_H

#include "hal/nx_status.h"
#include "hal/nx_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Helper Macros                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Concatenate two tokens
 */
#define _NX_CONCAT(a, ...) a##__VA_ARGS__
#define NX_CONCAT(a, ...)  _NX_CONCAT(a, __VA_ARGS__)

/*---------------------------------------------------------------------------*/
/* Platform Detection                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Determine if manual device registration is needed
 * \details         Manual registration is used when:
 *                  - Native platform (for testing on host)
 *                  - MSVC compiler (no linker section support)
 *                  - Unknown compilers without linker section support
 */
#if defined(NEXUS_PLATFORM_NATIVE) || defined(_MSC_VER) ||                     \
    (!defined(__GNUC__) && !defined(__ARMCC_VERSION) &&                        \
     !defined(__ICCARM__) && !defined(__TI_ARM__) && !defined(__TASKING__) &&  \
     !defined(__CC_ARM))
#define NX_DEVICE_MANUAL_REGISTRATION 1
#else
#define NX_DEVICE_MANUAL_REGISTRATION 0
#endif

/*---------------------------------------------------------------------------*/
/* Device State Structure                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device Kconfig state structure
 */
typedef struct nx_device_config_state_s {
    uint8_t init_res; /**< Initialization result */
    bool initialized; /**< Initialization flag */
    void* api;        /**< Cached API pointer */
} nx_device_config_state_t;

/*---------------------------------------------------------------------------*/
/* Device Descriptor Structure                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device descriptor structure
 *
 * This structure describes a device registered at compile time.
 * It contains the device name, configuration, state, and initialization
 * function pointer.
 */
typedef struct nx_device_s {
    const char* name;                                    /**< Device name */
    const void* config;                                  /**< Device config */
    struct nx_device_config_state_s* state;              /**< Device state */
    void* (*device_init)(const struct nx_device_s* dev); /**< Init fn */
} nx_device_t;

/*---------------------------------------------------------------------------*/
/* Device Registration Macro                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Register a device at compile time
 * \param[in]       device_type: Device type identifier (e.g., NX_UART)
 * \param[in]       index: Device index
 * \param[in]       device_name: Device name string
 * \param[in]       device_config: Pointer to device configuration
 * \param[in]       device_state: Pointer to device state
 * \param[in]       init: Device initialization function
 *
 * This macro registers a device in the .nx_device linker section.
 * The device will be automatically discovered at runtime.
 *
 * Example:
 * \code
 * NX_DEVICE_REGISTER(NX_UART, 0, "UART0", &uart0_config,
 *                    &uart0_state, uart0_init);
 * \endcode
 */
#if NX_DEVICE_MANUAL_REGISTRATION
/* Manual registration: Devices are not static */
#define NX_DEVICE_REGISTER(device_type, index, device_name, device_config,     \
                           device_state, init)                                 \
    const nx_device_t NX_CONCAT(device_type, index) = {                        \
        .name = device_name,                                                   \
        .config = device_config,                                               \
        .state = device_state,                                                 \
        .device_init = init,                                                   \
    }
#else
/* Linker section: Devices are static and placed in .nx_device section */
#define NX_DEVICE_REGISTER(device_type, index, device_name, device_config,     \
                           device_state, init)                                 \
    NX_USED NX_SECTION(".nx_device")                                           \
        NX_ALIGNED(sizeof(void*)) static const nx_device_t                     \
        NX_CONCAT(device_type, index) = {                                      \
            .name = device_name,                                               \
            .config = device_config,                                           \
            .state = device_state,                                             \
            .device_init = init,                                               \
    }
#endif

/*---------------------------------------------------------------------------*/
/* Instance Traversal Macro                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Traverse all enabled instances of a device type
 * \param[in]       fn: Function macro to call for each instance
 * \param[in]       device_type: Device type identifier
 *
 * This macro expands to call the provided function macro for each
 * enabled instance of the device type. The instance list is generated
 * by Kconfig using NX_DEFINE_INSTANCE_* macros.
 *
 * Example:
 * \code
 * #define UART_REGISTER(index) \
 *     NX_DEVICE_REGISTER(NX_UART, index, "UART" #index, ...)
 *
 * NX_TRAVERSE_EACH_INSTANCE(UART_REGISTER, NX_UART);
 * \endcode
 */
#define NX_TRAVERSE_EACH_INSTANCE(fn, device_type)                             \
    NX_CONCAT(NX_DEFINE_INSTANCE_, device_type)(fn)

/*---------------------------------------------------------------------------*/
/* Device Lookup Functions                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Find device descriptor by name
 * \param[in]       name: Device name
 * \return          Device descriptor pointer, NULL if not found
 * \note            This function only finds the device, does not initialize
 */
const nx_device_t* nx_device_find(const char* name);

/**
 * \brief           Initialize device and cache the API pointer
 * \param[in]       dev: Device descriptor
 * \return          Device API pointer, NULL on failure
 * \note            This function caches the API pointer after first init
 *                  Subsequent calls return the cached pointer
 */
void* nx_device_init(const nx_device_t* dev);

/**
 * \brief           Get device by name (find + init)
 * \param[in]       name: Device name
 * \return          Device API pointer, NULL if not found or init failed
 * \note            This is a convenience function combining find and init
 */
void* nx_device_get(const char* name);

/*---------------------------------------------------------------------------*/
/* Manual Registration (for MSVC, native platform, and testing)              */
/*---------------------------------------------------------------------------*/

#if NX_DEVICE_MANUAL_REGISTRATION

/**
 * \brief           Manually register a device
 * \param[in]       dev: Device descriptor pointer
 * \return          NX_OK on success, error code otherwise
 * \note            Available on platforms without linker section support
 *                  This function is not thread-safe
 */
nx_status_t nx_device_register(const nx_device_t* dev);

/**
 * \brief           Clear all manually registered devices
 * \note            Useful for test cleanup
 *                  This function is not thread-safe
 */
void nx_device_clear_all(void);

#endif

#ifdef __cplusplus
}
#endif

#endif /* NX_DEVICE_H */
