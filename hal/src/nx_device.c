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

/* GCC / Clang / Arm Compiler 6 / IAR / TI / TASKING use standard symbols */
#elif defined(__GNUC__) || defined(__ARMCC_VERSION) || defined(__ICCARM__) ||  \
    defined(__TI_ARM__) || defined(__TASKING__)

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

/* MSVC and unknown compilers without weak symbol support */
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

#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
    /* MSVC: iterate through pointer array */
    for (size_t i = 0; i < __nx_device_count; i++) {
        const nx_device_t* dev = __nx_device_registry[i];
        if (dev->name != NULL && strcmp(dev->name, name) == 0) {
            return dev;
        }
    }
#else
    /* GCC/Clang: iterate through linker section */
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
    if (dev == NULL) {
        return NULL;
    }

    /* Return cached API if already initialized */
    if (dev->state->initialized) {
        return dev->state->api;
    }

    /* Call device-specific initialization function */
    if (dev->device_init == NULL) {
        return NULL;
    }

    void* api = dev->device_init(dev);

    if (api != NULL) {
        /* Cache the API pointer in state (which is writable) */
        dev->state->api = api;
        dev->state->initialized = true;
        dev->state->init_res = 0;
    } else {
        dev->state->init_res = 1;
    }

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
/* Manual Registration (for MSVC and testing)                                */
/*---------------------------------------------------------------------------*/

#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)

/**
 * \brief           Manually register a device
 * \details         This function is only available on platforms without
 *                  linker section support (e.g., MSVC). It allows manual
 *                  registration of devices for testing purposes.
 * \note            This function is not thread-safe
 */
nx_status_t nx_device_register(const nx_device_t* dev) {
    if (dev == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (__nx_device_count >= NX_DEVICE_REGISTRY_SIZE) {
        return NX_ERR_NO_MEMORY;
    }

    __nx_device_registry[__nx_device_count++] = dev;
    return NX_OK;
}

/**
 * \brief           Clear all manually registered devices
 * \details         This function is useful for test cleanup
 * \note            This function is not thread-safe
 */
void nx_device_clear_all(void) {
    __nx_device_count = 0;
}

#endif
