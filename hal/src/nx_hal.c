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
 *                  The selected platform registers effective-config device
 * descriptors; factories initialize and cache their APIs on first use.
 */

#include "hal/nx_hal.h"
#include "arch/nx_arch.h"
#include "hal/provider/nx_device_provider.h"
#include "osal/osal.h"
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

static nx_hal_state_t hal_state = NX_HAL_OFFLINE;
static nx_status_t last_cleanup_status = NX_OK;
static bool shutdown_fence_owned;
static const unsigned char shutdown_owner;
nx_status_t nx_platform_deinit(void);
nx_status_t nx_platform_shutdown_check(void);
nx_status_t nx_platform_init_check(void);

/* Only this entry owns the registry admission fence. Platform hooks are
 * private, externally serialized implementation calls, never application APIs.
 * The fence spans every platform side effect. PARTIAL retains it until a
 * successful cleanup; existing owners may still close/recover for settlement. */
static nx_status_t cleanup_platform(bool* attempted) {
    *attempted = false;
    nx_status_t status;
    if (!shutdown_fence_owned) {
        uintptr_t owner = (uintptr_t)&shutdown_owner;
        status = hal_state == NX_HAL_PARTIAL ?
            nx_device_shutdown_quarantine_begin(owner) : nx_device_shutdown_begin_owned(owner);
        if (status != NX_OK) return status;
        shutdown_fence_owned = true;
    }
    status = nx_device_shutdown_check();
    if (status == NX_OK) status = nx_platform_shutdown_check();
    if (status == NX_OK) {
        *attempted = true;
        status = nx_platform_deinit();
    }
    return status;
}

static nx_status_t release_shutdown_fence(void) {
    if (shutdown_fence_owned) {
        nx_status_t status = nx_device_shutdown_end_owned((uintptr_t)&shutdown_owner);
        if (status != NX_OK) return status;
        shutdown_fence_owned = false;
    }
    return NX_OK;
}

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
 *                  the default implementation reports unsupported.
 */
#if defined(_MSC_VER)
/* MSVC weak symbol support */
#pragma comment(linker,                                                        \
                "/alternatename:nx_platform_init=nx_platform_init_default")
nx_status_t nx_platform_init_default(void) {
    return NX_ERR_NOT_SUPPORTED;
}
nx_status_t nx_platform_init(void);
#elif defined(__GNUC__) || defined(__clang__)
/* GCC/Clang weak symbol support */
__attribute__((weak)) nx_status_t nx_platform_init(void) {
    return NX_ERR_NOT_SUPPORTED;
}
#else
/* Fallback for other compilers */
nx_status_t nx_platform_init(void) {
    return NX_ERR_NOT_SUPPORTED;
}
#endif

/* Read-only startup admission before acquiring any platform hardware. */
#if defined(_MSC_VER)
#pragma comment(linker, "/alternatename:nx_platform_init_check=nx_platform_init_check_default")
nx_status_t nx_platform_init_check_default(void) { return NX_OK; }
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((weak)) nx_status_t nx_platform_init_check(void) { return NX_OK; }
#else
nx_status_t nx_platform_init_check(void) { return NX_OK; }
#endif

/* Optional read-only selected-platform admission check. An implementation
 * rejects hardware/manager ownership before any cleanup side effect. Missing
 * checks do not certify resources: the mutating hook still has to settle them. */
#if defined(_MSC_VER)
#pragma comment(linker, "/alternatename:nx_platform_shutdown_check=nx_platform_shutdown_check_default")
nx_status_t nx_platform_shutdown_check_default(void) { return NX_OK; }
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((weak)) nx_status_t nx_platform_shutdown_check(void) { return NX_OK; }
#else
nx_status_t nx_platform_shutdown_check(void) { return NX_OK; }
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
 *                  the default implementation reports unsupported.
 */
#if defined(_MSC_VER)
/* MSVC weak symbol support */
#pragma comment(                                                               \
    linker, "/alternatename:nx_platform_deinit=nx_platform_deinit_default")
nx_status_t nx_platform_deinit_default(void) {
    return NX_ERR_NOT_SUPPORTED;
}
nx_status_t nx_platform_deinit(void);
#elif defined(__GNUC__) || defined(__clang__)
/* GCC/Clang weak symbol support */
__attribute__((weak)) nx_status_t nx_platform_deinit(void) {
    return NX_ERR_NOT_SUPPORTED;
}
#else
/* Fallback for other compilers */
nx_status_t nx_platform_deinit(void) {
    return NX_ERR_NOT_SUPPORTED;
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

    /* Platform clock/failure deadlines require interrupt progress. */
    if (nx_arch_in_isr()) {
        return NX_ERR_CONTEXT;
    }
    if (nx_arch_irq_is_masked()) {
        return NX_ERR_INVALID_STATE;
    }

    /* Check if already initialized */
    if (hal_state == NX_HAL_READY) {
        return NX_OK;
    }
    if (hal_state == NX_HAL_PARTIAL) {
        return NX_ERR_INVALID_STATE;
    }
    last_cleanup_status = NX_OK;
    if (osal_is_initialized()) {
        return NX_ERR_BUSY;
    }
    status = nx_device_shutdown_check();
    if (status == NX_OK) status = nx_platform_init_check();
    if (status != NX_OK) return status;

    /* Initialize platform-specific hardware */
    status = nx_platform_init();
    if (status != NX_OK) {
        bool attempted;
        hal_state = NX_HAL_PARTIAL;
        last_cleanup_status = cleanup_platform(&attempted);
        if (last_cleanup_status == NX_OK) last_cleanup_status = release_shutdown_fence();
        hal_state = last_cleanup_status == NX_OK ? NX_HAL_OFFLINE : NX_HAL_PARTIAL;
        return status;
    }

    /* Mark as initialized */
    hal_state = NX_HAL_READY;
    last_cleanup_status = NX_OK;

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

    if (nx_arch_in_isr()) {
        return NX_ERR_CONTEXT;
    }
    if (nx_arch_irq_is_masked()) {
        return NX_ERR_INVALID_STATE;
    }

    /* Check if not initialized */
    if (hal_state == NX_HAL_OFFLINE) {
        return NX_OK;
    }
    /* Runtime releases OSAL first. Direct HAL callers may not stop a clock
     * underneath an initialized backend, even when its object pools are idle. */
    if (osal_is_initialized()) {
        return NX_ERR_BUSY;
    }

    /* Deinitialize platform-specific hardware */
    bool attempted;
    status = cleanup_platform(&attempted);
    last_cleanup_status = status;
    if (status != NX_OK) {
        /* Admission is read-only. Once the cleanup hook was entered, any
         * failure can follow partial hardware changes, irrespective of code. */
        if (attempted) {
            hal_state = NX_HAL_PARTIAL;
        }
        if (hal_state == NX_HAL_READY) {
            nx_status_t release_status = release_shutdown_fence();
            if (release_status != NX_OK) {
                hal_state = NX_HAL_PARTIAL;
                last_cleanup_status = release_status;
                return release_status;
            }
        }
        return status;
    }

    /* Mark as not initialized */
    status = release_shutdown_fence();
    last_cleanup_status = status;
    hal_state = status == NX_OK ? NX_HAL_OFFLINE : NX_HAL_PARTIAL;

    return status;
}

bool nx_hal_is_initialized(void) {
    return hal_state == NX_HAL_READY;
}

nx_hal_state_t nx_hal_get_state(void) {
    return hal_state;
}

nx_status_t nx_hal_get_last_cleanup_status(void) {
    return last_cleanup_status;
}

const char* nx_hal_get_version(void) {
    return NX_HAL_VERSION_STRING;
}
