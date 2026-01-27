/**
 * \file            nx_sdio_device.c
 * \brief           SDIO device registration for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements SDIO device registration and initialization
 *                  using Kconfig-driven configuration.
 */

#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "hal/system/nx_mem.h"
#include "nexus_config.h"
#include "nx_sdio_helpers.h"
#include "nx_sdio_types.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE NX_SDIO

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Lifecycle interface functions */
static nx_status_t sdio_lifecycle_init(nx_lifecycle_t* self);
static nx_status_t sdio_lifecycle_deinit(nx_lifecycle_t* self);
static nx_status_t sdio_lifecycle_suspend(nx_lifecycle_t* self);
static nx_status_t sdio_lifecycle_resume(nx_lifecycle_t* self);
static nx_device_state_t sdio_lifecycle_get_state(nx_lifecycle_t* self);

/* Power interface functions */
static nx_status_t sdio_power_enable(nx_power_t* self);
static nx_status_t sdio_power_disable(nx_power_t* self);
static bool sdio_power_is_enabled(nx_power_t* self);
static nx_status_t sdio_power_set_callback(nx_power_t* self,
                                           nx_power_callback_t callback,
                                           void* user_data);

/* SDIO interface functions */
static nx_status_t sdio_read(nx_sdio_t* self, uint32_t block, uint8_t* data,
                             size_t block_count);
static nx_status_t sdio_write(nx_sdio_t* self, uint32_t block,
                              const uint8_t* data, size_t block_count);
static nx_status_t sdio_erase(nx_sdio_t* self, uint32_t start_block,
                              size_t block_count);
static size_t sdio_get_block_size(nx_sdio_t* self);
static uint64_t sdio_get_capacity(nx_sdio_t* self);
static bool sdio_is_present(nx_sdio_t* self);
static nx_lifecycle_t* sdio_get_lifecycle(nx_sdio_t* self);
static nx_power_t* sdio_get_power(nx_sdio_t* self);

/*---------------------------------------------------------------------------*/
/* Device Initialization                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
static void* nx_sdio_device_init(const nx_device_t* dev) {
    const nx_sdio_platform_config_t* config =
        (const nx_sdio_platform_config_t*)dev->config;

    if (config == NULL) {
        return NULL;
    }

    /* Allocate implementation structure */
    nx_sdio_impl_t* impl =
        (nx_sdio_impl_t*)nx_mem_alloc(sizeof(nx_sdio_impl_t));
    if (!impl) {
        return NULL;
    }
    memset(impl, 0, sizeof(nx_sdio_impl_t));

    /* Allocate state structure */
    nx_sdio_state_t* state =
        (nx_sdio_state_t*)nx_mem_alloc(sizeof(nx_sdio_state_t));
    if (!state) {
        nx_mem_free(impl);
        return NULL;
    }
    memset(state, 0, sizeof(nx_sdio_state_t));

    /* Allocate blocks */
    nx_sdio_block_t* blocks = (nx_sdio_block_t*)nx_mem_alloc(
        sizeof(nx_sdio_block_t) * config->num_blocks);
    if (!blocks) {
        nx_mem_free(state);
        nx_mem_free(impl);
        return NULL;
    }
    memset(blocks, 0, sizeof(nx_sdio_block_t) * config->num_blocks);

    /* Initialize state */
    state->index = config->sdio_index;
    state->card_present = config->card_present;
    state->blocks = blocks;
    state->initialized = false;
    state->suspended = false;

    /* Initialize configuration from platform config */
    state->config.clock_speed = config->clock_speed;
    state->config.bus_width = config->bus_width;

    /* Initialize lifecycle interface */
    impl->lifecycle.init = sdio_lifecycle_init;
    impl->lifecycle.deinit = sdio_lifecycle_deinit;
    impl->lifecycle.suspend = sdio_lifecycle_suspend;
    impl->lifecycle.resume = sdio_lifecycle_resume;
    impl->lifecycle.get_state = sdio_lifecycle_get_state;

    /* Initialize power interface */
    impl->power.enable = sdio_power_enable;
    impl->power.disable = sdio_power_disable;
    impl->power.is_enabled = sdio_power_is_enabled;
    impl->power.set_callback = sdio_power_set_callback;

    /* Initialize SDIO interface */
    impl->base.read = sdio_read;
    impl->base.write = sdio_write;
    impl->base.erase = sdio_erase;
    impl->base.get_block_size = sdio_get_block_size;
    impl->base.get_capacity = sdio_get_capacity;
    impl->base.is_present = sdio_is_present;
    impl->base.get_lifecycle = sdio_get_lifecycle;
    impl->base.get_power = sdio_get_power;

    /* Link state and device */
    impl->state = state;
    impl->device = (nx_device_t*)dev;

    /* Device is created but not initialized - tests will call init() */
    return &impl->base;
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Interface Implementation                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize SDIO
 */
