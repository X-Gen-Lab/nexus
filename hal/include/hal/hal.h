/**
 * \file            hal.h
 * \brief           HAL Main Header
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This is the main header file for the Hardware Abstraction
 *                  Layer (HAL). Include this file to access all HAL modules.
 */

#ifndef HAL_H
#define HAL_H

/* Common definitions */
#include "hal_def.h"

/* HAL modules */
#include "hal_adc.h"
#include "hal_gpio.h"
#include "hal_i2c.h"
#include "hal_spi.h"
#include "hal_timer.h"
#include "hal_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        HAL Hardware Abstraction Layer
 * \brief           Hardware abstraction layer for Nexus platform
 * \{
 */

/**
 * \brief           Initialize HAL layer
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_init(void);

/**
 * \brief           Deinitialize HAL layer
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_deinit(void);

/**
 * \brief           Delay in milliseconds
 * \param[in]       ms: Delay time in milliseconds
 */
void hal_delay_ms(uint32_t ms);

/**
 * \brief           Delay in microseconds
 * \param[in]       us: Delay time in microseconds
 */
void hal_delay_us(uint32_t us);

/**
 * \brief           Get system tick count in milliseconds
 * \return          System tick count
 */
uint32_t hal_get_tick(void);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* HAL_H */
