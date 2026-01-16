/**
 * \file            nx_device_registry.h
 * \brief           Static device registry for compile-time registration
 * \author          Nexus Team
 *
 * \details         This file provides compile-time device registration using
 *                  linker sections. Use nx_device.h for device access API.
 */

#ifndef NX_DEVICE_REGISTRY_H
#define NX_DEVICE_REGISTRY_H

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Linker Section Definitions                                                */
/*---------------------------------------------------------------------------*/

#if defined(_MSC_VER)
extern const nx_device_t* __nx_device_registry_array[];
extern const size_t __nx_device_registry_count;

#define NX_DEVICE_REGISTRY_START __nx_device_registry_array
#define NX_DEVICE_REGISTRY_END                                                 \
    (__nx_device_registry_array + __nx_device_registry_count)

#elif defined(__GNUC__) && !defined(__ARMCC_VERSION)
extern const nx_device_t __nx_device_start[];
extern const nx_device_t __nx_device_end[];

#define NX_DEVICE_REGISTRY_START __nx_device_start
#define NX_DEVICE_REGISTRY_END   __nx_device_end

#elif defined(__ARMCC_VERSION)
extern const nx_device_t Image$nx_device$Base[];
extern const nx_device_t Image$nx_device$Limit[];

#define NX_DEVICE_REGISTRY_START Image$nx_device$Base
#define NX_DEVICE_REGISTRY_END   Image$nx_device$Limit

#elif defined(__ICCARM__)
#pragma section = "nx_device"

#define NX_DEVICE_REGISTRY_START                                               \
    ((const nx_device_t*)__section_begin("nx_device"))
#define NX_DEVICE_REGISTRY_END ((const nx_device_t*)__section_end("nx_device"))

#else
#error "Unsupported compiler for static device registry"
#endif

/*---------------------------------------------------------------------------*/
/* Static Registration Macros                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Register a device at compile time
 * \param[in]       _var: Variable name for the device descriptor
 * \param[in]       _name: Device name string (must be unique)
 * \param[in]       _default_cfg: Pointer to default configuration
 * \param[in]       _runtime_cfg: Pointer to runtime configuration buffer
 * \param[in]       _cfg_size: Size of configuration structure
 * \param[in]       _init: Initialization function
 * \param[in]       _deinit: Deinitialization function
 * \param[in]       _suspend: Suspend function (can be NULL)
 * \param[in]       _resume: Resume function (can be NULL)
 */
#if defined(_MSC_VER)
#define NX_DEVICE_REGISTER(_var, _name, _default_cfg, _runtime_cfg, _cfg_size, \
                           _init, _deinit, _suspend, _resume)                  \
    static nx_device_t _var =                                                  \
        NX_DEVICE_DEFINE(_name, _default_cfg, _runtime_cfg, _cfg_size, _init,  \
                         _deinit, _suspend, _resume)
#else
#define NX_DEVICE_REGISTER(_var, _name, _default_cfg, _runtime_cfg, _cfg_size, \
                           _init, _deinit, _suspend, _resume)                  \
    NX_USED NX_ALIGNED(sizeof(void*))                                          \
        const nx_device_t _var NX_SECTION(".nx_device") =                      \
            NX_DEVICE_DEFINE(_name, _default_cfg, _runtime_cfg, _cfg_size,     \
                             _init, _deinit, _suspend, _resume)
#endif

/**
 * \brief           Iterate over all statically registered devices
 * \param[in]       _dev: Variable name for device pointer
 */
#if defined(_MSC_VER)
#define NX_DEVICE_FOREACH(_dev)                                                \
    for (size_t _nx_dev_idx = 0;                                               \
         _nx_dev_idx < __nx_device_registry_count &&                           \
         ((_dev) = __nx_device_registry_array[_nx_dev_idx], 1);                \
         _nx_dev_idx++)
#else
#define NX_DEVICE_FOREACH(_dev)                                                \
    for (const nx_device_t* _dev = NX_DEVICE_REGISTRY_START;                   \
         _dev < NX_DEVICE_REGISTRY_END; _dev++)
#endif

/**
 * \brief           Get the number of statically registered devices
 */
#if defined(_MSC_VER)
#define NX_DEVICE_COUNT() __nx_device_registry_count
#else
#define NX_DEVICE_COUNT()                                                      \
    ((size_t)(NX_DEVICE_REGISTRY_END - NX_DEVICE_REGISTRY_START))
#endif

/*---------------------------------------------------------------------------*/
/* Static Registry API                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Find a device in the static registry
 * \param[in]       name: Device name to search for
 * \return          Pointer to device descriptor, NULL if not found
 * \note            For device access with ref counting, use nx_device_get()
 */
const nx_device_t* nx_device_registry_find(const char* name);

/**
 * \brief           Get the number of statically registered devices
 * \return          Number of devices
 */
size_t nx_device_registry_count(void);

/**
 * \brief           Get a device by index from the static registry
 * \param[in]       index: Zero-based index
 * \return          Pointer to device descriptor, NULL if out of bounds
 */
const nx_device_t* nx_device_registry_get_by_index(size_t index);

/**
 * \brief           Initialize all statically registered devices
 * \return          NX_OK if all succeeded, error code if any failed
 */
nx_status_t nx_device_registry_init_all(void);

/**
 * \brief           Deinitialize all statically registered devices
 * \return          NX_OK if all succeeded, error code if any failed
 * \note            Devices are deinitialized in reverse order
 */
nx_status_t nx_device_registry_deinit_all(void);

#ifdef __cplusplus
}
#endif

#endif /* NX_DEVICE_REGISTRY_H */