static nx_status_t sdio_lifecycle_init(nx_lifecycle_t* self) {
    if (!self) {
        return NX_ERR_NULL_PTR;
    }

    nx_sdio_impl_t* impl =
        (nx_sdio_impl_t*)((char*)self - offsetof(nx_sdio_impl_t, lifecycle));
    nx_sdio_state_t* state = impl->state;

    /* Check if already initialized */
    if (state->initialized) {
        return NX_ERR_ALREADY_INIT;
    }

    /* Check if card is present before initializing */
    if (!state->card_present) {
        return NX_ERR_INVALID_STATE;
    }

    /* Initialize card */
    nx_status_t status = sdio_init_card(state);
    if (status != NX_OK) {
        return status;
    }

    state->initialized = true;
    state->suspended = false;
    return NX_OK;
}

/**
 * \brief           Deinitialize SDIO
 */
static nx_status_t sdio_lifecycle_deinit(nx_lifecycle_t* self) {
    if (!self) {
        return NX_ERR_NULL_PTR;
    }

    nx_sdio_impl_t* impl =
        (nx_sdio_impl_t*)((char*)self - offsetof(nx_sdio_impl_t, lifecycle));
    nx_sdio_state_t* state = impl->state;

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    state->initialized = false;
    state->suspended = false;
    return NX_OK;
}

/**
 * \brief           Suspend SDIO
 */
static nx_status_t sdio_lifecycle_suspend(nx_lifecycle_t* self) {
    if (!self) {
        return NX_ERR_NULL_PTR;
    }

    nx_sdio_impl_t* impl =
        (nx_sdio_impl_t*)((char*)self - offsetof(nx_sdio_impl_t, lifecycle));
    nx_sdio_state_t* state = impl->state;

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    state->suspended = true;
    return NX_OK;
}

/**
 * \brief           Resume SDIO
 */
