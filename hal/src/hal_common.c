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
hal_status_t hal_init(void)
{
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
hal_status_t hal_deinit(void)
{
    if (!s_hal_initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    s_hal_initialized = false;
    return HAL_OK;
}

/* Weak implementations - to be overridden by platform */

__attribute__((weak))
void hal_delay_ms(uint32_t ms)
{
    /* Simple busy-wait delay - should be overridden */
    volatile uint32_t count = ms * 1000;
    while (count--) {
        __asm volatile("nop");
    }
}

__attribute__((weak))
void hal_delay_us(uint32_t us)
{
    /* Simple busy-wait delay - should be overridden */
    volatile uint32_t count = us;
    while (count--) {
        __asm volatile("nop");
    }
}

__attribute__((weak))
uint32_t hal_get_tick(void)
{
    /* Should be overridden by platform */
    static uint32_t tick = 0;
    return tick++;
}
