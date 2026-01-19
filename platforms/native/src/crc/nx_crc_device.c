/**
 * \file            nx_crc_device.c
 * \brief           CRC device registration for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements CRC device registration using Kconfig-driven
 *                  configuration. Provides factory functions for test access
 *                  and manages CRC instance lifecycle.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_crc.h"
#include "nexus_config.h"
#include "nx_crc_helpers.h"
#include "nx_crc_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define NX_CRC_MAX_INSTANCES 4
#define DEVICE_TYPE          NX_CRC

/*---------------------------------------------------------------------------*/
/* Static Storage                                                            */
/*---------------------------------------------------------------------------*/

static nx_crc_state_t g_crc_states[NX_CRC_MAX_INSTANCES];
static nx_crc_impl_t g_crc_instances[NX_CRC_MAX_INSTANCES];
static uint8_t g_crc_instance_count = 0;

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Interface implementations (defined in separate files) */
extern void crc_init_interface(nx_crc_t* crc);
extern void crc_init_lifecycle(nx_lifecycle_t* lifecycle);
extern void crc_init_power(nx_power_t* power);

/*---------------------------------------------------------------------------*/
/* Instance Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize CRC instance with platform configuration
 */
static void crc_init_instance(nx_crc_impl_t* impl, uint8_t index,
                              const nx_crc_platform_config_t* platform_cfg) {
    /* Initialize interfaces (implemented in separate files) */
    crc_init_interface(&impl->base);
    crc_init_lifecycle(&impl->lifecycle);
    crc_init_power(&impl->power);

    /* Link to state */
    impl->state = &g_crc_states[index];
    impl->state->index = index;
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->current_crc = 0;

    /* Set configuration from Kconfig */
    if (platform_cfg != NULL) {
        impl->state->config.algorithm = platform_cfg->algorithm;
        impl->state->config.polynomial = platform_cfg->polynomial;
        impl->state->config.init_value = platform_cfg->init_value;
        impl->state->config.final_xor = platform_cfg->final_xor;
        impl->state->current_crc = platform_cfg->init_value;
    }

    /* Clear statistics */
    memset(&impl->state->stats, 0, sizeof(nx_crc_stats_t));
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
static void* nx_crc_device_init(const nx_device_t* dev) {
    const nx_crc_platform_config_t* config =
        (const nx_crc_platform_config_t*)dev->config;

    if (config == NULL || g_crc_instance_count >= NX_CRC_MAX_INSTANCES) {
        return NULL;
    }

    uint8_t index = g_crc_instance_count++;
    nx_crc_impl_t* impl = &g_crc_instances[index];

    /* Initialize instance with platform configuration */
    crc_init_instance(impl, index, config);

    /* Store device pointer */
    impl->device = (nx_device_t*)dev;

    /* Initialize lifecycle */
    nx_status_t status = impl->lifecycle.init(&impl->lifecycle);
    if (status != NX_OK) {
        return NULL;
    }

    return &impl->base;
}

/* Register all enabled CRC instances */
/* TODO: Fix MSVC compilation issue with NX_DEVICE_REGISTER macro */
/*
#if defined(NX_CONFIG_INSTANCE_NX_CRC0)
static const nx_crc_platform_config_t crc_config_0 = {
    .index = 0,
    .algorithm =
        NX_CONFIG_CRC0_ALGORITHM_CRC32 ? NX_CRC_ALGO_CRC32 : NX_CRC_ALGO_CRC16,
    .polynomial = NX_CONFIG_CRC0_POLYNOMIAL,
    .init_value = NX_CONFIG_CRC0_INIT_VALUE,
    .final_xor = NX_CONFIG_CRC0_FINAL_XOR,
};
static nx_device_config_state_t crc_kconfig_state_0 = {
    .init_res = 0,
    .initialized = false,
};
NX_DEVICE_REGISTER(DEVICE_TYPE, 0, "CRC0", &crc_config_0, &crc_kconfig_state_0,
                   nx_crc_device_init);
#endif
*/
