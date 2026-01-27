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
#include "hal/system/nx_mem.h"
#include "nexus_config.h"
#include "nx_flash_helpers.h"
#include "nx_flash_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE NX_INTERNAL_FLASH

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

    /* Allocate and initialize state */
    impl->state = (nx_flash_state_t*)nx_mem_alloc(sizeof(nx_flash_state_t));
    if (!impl->state) {
        return;
    }
    memset(impl->state, 0, sizeof(nx_flash_state_t));

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
    /* Allocate implementation structure */
    nx_flash_impl_t* impl =
        (nx_flash_impl_t*)nx_mem_alloc(sizeof(nx_flash_impl_t));
    if (!impl) {
        return NULL;
    }
    memset(impl, 0, sizeof(nx_flash_impl_t));

    /* Initialize instance */
    flash_init_instance(impl, 0);

    /* Check if state allocation succeeded */
    if (!impl->state) {
        nx_mem_free(impl);
        return NULL;
    }

    /* Store device pointer */
    impl->device = (nx_device_t*)dev;

    /* Device is created but not initialized - tests will call init() */
    return &impl->base;
}

/**
 * \brief           Device registration macro
 */
#define NX_FLASH_DEVICE_REGISTER(index)                                        \
    static nx_device_config_state_t flash_kconfig_state_##index = {            \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, index, "FLASH" #index, NULL,               \
                       &flash_kconfig_state_##index, nx_flash_device_init);

/**
 * \brief           Register all enabled Flash instances
 */
NX_TRAVERSE_EACH_INSTANCE(NX_FLASH_DEVICE_REGISTER, DEVICE_TYPE)
