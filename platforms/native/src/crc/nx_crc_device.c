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
#include "hal/system/nx_mem.h"
#include "nexus_config.h"
#include "nx_crc_helpers.h"
#include "nx_crc_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE NX_CRC

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

    /* Allocate and initialize state */
    impl->state = (nx_crc_state_t*)nx_mem_alloc(sizeof(nx_crc_state_t));
    if (!impl->state) {
        return;
    }
    memset(impl->state, 0, sizeof(nx_crc_state_t));

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

    if (config == NULL) {
        return NULL;
    }

    /* Allocate implementation structure */
    nx_crc_impl_t* impl = (nx_crc_impl_t*)nx_mem_alloc(sizeof(nx_crc_impl_t));
    if (!impl) {
        return NULL;
    }
    memset(impl, 0, sizeof(nx_crc_impl_t));

    /* Initialize instance with platform configuration */
    crc_init_instance(impl, config->crc_index, config);

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
#define NX_CRC_CONFIG(index)                                                   \
    static const nx_crc_platform_config_t crc_config_##index = {               \
        .crc_index = index,                                                    \
        .algorithm = NX_CONFIG_NX_CRC##index##_ALGORITHM_VALUE == 0            \
                         ? NX_CRC_ALGO_CRC32                                   \
                         : (NX_CONFIG_NX_CRC##index##_ALGORITHM_VALUE == 1     \
                                ? NX_CRC_ALGO_CRC16                            \
                                : NX_CRC_ALGO_CRC8),                           \
        .polynomial = NX_CONFIG_NX_CRC##index##_POLYNOMIAL,                    \
        .init_value = NX_CONFIG_NX_CRC##index##_INIT_VALUE,                    \
        .final_xor = NX_CONFIG_NX_CRC##index##_FINAL_XOR,                      \
    }

/**
 * \brief           Device registration macro
 */
#define NX_CRC_DEVICE_REGISTER(index)                                          \
    NX_CRC_CONFIG(index);                                                      \
    static nx_device_config_state_t crc_kconfig_state_##index = {              \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, index, "CRC" #index, &crc_config_##index,  \
                       &crc_kconfig_state_##index, nx_crc_device_init);

/**
 * \brief           Register all enabled CRC instances
 */
NX_TRAVERSE_EACH_INSTANCE(NX_CRC_DEVICE_REGISTER, DEVICE_TYPE);
