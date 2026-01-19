/**
 * \file            nx_option_bytes_device.c
 * \brief           Option Bytes device registration for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements Option Bytes device registration using
 *                  Kconfig-driven configuration. Provides factory functions
 *                  for test access and manages Option Bytes instance lifecycle.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_option_bytes.h"
#include "nexus_config.h"
#include "nx_option_bytes_helpers.h"
#include "nx_option_bytes_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define NX_OPTION_BYTES_MAX_INSTANCES 4
#define DEVICE_TYPE                   NX_OPTION_BYTES

/*---------------------------------------------------------------------------*/
/* Static Storage                                                            */
/*---------------------------------------------------------------------------*/

static nx_option_bytes_state_t
    g_option_bytes_states[NX_OPTION_BYTES_MAX_INSTANCES];
static nx_option_bytes_impl_t
    g_option_bytes_instances[NX_OPTION_BYTES_MAX_INSTANCES];
static uint8_t g_option_bytes_instance_count = 0;

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Interface implementations (defined in separate files) */
extern void option_bytes_init_interface(nx_option_bytes_t* option_bytes);
extern void option_bytes_init_lifecycle(nx_lifecycle_t* lifecycle);
extern void option_bytes_init_power(nx_power_t* power);

/*---------------------------------------------------------------------------*/
/* Instance Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize Option Bytes instance
 */
static void option_bytes_init_instance(nx_option_bytes_impl_t* impl,
                                       uint8_t index) {
    /* Initialize interfaces (implemented in separate files) */
    option_bytes_init_interface(&impl->base);
    option_bytes_init_lifecycle(&impl->lifecycle);
    option_bytes_init_power(&impl->power);

    /* Link to state */
    impl->state = &g_option_bytes_states[index];
    impl->state->index = index;
    impl->state->initialized = false;
    impl->state->suspended = false;

    /* Initialize option bytes data with defaults */
    memset(&impl->state->data, 0, sizeof(nx_option_bytes_data_t));
    impl->state->data.read_protection = 0; /* No protection by default */
    impl->state->data.write_protected = false;
    impl->state->data.pending_changes = false;

    /* Initialize pending buffer */
    memset(&impl->state->pending, 0, sizeof(nx_option_bytes_data_t));
    impl->state->pending.pending_changes = false;
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
static void* nx_option_bytes_device_init(const nx_device_t* dev) {
    if (g_option_bytes_instance_count >= NX_OPTION_BYTES_MAX_INSTANCES) {
        return NULL;
    }

    uint8_t index = g_option_bytes_instance_count++;
    nx_option_bytes_impl_t* impl = &g_option_bytes_instances[index];

    /* Initialize instance */
    option_bytes_init_instance(impl, index);

    /* Store device pointer */
    impl->device = (nx_device_t*)dev;

    /* Initialize lifecycle */
    nx_status_t status = impl->lifecycle.init(&impl->lifecycle);
    if (status != NX_OK) {
        return NULL;
    }

    return &impl->base;
}

/* Register all enabled Option Bytes instances */
#if defined(NX_CONFIG_INSTANCE_NX_OPTION_BYTES0)
static nx_device_config_state_t option_bytes_kconfig_state_0 = {
    .init_res = 0,
    .initialized = false,
};
NX_DEVICE_REGISTER(DEVICE_TYPE, 0, "OPTBYTES0", NULL,
                   &option_bytes_kconfig_state_0, nx_option_bytes_device_init);
#endif

/*---------------------------------------------------------------------------*/
/* Factory Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Option Bytes instance by index
 */
nx_option_bytes_t* nx_option_bytes_native_get(uint8_t index) {
    if (index >= NX_OPTION_BYTES_MAX_INSTANCES) {
        return NULL;
    }

    /* Use device registration mechanism */
    char name[16];
    snprintf(name, sizeof(name), "OPTBYTES%d", index);
    return (nx_option_bytes_t*)nx_device_get(name);
}

/**
 * \brief           Reset all Option Bytes instances (for testing)
 */
void nx_option_bytes_native_reset_all(void) {
    for (uint8_t i = 0; i < g_option_bytes_instance_count; i++) {
        nx_option_bytes_impl_t* impl = &g_option_bytes_instances[i];
        if (impl->state && impl->state->initialized) {
            impl->lifecycle.deinit(&impl->lifecycle);
        }
        memset(&g_option_bytes_states[i], 0, sizeof(nx_option_bytes_state_t));
    }
    g_option_bytes_instance_count = 0;
}

/**
 * \brief           Reset Option Bytes instance (for testing)
 */
nx_status_t nx_option_bytes_native_reset(uint8_t index) {
    if (index >= g_option_bytes_instance_count) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_option_bytes_impl_t* impl = &g_option_bytes_instances[index];
    if (impl->state) {
        /* Reinitialize state */
        option_bytes_init_instance(impl, index);
        return NX_OK;
    }

    return NX_ERR_NOT_FOUND;
}

/*---------------------------------------------------------------------------*/
/* Test Support Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Option Bytes state (for testing)
 */
nx_status_t nx_option_bytes_native_get_state(uint8_t index, bool* initialized,
                                             bool* suspended) {
    if (index >= g_option_bytes_instance_count) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_option_bytes_impl_t* impl = &g_option_bytes_instances[index];
    if (impl->state) {
        if (initialized) {
            *initialized = impl->state->initialized;
        }
        if (suspended) {
            *suspended = impl->state->suspended;
        }
        return NX_OK;
    }

    return NX_ERR_NOT_FOUND;
}

/**
 * \brief           Get Option Bytes device descriptor (for testing)
 */
nx_device_t* nx_option_bytes_native_get_device(uint8_t index) {
    if (index >= g_option_bytes_instance_count) {
        return NULL;
    }

    return g_option_bytes_instances[index].device;
}

/*---------------------------------------------------------------------------*/
/* Option Bytes-Specific Test Helpers                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Set write protection status (for testing)
 */
nx_status_t nx_option_bytes_native_set_write_protection(uint8_t index,
                                                        bool is_protected) {
    if (index >= g_option_bytes_instance_count) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_option_bytes_impl_t* impl = &g_option_bytes_instances[index];
    if (impl->state) {
        impl->state->data.write_protected = is_protected;
        return NX_OK;
    }

    return NX_ERR_NOT_FOUND;
}

/**
 * \brief           Get write protection status (for testing)
 */
nx_status_t nx_option_bytes_native_get_write_protection(uint8_t index,
                                                        bool* is_protected) {
    if (index >= g_option_bytes_instance_count || is_protected == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_option_bytes_impl_t* impl = &g_option_bytes_instances[index];
    if (impl->state) {
        *is_protected = impl->state->data.write_protected;
        return NX_OK;
    }

    return NX_ERR_NOT_FOUND;
}

/**
 * \brief           Check if there are pending changes (for testing)
 */
nx_status_t nx_option_bytes_native_has_pending_changes(uint8_t index,
                                                       bool* has_pending) {
    if (index >= g_option_bytes_instance_count || has_pending == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    nx_option_bytes_impl_t* impl = &g_option_bytes_instances[index];
    if (impl->state) {
        *has_pending = impl->state->pending.pending_changes;
        return NX_OK;
    }

    return NX_ERR_NOT_FOUND;
}
