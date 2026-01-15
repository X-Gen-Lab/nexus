/**
 * \file            nx_device.c
 * \brief           Device base class implementation
 * \author          Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include <string.h>

/*
 * Device registry configuration
 */
#ifndef NX_DEVICE_MAX_DEVICES
#define NX_DEVICE_MAX_DEVICES 32
#endif

/*
 * Device registry - stores pointers to registered devices
 */
static nx_device_t* g_device_registry[NX_DEVICE_MAX_DEVICES];
static size_t g_device_count = 0;

/*
 * Device interface mapping - maps interface pointers to device descriptors
 * This is needed for nx_device_put() to find the device from interface pointer
 */
static struct {
    void* interface;
    nx_device_t* device;
} g_interface_map[NX_DEVICE_MAX_DEVICES];
static size_t g_interface_count = 0;

/**
 * \brief           Find device in registry by name
 */
static nx_device_t* device_find_internal(const char* name) {
    size_t i;

    if (name == NULL) {
        return NULL;
    }

    for (i = 0; i < g_device_count; i++) {
        if (g_device_registry[i] != NULL &&
            g_device_registry[i]->name != NULL &&
            strcmp(g_device_registry[i]->name, name) == 0) {
            return g_device_registry[i];
        }
    }

    return NULL;
}

/**
 * \brief           Find device by interface pointer
 */
static nx_device_t* device_find_by_interface(void* interface) {
    size_t i;

    if (interface == NULL) {
        return NULL;
    }

    for (i = 0; i < g_interface_count; i++) {
        if (g_interface_map[i].interface == interface) {
            return g_interface_map[i].device;
        }
    }

    return NULL;
}

/**
 * \brief           Add interface to device mapping
 */
static nx_status_t interface_map_add(void* interface, nx_device_t* device) {
    if (g_interface_count >= NX_DEVICE_MAX_DEVICES) {
        return NX_ERR_NO_RESOURCE;
    }

    g_interface_map[g_interface_count].interface = interface;
    g_interface_map[g_interface_count].device = device;
    g_interface_count++;

    return NX_OK;
}

/**
 * \brief           Remove interface from device mapping
 */
static void interface_map_remove(void* interface) {
    size_t i;

    for (i = 0; i < g_interface_count; i++) {
        if (g_interface_map[i].interface == interface) {
            /* Move last entry to this position */
            if (i < g_interface_count - 1) {
                g_interface_map[i] = g_interface_map[g_interface_count - 1];
            }
            g_interface_count--;
            break;
        }
    }
}

nx_status_t nx_device_register(nx_device_t* dev) {
    if (dev == NULL || dev->name == NULL) {
        return NX_ERR_NULL_PTR;
    }

    /* Check if already registered */
    if (device_find_internal(dev->name) != NULL) {
        return NX_ERR_ALREADY_INIT;
    }

    /* Check capacity */
    if (g_device_count >= NX_DEVICE_MAX_DEVICES) {
        return NX_ERR_NO_RESOURCE;
    }

    /* Initialize state */
    dev->state.initialized = 0;
    dev->state.state = NX_DEV_STATE_UNINITIALIZED;
    dev->state.ref_count = 0;
    dev->state.init_result = NX_OK;

    /* Add to registry */
    g_device_registry[g_device_count] = dev;
    g_device_count++;

    return NX_OK;
}

nx_status_t nx_device_unregister(nx_device_t* dev) {
    size_t i;

    if (dev == NULL) {
        return NX_ERR_NULL_PTR;
    }

    /* Check if device has references */
    if (dev->state.ref_count > 0) {
        return NX_ERR_BUSY;
    }

    /* Find and remove from registry */
    for (i = 0; i < g_device_count; i++) {
        if (g_device_registry[i] == dev) {
            /* Move last entry to this position */
            if (i < g_device_count - 1) {
                g_device_registry[i] = g_device_registry[g_device_count - 1];
            }
            g_device_count--;
            return NX_OK;
        }
    }

    return NX_ERR_NOT_FOUND;
}

const nx_device_t* nx_device_find(const char* name) {
    return device_find_internal(name);
}

