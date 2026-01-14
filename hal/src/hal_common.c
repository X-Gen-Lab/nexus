/**
 * \file            hal_common.c
 * \brief           HAL Common Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "hal/hal.h"

/**
 * \brief           HAL initialization flag
 */
static bool s_hal_initialized = false;

/**
 * \brief           Initialize HAL layer
 */
hal_status_t hal_init(void) {
    if (s_hal_initialized) {
        return HAL_ERROR_ALREADY_INIT;
    }

    /* Platform-specific initialization is done in platform code */

    s_hal_initialized = true;
    return HAL_OK;
}

/**
 * \brief           Deinitialize HAL layer
 */
hal_status_t hal_deinit(void) {
    if (!s_hal_initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    s_hal_initialized = false;
    return HAL_OK;
}

/*
 * Note: The following functions are implemented in platform-specific code.
 * For native platform, see platforms/native/src/hal_system_native.c
 * For STM32F4, see platforms/stm32f4/src/hal_system_stm32f4.c
 */