static nx_status_t sdio_lifecycle_resume(nx_lifecycle_t* self) {
    if (!self) {
        return NX_ERR_NULL_PTR;
    }

    nx_sdio_impl_t* impl =
        (nx_sdio_impl_t*)((char*)self - offsetof(nx_sdio_impl_t, lifecycle));
    nx_sdio_state_t* state = impl->state;

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (!state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    state->suspended = false;
    return NX_OK;
}

/**
 * \brief           Get SDIO state
 */
static nx_device_state_t sdio_lifecycle_get_state(nx_lifecycle_t* self) {
    if (!self) {
        return NX_DEV_STATE_UNINITIALIZED;
    }

    nx_sdio_impl_t* impl =
        (nx_sdio_impl_t*)((char*)self - offsetof(nx_sdio_impl_t, lifecycle));
    nx_sdio_state_t* state = impl->state;

    if (!state->initialized) {
        return NX_DEV_STATE_UNINITIALIZED;
    }

    if (state->suspended) {
        return NX_DEV_STATE_SUSPENDED;
    }

    return NX_DEV_STATE_RUNNING;
}

/*---------------------------------------------------------------------------*/
/* Power Interface Implementation                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enable SDIO power
 */
static nx_status_t sdio_power_enable(nx_power_t* self) {
    if (!self) {
        return NX_ERR_NULL_PTR;
    }
    /* Power is always enabled in simulation */
    return NX_OK;
}

/**
 * \brief           Disable SDIO power
 */
static nx_status_t sdio_power_disable(nx_power_t* self) {
    if (!self) {
        return NX_ERR_NULL_PTR;
    }
    /* Power is always enabled in simulation */
    return NX_OK;
}

/**
 * \brief           Check if SDIO power is enabled
 */
static bool sdio_power_is_enabled(nx_power_t* self) {
    (void)self;
    /* Power is always enabled in simulation */
    return true;
}

/**
 * \brief           Set power callback
 */
static nx_status_t sdio_power_set_callback(nx_power_t* self,
                                           nx_power_callback_t callback,
                                           void* user_data) {
    (void)self;
    (void)callback;
    (void)user_data;
    /* Callbacks not supported in simulation */
    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* SDIO Interface Implementation                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Read blocks
 */
static nx_status_t sdio_read(nx_sdio_t* self, uint32_t block, uint8_t* data,
                             size_t block_count) {
    nx_sdio_impl_t* impl = sdio_get_impl(self);
    if (!impl) {
        return NX_ERR_NULL_PTR;
    }
    return sdio_read_blocks(impl->state, block, data, block_count);
}

/**
 * \brief           Write blocks
 */
static nx_status_t sdio_write(nx_sdio_t* self, uint32_t block,
                              const uint8_t* data, size_t block_count) {
    nx_sdio_impl_t* impl = sdio_get_impl(self);
    if (!impl) {
        return NX_ERR_NULL_PTR;
    }
    return sdio_write_blocks(impl->state, block, data, block_count);
}

/**
 * \brief           Erase blocks
 */
static nx_status_t sdio_erase(nx_sdio_t* self, uint32_t start_block,
                              size_t block_count) {
    nx_sdio_impl_t* impl = sdio_get_impl(self);
    if (!impl) {
        return NX_ERR_NULL_PTR;
    }
    return sdio_erase_blocks(impl->state, start_block, block_count);
}

/**
 * \brief           Get block size
 */
static size_t sdio_get_block_size(nx_sdio_t* self) {
    (void)self;
    return NX_SDIO_BLOCK_SIZE;
}

/**
 * \brief           Get capacity
 */
static uint64_t sdio_get_capacity(nx_sdio_t* self) {
    (void)self;
    return (uint64_t)NX_SDIO_BLOCK_SIZE * NX_SDIO_NUM_BLOCKS;
}

/**
 * \brief           Check if card is present
 */
static bool sdio_is_present(nx_sdio_t* self) {
    nx_sdio_impl_t* impl = sdio_get_impl(self);
    if (!impl) {
        return false;
    }
    return sdio_is_card_present(impl->state);
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* sdio_get_lifecycle(nx_sdio_t* self) {
    nx_sdio_impl_t* impl = sdio_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* sdio_get_power(nx_sdio_t* self) {
    nx_sdio_impl_t* impl = sdio_get_impl(self);
    return impl ? &impl->power : NULL;
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Configuration macro - reads from Kconfig
 */
#define NX_SDIO_CONFIG(index)                                                  \
    static const nx_sdio_platform_config_t sdio_config_##index = {             \
        .sdio_index = index,                                                   \
        .bus_width = NX_CONFIG_SDIO##index##_BUS_WIDTH_VALUE,                  \
        .clock_speed = NX_CONFIG_SDIO##index##_CLOCK_SPEED,                    \
        .block_size = NX_CONFIG_SDIO##index##_BLOCK_SIZE,                      \
        .num_blocks = NX_CONFIG_SDIO##index##_NUM_BLOCKS,                      \
        .card_present = NX_CONFIG_SDIO##index##_CARD_PRESENT,                  \
    }

/**
 * \brief           Device registration macro
 */
#define NX_SDIO_DEVICE_REGISTER(index)                                         \
    NX_SDIO_CONFIG(index);                                                     \
    static nx_device_config_state_t sdio_kconfig_state_##index = {             \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, index, "SDIO" #index,                      \
                       &sdio_config_##index, &sdio_kconfig_state_##index,      \
                       nx_sdio_device_init);

/**
 * \brief           Register all enabled SDIO instances
 */
NX_TRAVERSE_EACH_INSTANCE(NX_SDIO_DEVICE_REGISTER, DEVICE_TYPE)