void* nx_device_get(const char* name) {
    nx_device_t* dev;
    void* interface;

    dev = device_find_internal(name);
    if (dev == NULL) {
        nx_report_error(NX_ERR_NOT_FOUND, "device", "Device not found");
        return NULL;
    }

    /* Check reference count limit */
    if (dev->state.ref_count >= NX_DEVICE_MAX_REF_COUNT) {
        nx_report_error(NX_ERR_NO_RESOURCE, "device",
                        "Max reference count reached");
        return NULL;
    }

    /* Initialize if not already initialized */
    if (!dev->state.initialized) {
        if (dev->device_init == NULL) {
            nx_report_error(NX_ERR_NOT_SUPPORTED, "device", "No init function");
            return NULL;
        }

        /* Copy default config to runtime config if available */
        if (dev->default_config != NULL && dev->runtime_config != NULL &&
            dev->config_size > 0) {
            memcpy(dev->runtime_config, dev->default_config, dev->config_size);
        }

        interface = dev->device_init(dev);
        if (interface == NULL) {
            dev->state.init_result = NX_ERR_GENERIC;
            nx_report_error(NX_ERR_GENERIC, "device", "Device init failed");
            return NULL;
        }

        /* Add to interface map */
        if (interface_map_add(interface, dev) != NX_OK) {
            /* Cleanup on failure */
            if (dev->device_deinit != NULL) {
                dev->device_deinit(dev);
            }
            nx_report_error(NX_ERR_NO_RESOURCE, "device", "Interface map full");
            return NULL;
        }

        dev->state.initialized = 1;
        dev->state.state = NX_DEV_STATE_RUNNING;
        dev->state.init_result = NX_OK;
        dev->priv = interface;
    }

    /* Increment reference count */
    dev->state.ref_count++;

    return dev->priv;
}

nx_status_t nx_device_put(void* dev_intf) {
    nx_device_t* dev;

    if (dev_intf == NULL) {
        return NX_ERR_NULL_PTR;
    }

    dev = device_find_by_interface(dev_intf);
    if (dev == NULL) {
        return NX_ERR_NOT_FOUND;
    }

    /* Check reference count */
    if (dev->state.ref_count == 0) {
        return NX_ERR_INVALID_STATE;
    }

    /* Decrement reference count */
    dev->state.ref_count--;

    /* Deinitialize if reference count reaches zero */
    if (dev->state.ref_count == 0 && dev->state.initialized) {
        if (dev->device_deinit != NULL) {
            nx_status_t status = dev->device_deinit(dev);
            if (NX_IS_ERROR(status)) {
                /* Restore reference count on failure */
                dev->state.ref_count++;
                return status;
            }
        }

        interface_map_remove(dev_intf);
        dev->state.initialized = 0;
        dev->state.state = NX_DEV_STATE_UNINITIALIZED;
        dev->priv = NULL;
    }

    return NX_OK;
}

nx_status_t nx_device_reinit(const nx_device_t* dev, const void* new_config) {
    nx_device_t* mutable_dev;
    void* interface;
    uint8_t saved_ref_count;

    if (dev == NULL) {
        return NX_ERR_NULL_PTR;
    }

    /* Cast away const for internal modification */
    mutable_dev = (nx_device_t*)dev;

    /* Save reference count */
    saved_ref_count = mutable_dev->state.ref_count;

    /* Deinitialize if initialized */
    if (mutable_dev->state.initialized) {
        if (mutable_dev->device_deinit != NULL) {
            nx_status_t status = mutable_dev->device_deinit(mutable_dev);
            if (NX_IS_ERROR(status)) {
                return status;
            }
        }

        if (mutable_dev->priv != NULL) {
            interface_map_remove(mutable_dev->priv);
        }

        mutable_dev->state.initialized = 0;
        mutable_dev->state.state = NX_DEV_STATE_UNINITIALIZED;
        mutable_dev->priv = NULL;
    }

    /* Update configuration */
    if (new_config != NULL && mutable_dev->runtime_config != NULL &&
        mutable_dev->config_size > 0) {
        memcpy(mutable_dev->runtime_config, new_config,
               mutable_dev->config_size);
    } else if (mutable_dev->default_config != NULL &&
               mutable_dev->runtime_config != NULL &&
               mutable_dev->config_size > 0) {
        memcpy(mutable_dev->runtime_config, mutable_dev->default_config,
               mutable_dev->config_size);
    }

    /* Reinitialize */
    if (mutable_dev->device_init == NULL) {
        return NX_ERR_NOT_SUPPORTED;
    }

    interface = mutable_dev->device_init(mutable_dev);
    if (interface == NULL) {
        mutable_dev->state.init_result = NX_ERR_GENERIC;
        return NX_ERR_GENERIC;
    }

    /* Add to interface map */
    if (interface_map_add(interface, mutable_dev) != NX_OK) {
        if (mutable_dev->device_deinit != NULL) {
            mutable_dev->device_deinit(mutable_dev);
        }
        return NX_ERR_NO_RESOURCE;
    }

    mutable_dev->state.initialized = 1;
    mutable_dev->state.state = NX_DEV_STATE_RUNNING;
    mutable_dev->state.init_result = NX_OK;
    mutable_dev->state.ref_count = saved_ref_count;
    mutable_dev->priv = interface;

    return NX_OK;
}

uint8_t nx_device_get_ref_count(const nx_device_t* dev) {
    if (dev == NULL) {
        return 0;
    }
    return dev->state.ref_count;
}

nx_device_state_t nx_device_get_state(const nx_device_t* dev) {
    if (dev == NULL) {
        return NX_DEV_STATE_UNINITIALIZED;
    }
    return (nx_device_state_t)dev->state.state;
}

bool nx_device_is_initialized(const nx_device_t* dev) {
    if (dev == NULL) {
        return false;
    }
    return dev->state.initialized != 0;
}
