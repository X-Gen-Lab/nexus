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
#include "hal/system/nx_mem.h"
#include "nexus_config.h"
#include "nx_option_bytes_helpers.h"
#include "nx_option_bytes_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE NX_OPTION_BYTES

/*---------------------------------------------------------------------------*/
/* Platform Configuration Structure                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Option Bytes platform configuration structure
 *
 * Contains compile-time configuration from Kconfig.
 */
typedef struct {
    uint8_t option_bytes_index;     /**< Option bytes instance index */
    uint8_t rdp_level;              /**< Read protection level */
    uint8_t wdg_selection;          /**< Watchdog selection (SW/HW) */
    uint8_t stop_rst;               /**< Reset on stop mode */
    uint8_t stdby_rst;              /**< Reset on standby mode */
    uint32_t write_protect_sectors; /**< Write protection sector mask */
    uint8_t user_data0;             /**< User data byte 0 */
    uint8_t user_data1;             /**< User data byte 1 */
} nx_option_bytes_platform_config_t;

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
static void option_bytes_init_instance(
    nx_option_bytes_impl_t* impl, uint8_t index,
    const nx_option_bytes_platform_config_t* platform_cfg) {
    /* Initialize interfaces (implemented in separate files) */
    option_bytes_init_interface(&impl->base);
    option_bytes_init_lifecycle(&impl->lifecycle);
    option_bytes_init_power(&impl->power);

    /* Allocate and initialize state */
    impl->state =
        (nx_option_bytes_state_t*)nx_mem_alloc(sizeof(nx_option_bytes_state_t));
    if (!impl->state) {
        return;
    }
    memset(impl->state, 0, sizeof(nx_option_bytes_state_t));

    impl->state->index = index;
    impl->state->initialized = false;
    impl->state->suspended = false;

    /* Initialize option bytes data with Kconfig defaults */
    if (platform_cfg != NULL) {
        impl->state->data.read_protection = platform_cfg->rdp_level;
        impl->state->data.user_data[0] = platform_cfg->user_data0;
        impl->state->data.user_data[1] = platform_cfg->user_data1;
        /* Initialize remaining user data bytes to 0xFF */
        for (int i = 2; i < NX_OPTION_BYTES_USER_DATA_SIZE; i++) {
            impl->state->data.user_data[i] = 0xFF;
        }
    } else {
        /* Default values if no config provided */
        impl->state->data.read_protection = 0; /* No protection by default */
        for (int i = 0; i < NX_OPTION_BYTES_USER_DATA_SIZE; i++) {
            impl->state->data.user_data[i] = 0xFF;
        }
    }

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
    const nx_option_bytes_platform_config_t* config =
        (const nx_option_bytes_platform_config_t*)dev->config;

    /* Allocate implementation structure */
    nx_option_bytes_impl_t* impl =
        (nx_option_bytes_impl_t*)nx_mem_alloc(sizeof(nx_option_bytes_impl_t));
    if (!impl) {
        return NULL;
    }
    memset(impl, 0, sizeof(nx_option_bytes_impl_t));

    /* Initialize instance with platform configuration */
    uint8_t index = config ? config->option_bytes_index : 0;
    option_bytes_init_instance(impl, index, config);

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
 * \brief           Configuration macro - reads from Kconfig
 */
#define NX_OPTION_BYTES_CONFIG(index)                                          \
    static const nx_option_bytes_platform_config_t                             \
        option_bytes_config_##index = {                                        \
            .option_bytes_index = index,                                       \
            .rdp_level = NX_CONFIG_OPTBYTES##index##_RDP_LEVEL_VALUE,          \
            .wdg_selection = NX_CONFIG_OPTBYTES##index##_WDG_VALUE,            \
            .stop_rst = NX_CONFIG_OPTBYTES##index##_STOP_RST_VALUE,            \
            .stdby_rst = NX_CONFIG_OPTBYTES##index##_STDBY_RST_VALUE,          \
            .write_protect_sectors =                                           \
                NX_CONFIG_OPTBYTES##index##_WRITE_PROTECT_SECTORS,             \
            .user_data0 = NX_CONFIG_OPTBYTES##index##_USER_DATA0,              \
            .user_data1 = NX_CONFIG_OPTBYTES##index##_USER_DATA1,              \
    }

/**
 * \brief           Device registration macro
 */
#define NX_OPTION_BYTES_DEVICE_REGISTER(index)                                 \
    NX_OPTION_BYTES_CONFIG(index);                                             \
    static nx_device_config_state_t option_bytes_kconfig_state_##index = {     \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(                                                        \
        DEVICE_TYPE, index, "OPTBYTES" #index, &option_bytes_config_##index,   \
        &option_bytes_kconfig_state_##index, nx_option_bytes_device_init);

/**
 * \brief           Register all enabled Option Bytes instances
 */
NX_TRAVERSE_EACH_INSTANCE(NX_OPTION_BYTES_DEVICE_REGISTER, DEVICE_TYPE);
