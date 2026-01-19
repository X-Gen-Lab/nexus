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
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Linker Section Symbols                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Start of device registry section
 */
extern const nx_device_t __nx_device_start[];

/**
 * \brief           End of device registry section
 */
extern const nx_device_t __nx_device_end[];

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

    for (const nx_device_t* dev = __nx_device_start; dev < __nx_device_end;
         dev++) {
        if (dev->name != NULL && strcmp(dev->name, name) == 0) {
            return dev;
        }
    }

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
        return ((nx_device_t*)dev)->api;
    }

    /* Call device-specific initialization function */
    if (dev->device_init == NULL) {
        return NULL;
    }

    void* api = dev->device_init(dev);

    if (api != NULL) {
        /* Cache the API pointer */
        ((nx_device_t*)dev)->api = api;
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
