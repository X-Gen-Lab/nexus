/**
 * \file            nx_hal.h
 * \brief           Nexus HAL main header file - includes all public interfaces
 * \author          Nexus Team
 *
 * This is the main entry point for the Nexus Hardware Abstraction Layer.
 * Include this file to access all HAL functionality.
 *
 * Usage:
 * \code{.c}
 * #include "hal/nx_hal.h"
 *
 * int main(void) {
 *     // Initialize HAL
 *     nx_hal_init();
 *
 *     // Get UART device
 *     nx_uart_t *uart = nx_factory_uart(0);
 *     if (uart) {
 *         // Use UART...
 *         nx_factory_uart_release(uart);
 *     }
 *
 *     // Cleanup
 *     nx_hal_deinit();
 *     return 0;
 * }
 * \endcode
 */

#ifndef NX_HAL_H
#define NX_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Base Types and Status Codes                                               */
/*---------------------------------------------------------------------------*/

#include "hal/nx_status.h"
#include "hal/nx_types.h"

/*---------------------------------------------------------------------------*/
/* Device Base Class                                                         */
/*---------------------------------------------------------------------------*/

#include "hal/base/nx_device.h"

/*---------------------------------------------------------------------------*/
/* Base Interfaces                                                           */
/*---------------------------------------------------------------------------*/

#include "hal/interface/nx_configurable.h"
#include "hal/interface/nx_diagnostic.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"

/*---------------------------------------------------------------------------*/
/* Peripheral Interfaces                                                     */
/*---------------------------------------------------------------------------*/

#include "hal/interface/nx_adc.h"
#include "hal/interface/nx_gpio.h"
#include "hal/interface/nx_i2c.h"
#include "hal/interface/nx_spi.h"
#include "hal/interface/nx_timer.h"
#include "hal/interface/nx_uart.h"

/*---------------------------------------------------------------------------*/
/* Resource Managers                                                         */
/*---------------------------------------------------------------------------*/

#include "hal/resource/nx_dma_manager.h"
#include "hal/resource/nx_isr_manager.h"

/*---------------------------------------------------------------------------*/
/* Factory Interface                                                         */
/*---------------------------------------------------------------------------*/

#include "hal/nx_factory.h"

/*---------------------------------------------------------------------------*/
/* Static Device Registry                                                    */
/*---------------------------------------------------------------------------*/

#include "hal/nx_device_registry.h"

/*---------------------------------------------------------------------------*/
/* HAL Initialization and Deinitialization                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize the Nexus HAL
 * \return          NX_OK on success, error code otherwise
 *
 * This function initializes the HAL subsystem. It should be called once
 * at system startup before using any HAL functionality.
 *
 * The function performs the following:
 * - Initializes platform-specific hardware
 * - Sets up resource managers (DMA, ISR)
 * - Prepares device registry
 *
 * \note            This function is idempotent - calling it multiple times
 *                  has no additional effect after the first successful call.
 */
nx_status_t nx_hal_init(void);

/**
 * \brief           Deinitialize the Nexus HAL
 * \return          NX_OK on success, error code otherwise
 *
 * This function deinitializes the HAL subsystem and releases all resources.
 * It should be called at system shutdown.
 *
 * The function performs the following:
 * - Deinitializes all active devices
 * - Releases resource managers
 * - Cleans up platform-specific hardware
 *
 * \warning         After calling this function, no HAL functions should be
 *                  called until nx_hal_init() is called again.
 */
nx_status_t nx_hal_deinit(void);

/**
 * \brief           Check if HAL is initialized
 * \return          true if initialized, false otherwise
 */
bool nx_hal_is_initialized(void);

/**
 * \brief           Get HAL version string
 * \return          Version string (e.g., "1.0.0")
 */
const char* nx_hal_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NX_HAL_H */
