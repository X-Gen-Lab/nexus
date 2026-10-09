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

#include "hal/provider/nx_device_provider.h"
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

#define DEVICE_TYPE NX_INTERNAL_FLASH

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Interface implementations (defined in separate files) */
extern void flash_init_interface(nx_internal_flash_t* flash);
extern void flash_init_lifecycle(nx_lifecycle_t* lifecycle);

/** Backing image and interface identity remain attached to one descriptor. */
typedef struct {
    nx_device_config_state_t core;
    nx_flash_impl_t impl;
    nx_flash_state_t state;
    uint8_t index;
} native_flash_storage_t;

static nx_status_t nx_flash_construct(const nx_device_t* dev, void** out) {
    if (!out) return NX_ERR_NULL_PTR;
    *out = NULL;
    if (!dev || !dev->state) return NX_ERR_INVALID_PARAM;
    native_flash_storage_t* storage = NX_CONTAINER_OF(dev->state, native_flash_storage_t, core);
    nx_flash_impl_t* impl = &storage->impl;
    memset(impl, 0, sizeof(*impl));
    impl->state = &storage->state;
    memset(impl->state, 0, sizeof(*impl->state));
    impl->state->index = storage->index;
    impl->state->locked = true;
    int n = snprintf(impl->state->backing_file, sizeof(impl->state->backing_file),
        "native_flash%u.bin", (unsigned)storage->index);
    if (n < 0 || (size_t)n >= sizeof(impl->state->backing_file))
        return NX_ERR_INVALID_PARAM;
    for (uint32_t i = 0; i < NX_FLASH_NUM_SECTORS; ++i) {
        memset(impl->state->sectors[i].data, NX_FLASH_ERASED_BYTE, NX_FLASH_SECTOR_SIZE);
        impl->state->sectors[i].erased = true;
    }
    impl->device = (nx_device_t*)dev;
    flash_init_interface(&impl->base);
    flash_init_lifecycle(&impl->lifecycle);
    *out = &impl->base;
    return NX_OK;
}
#define NX_FLASH_DEVICE_REGISTER(index_)                                      \
    static native_flash_storage_t flash_storage_##index_ = {.index = index_}; \
    NX_DEVICE_REGISTER_TYPED(DEVICE_TYPE, index_, "FLASH" #index_, NULL,      \
        &flash_storage_##index_.core, NX_DEVICE_CLASS_FLASH,                  \
        NX_DEVICE_CAP_FLASH_GEOMETRY | NX_DEVICE_CAP_FLASH_PROGRAM |          \
            NX_DEVICE_CAP_FLASH_ERASE, nx_flash_construct, NULL);
NX_TRAVERSE_EACH_INSTANCE(NX_FLASH_DEVICE_REGISTER, DEVICE_TYPE)
