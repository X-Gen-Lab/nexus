/**
 * \file            nx_hal.c
 * \brief           Nexus HAL initialization and deinitialization
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "hal/nx_hal.h"
#include "hal/nx_device_registry.h"
#include <stdbool.h>

/*---------------------------------------------------------------------------*/
/* Version Information                                                       */
/*---------------------------------------------------------------------------*/

#define NX_HAL_VERSION_MAJOR 1
#define NX_HAL_VERSION_MINOR 0
#define NX_HAL_VERSION_PATCH 0

#define NX_HAL_VERSION_STRING "1.0.0"

/*---------------------------------------------------------------------------*/
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

static bool hal_initialized = false;

/*---------------------------------------------------------------------------*/
/* Platform-Specific Initialization (weak symbols)                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Platform-specific initialization
 * \return          NX_OK on success
 *
 * This function should be implemented by each platform to perform
 * platform-specific initialization (clocks, system config, etc.)
 */
#if defined(_MSC_VER)
/* MSVC weak symbol support */
#pragma comment(linker,                                                        \
                "/alternatename:nx_platform_init=nx_platform_init_default")
nx_status_t nx_platform_init_default(void) {
    return NX_OK;
}
nx_status_t nx_platform_init(void);
#elif defined(__GNUC__) || defined(__clang__)
/* GCC/Clang weak symbol support */
__attribute__((weak)) nx_status_t nx_platform_init(void) {
    return NX_OK;
}
#else
/* Fallback for other compilers */
nx_status_t nx_platform_init(void) {
    return NX_OK;
}
#endif

/**
 * \brief           Platform-specific deinitialization
 * \return          NX_OK on success
 *
 * This function should be implemented by each platform to perform
 * platform-specific cleanup.
 */
#if defined(_MSC_VER)
/* MSVC weak symbol support */
#pragma comment(                                                               \
    linker, "/alternatename:nx_platform_deinit=nx_platform_deinit_default")
nx_status_t nx_platform_deinit_default(void) {
    return NX_OK;
}
nx_status_t nx_platform_deinit(void);
#elif defined(__GNUC__) || defined(__clang__)
/* GCC/Clang weak symbol support */
__attribute__((weak)) nx_status_t nx_platform_deinit(void) {
    return NX_OK;
}
#else
/* Fallback for other compilers */
nx_status_t nx_platform_deinit(void) {
    return NX_OK;
}
#endif

/*---------------------------------------------------------------------------*/
/* Public Functions                                                          */
/*---------------------------------------------------------------------------*/

nx_status_t nx_hal_init(void) {
    nx_status_t status;

    /* Check if already initialized */
    if (hal_initialized) {
        return NX_OK;
    }

    /* Initialize platform-specific hardware */
    status = nx_platform_init();
    if (status != NX_OK) {
        return status;
    }

    /* Initialize all devices in the static registry */
    status = nx_device_registry_init_all();
    if (status != NX_OK) {
        /* Log warning but continue - some devices may have failed */
        /* Individual device errors are tracked in device state */
    }

    /* Mark as initialized */
    hal_initialized = true;

    return NX_OK;
}

nx_status_t nx_hal_deinit(void) {
    nx_status_t status;

    /* Check if not initialized */
    if (!hal_initialized) {
        return NX_OK;
    }

    /* Deinitialize all devices in the static registry */
    status = nx_device_registry_deinit_all();
    if (status != NX_OK) {
        /* Log warning but continue - some devices may have failed */
    }

    /* Deinitialize platform-specific hardware */
    status = nx_platform_deinit();
    if (status != NX_OK) {
        return status;
    }

    /* Mark as not initialized */
    hal_initialized = false;

    return NX_OK;
}

bool nx_hal_is_initialized(void) {
    return hal_initialized;
}

const char* nx_hal_get_version(void) {
    return NX_HAL_VERSION_STRING;
}
