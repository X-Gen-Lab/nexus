/**
 * \file            nx_device_kconfig.c
 * \brief           Kconfig-driven device registration implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-17
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This file implements the Kconfig-driven device
 *                  registration mechanism. Devices are registered at
 *                  compile time in the .nx_device linker section and
 *                  discovered at runtime.
 */

#include "hal/base/nx_device.h"
#include "hal/system/nx_mutex.h"
#include "osal/osal.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Linker Section Symbols                                                    */
/*---------------------------------------------------------------------------*/

/* Arm Compiler 4/5 (armcc) uses Image$$ symbols */
#if defined(__CC_ARM)

/**
 * \brief           Start of device registry section
 */
extern const nx_device_t Image$$nx_device$$Base NX_WEAK;

/**
 * \brief           End of device registry section
 */
extern const nx_device_t Image$$nx_device$$Limit NX_WEAK;

static const nx_device_t __nx_device_default[1] = {{0}};

#define DEVICE_START                                                           \
    (&Image$$nx_device$$Base ? (const nx_device_t*)&Image$$nx_device$$Base     \
                             : __nx_device_default)
#define DEVICE_END                                                             \
    (&Image$$nx_device$$Limit ? (const nx_device_t*)&Image$$nx_device$$Limit   \
                              : __nx_device_default)

/* GCC / Clang / Arm Compiler 6 / IAR / TI / TASKING use linker sections */
/* Native platform always uses manual registration even with GCC */
#elif !defined(NEXUS_PLATFORM_NATIVE) &&                                       \
    (defined(__GNUC__) || defined(__ARMCC_VERSION) || defined(__ICCARM__) ||   \
     defined(__TI_ARM__) || defined(__TASKING__))

/**
 * \brief           Start of device registry section
 */
extern const nx_device_t __nx_device_start[] NX_WEAK;

/**
 * \brief           End of device registry section
 */
extern const nx_device_t __nx_device_end[] NX_WEAK;

static const nx_device_t __nx_device_default[1] = {{0}};

#define DEVICE_START                                                           \
    (__nx_device_start ? __nx_device_start : __nx_device_default)
#define DEVICE_END (__nx_device_end ? __nx_device_end : __nx_device_default)

/* Native platform, MSVC, and other compilers use manual registration */
#else

#ifndef NX_DEVICE_REGISTRY_SIZE
#define NX_DEVICE_REGISTRY_SIZE 320
#endif

static const nx_device_t* __nx_device_registry[NX_DEVICE_REGISTRY_SIZE];
static size_t __nx_device_count = 0;

#define DEVICE_START __nx_device_registry
#define DEVICE_END   (__nx_device_registry + __nx_device_count)

#endif

/*---------------------------------------------------------------------------*/
/* Device Lookup Functions                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Find device descriptor by name
 */
const nx_device_t* nx_device_find(const char* name) {
    if (name == NULL) {
        return NULL;
    }

#if NX_DEVICE_MANUAL_REGISTRATION
    uint32_t saved = nx_critical_enter();
    /* Manual registration: iterate through pointer array */
    for (size_t i = 0; i < __nx_device_count; i++) {
        const nx_device_t* dev = __nx_device_registry[i];
        if (dev->name != NULL && strcmp(dev->name, name) == 0) {
            nx_critical_exit(saved);
            return dev;
        }
    }
    nx_critical_exit(saved);
#else
    /* Linker section: iterate through section */
    for (const nx_device_t* dev = DEVICE_START; dev < DEVICE_END; dev++) {
        if (dev->name != NULL && strcmp(dev->name, name) == 0) {
            return dev;
        }
    }
#endif

    return NULL;
}

/**
 * \brief           Initialize device and cache the API pointer
 */
void* nx_device_init(const nx_device_t* dev) {
    if (osal_is_isr() || dev == NULL || dev->state == NULL || dev->device_init == NULL) {
        return NULL;
    }

    /* Return cached API if already initialized */
    uint32_t saved = nx_critical_enter();
    if (dev->state->initialized) {
        void* api = dev->state->api;
        nx_critical_exit(saved);
        return api;
    }

    /* Call device-specific initialization function */
    if (dev->state->initializing) {
        nx_critical_exit(saved);
        return NULL;
    }
    dev->state->initializing = true;
    nx_critical_exit(saved);

    void* api = dev->device_init(dev);
    saved = nx_critical_enter();
    if (api != NULL) {
        /* Cache the API pointer in state (which is writable) */
        dev->state->api = api;
        dev->state->initialized = true;
        dev->state->init_res = 0;
    } else {
        dev->state->init_res = 1;
    }
    dev->state->initializing = false;
    nx_critical_exit(saved);

    return api;
}

/**
 * \brief           Get device by name (find + init)
 */
void* nx_device_get(const char* name) {
    const nx_device_t* dev = nx_device_find(name);
    if (dev == NULL) {
        return NULL;
    }

    return nx_device_init(dev);
}

/*---------------------------------------------------------------------------*/
/* Manual Registration (for MSVC, native platform, and testing)              */
/*---------------------------------------------------------------------------*/

#if NX_DEVICE_MANUAL_REGISTRATION

/**
 * \brief           Manually register a device
 * \details         This function is available on platforms without linker
 *                  section support (e.g., MSVC, Windows native testing).
 *                  It allows manual registration of devices.
 * \note            Registry updates are protected by the architecture port.
 */
nx_status_t nx_device_register(const nx_device_t* dev) {
    if (dev == NULL) {
        return NX_ERR_NULL_PTR;
    }

    uint32_t saved = nx_critical_enter();
    for (size_t i = 0; i < __nx_device_count; ++i) {
        if (__nx_device_registry[i] == dev) {
            nx_critical_exit(saved);
            return NX_OK;
        }
        if (dev->name && __nx_device_registry[i]->name &&
            strcmp(dev->name, __nx_device_registry[i]->name) == 0) {
            nx_critical_exit(saved);
            return NX_ERR_ALREADY_INIT;
        }
    }
    if (__nx_device_count >= NX_DEVICE_REGISTRY_SIZE) {
        nx_critical_exit(saved);
        return NX_ERR_NO_MEMORY;
    }

    __nx_device_registry[__nx_device_count++] = dev;
    nx_critical_exit(saved);
    return NX_OK;
}

/**
 * \brief           Clear all manually registered devices
 * \details         This function is useful for test cleanup
 * \note            Registry updates are protected by the architecture port.
 */
void nx_device_clear_all(void) {
    uint32_t saved = nx_critical_enter();
    __nx_device_count = 0;
    nx_critical_exit(saved);
}

#endif
