/**
 * \file            nx_device_registry.c
 * \brief           Static device registry implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This file implements the static device registry for
 *                  compile-time device registration. Device access with
 *                  reference counting is handled by nx_device.c.
 */

#include "hal/base/nx_device_registry.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* MSVC Test Support                                                         */
/*---------------------------------------------------------------------------*/

#if defined(_MSC_VER)
const nx_device_t* __nx_device_registry_array[1] = {NULL};
const size_t __nx_device_registry_count = 0;
#endif

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

const nx_device_t* nx_device_registry_find(const char* name) {
    if (name == NULL) {
        return NULL;
    }

#if defined(_MSC_VER)
    for (size_t i = 0; i < __nx_device_registry_count; i++) {
        const nx_device_t* dev = __nx_device_registry_array[i];
        if (dev != NULL && dev->name != NULL) {
            if (strcmp(dev->name, name) == 0) {
                return dev;
            }
        }
    }
#else
    for (const nx_device_t* dev = NX_DEVICE_REGISTRY_START;
         dev < NX_DEVICE_REGISTRY_END; dev++) {
        if (dev->name != NULL && strcmp(dev->name, name) == 0) {
            return dev;
        }
    }
#endif

    return NULL;
}

size_t nx_device_registry_count(void) {
#if defined(_MSC_VER)
    return __nx_device_registry_count;
#else
    return (size_t)(NX_DEVICE_REGISTRY_END - NX_DEVICE_REGISTRY_START);
#endif
}

const nx_device_t* nx_device_registry_get_by_index(size_t index) {
    size_t count = nx_device_registry_count();

    if (index >= count) {
        return NULL;
    }

#if defined(_MSC_VER)
    return __nx_device_registry_array[index];
#else
    return &NX_DEVICE_REGISTRY_START[index];
#endif
}

nx_status_t nx_device_registry_init_all(void) {
    nx_status_t result = NX_OK;
    size_t count = nx_device_registry_count();

    for (size_t i = 0; i < count; i++) {
        const nx_device_t* dev = nx_device_registry_get_by_index(i);

        if (dev == NULL || dev->name == NULL) {
            continue;
        }

        if (dev->state.initialized || dev->device_init == NULL) {
            continue;
        }

        nx_device_t* mutable_dev = (nx_device_t*)dev;

        if (dev->default_config != NULL && dev->runtime_config != NULL &&
            dev->config_size > 0) {
            memcpy(mutable_dev->runtime_config, dev->default_config,
                   dev->config_size);
        }

        void* interface = dev->device_init(dev);

        if (interface != NULL) {
            mutable_dev->state.initialized = 1;
            mutable_dev->state.state = NX_DEV_STATE_RUNNING;
            mutable_dev->state.init_result = NX_OK;
            mutable_dev->priv = interface;
        } else {
            mutable_dev->state.init_result = NX_ERR_GENERIC;
            result = NX_ERR_GENERIC;
        }
    }

    return result;
}

nx_status_t nx_device_registry_deinit_all(void) {
    nx_status_t result = NX_OK;
    size_t count = nx_device_registry_count();

    for (size_t i = count; i > 0; i--) {
        const nx_device_t* dev = nx_device_registry_get_by_index(i - 1);

        if (dev == NULL || dev->name == NULL) {
            continue;
        }

        if (!dev->state.initialized || dev->device_deinit == NULL) {
            continue;
        }

        nx_device_t* mutable_dev = (nx_device_t*)dev;
        nx_status_t status = dev->device_deinit(dev);

        if (status == NX_OK) {
            mutable_dev->state.initialized = 0;
            mutable_dev->state.state = NX_DEV_STATE_UNINITIALIZED;
            mutable_dev->priv = NULL;
        } else {
            result = NX_ERR_GENERIC;
        }
    }

    return result;
}
