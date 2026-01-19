/**
 * \file            nx_flash_device.c
 * \brief           Flash device registration for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements Flash device registration using Kconfig-driven
 *                  configuration. Provides factory functions for test access
 *                  and manages Flash instance lifecycle.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_flash.h"
#include "nexus_config.h"
#include "nx_flash_helpers.h"
#include "nx_flash_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define NX_FLASH_MAX_INSTANCES 4
#define DEVICE_TYPE            NX_INTERNAL_FLASH

/*---------------------------------------------------------------------------*/
/* Static Storage                                                            */
/*---------------------------------------------------------------------------*/

static nx_flash_state_t g_flash_states[NX_FLASH_MAX_INSTANCES];
static nx_flash_impl_t g_flash_instances[NX_FLASH_MAX_INSTANCES];
static uint8_t g_flash_instance_count = 0;

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Interface implementations (defined in separate files) */
extern void flash_init_interface(nx_internal_flash_t* flash);
extern void flash_init_lifecycle(nx_lifecycle_t* lifecycle);

/*---------------------------------------------------------------------------*/
/* Instance Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize Flash instance
 */
static void flash_init_instance(nx_flash_impl_t* impl, uint8_t index) {
    /* Initialize interfaces (implemented in separate files) */
    flash_init_interface(&impl->base);
    flash_init_lifecycle(&impl->lifecycle);

    /* Link to state */
    impl->state = &g_flash_states[index];
    impl->state->index = index;
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->locked = true;

    /* Set backing file path */
    snprintf(impl->state->backing_file, sizeof(impl->state->backing_file),
             "native_flash%d.bin", index);

    /* Initialize all sectors as erased */
    for (uint32_t i = 0; i < NX_FLASH_NUM_SECTORS; i++) {
        memset(impl->state->sectors[i].data, NX_FLASH_ERASED_BYTE,
               NX_FLASH_SECTOR_SIZE);
        impl->state->sectors[i].erased = true;
    }
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
static void* nx_flash_device_init(const nx_device_t* dev) {
    if (g_flash_instance_count >= NX_FLASH_MAX_INSTANCES) {
        return NULL;
    }

    uint8_t index = g_flash_instance_count++;
    nx_flash_impl_t* impl = &g_flash_instances[index];

    /* Initialize instance */
    flash_init_instance(impl, index);

    /* Store device pointer */
    impl->device = (nx_device_t*)dev;

    /* Initialize lifecycle */
    nx_status_t status = impl->lifecycle.init(&impl->lifecycle);
    if (status != NX_OK) {
        return NULL;
    }

    return &impl->base;
}

/* Register all enabled Flash instances */
#if defined(NX_CONFIG_INSTANCE_NX_INTERNAL_FLASH0)
static nx_device_config_state_t flash_kconfig_state_0 = {
    .init_res = 0,
    .initialized = false,
};
NX_DEVICE_REGISTER(DEVICE_TYPE, 0, "FLASH0", NULL, &flash_kconfig_state_0,
                   nx_flash_device_init);
#endif
