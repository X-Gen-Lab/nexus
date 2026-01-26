/**
 * \file            nx_hal.c
 * \brief           Nexus HAL initialization and deinitialization
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-26
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This module provides HAL-level initialization and cleanup.
 *                  Device instances are created dynamically via factory
 * functions (nx_factory_*) rather than through a static registry.
 */

#include "hal/nx_hal.h"
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
 * \brief           Platform-specific initialization (weak symbol)
 * \return          NX_OK on success, error code otherwise
 *
 * \details         This function should be implemented by each platform to
 *                  perform platform-specific initialization such as:
 *                  - System clock configuration
 *                  - Power management setup
 *                  - Platform-specific peripheral initialization
 *
 * \note            This is a weak symbol. If not overridden by platform code,
 *                  the default implementation returns NX_OK.
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
 * \brief           Platform-specific deinitialization (weak symbol)
 * \return          NX_OK on success, error code otherwise
 *
 * \details         This function should be implemented by each platform to
 *                  perform platform-specific cleanup such as:
 *                  - Disabling peripheral clocks
 *                  - Releasing platform resources
 *                  - Power management cleanup
 *
 * \note            This is a weak symbol. If not overridden by platform code,
 *                  the default implementation returns NX_OK.
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

/**
 * \brief           Initialize the Nexus HAL
 *
 * \details         Performs HAL-level initialization including
 * platform-specific hardware setup. Device instances are created dynamically
 * via factory functions (nx_factory_*) when needed.
 *
 * \note            This function is idempotent - multiple calls have no effect
 *                  after the first successful initialization.
 */
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

    /* Mark as initialized */
    hal_initialized = true;

    return NX_OK;
}

/**
 * \brief           Deinitialize the Nexus HAL
 *
 * \details         Performs HAL-level cleanup including platform-specific
 *                  hardware deinitialization. Active device instances should
 *                  be released via factory release functions before calling
 *                  this function.
 *
 * \note            This function is idempotent - multiple calls have no effect
 *                  if HAL is not initialized.
 */
nx_status_t nx_hal_deinit(void) {
    nx_status_t status;

    /* Check if not initialized */
    if (!hal_initialized) {
        return NX_OK;
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
