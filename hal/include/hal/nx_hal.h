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
 *     // Device actions use an explicit owner and a typed reference.
 *     nx_device_ref_t led = {0};
 *     if (nx_device_open("GPIOA0", NX_DEVICE_CLASS_GPIO, 1, &led) == NX_OK) {
 *         nx_device_gpio_write(led, 1);
 *         nx_device_close(led);
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

#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"

/*---------------------------------------------------------------------------*/
/* Peripheral Interfaces                                                     */
/*---------------------------------------------------------------------------*/

#include "hal/interface/nx_adc.h"
#include "hal/interface/nx_can.h"
#include "hal/interface/nx_flash.h"
#include "hal/interface/nx_gpio.h"
#include "hal/interface/nx_i2c.h"
#include "hal/interface/nx_spi.h"
#include "hal/interface/nx_timer.h"
#include "hal/interface/nx_uart.h"
#include "hal/interface/nx_usb.h"

/*---------------------------------------------------------------------------*/
/* Resource Managers                                                         */
/*---------------------------------------------------------------------------*/

#include "hal/resource/nx_dma_manager.h"
#include "hal/resource/nx_isr_manager.h"

/*---------------------------------------------------------------------------*/
/* Factory Interface                                                         */
/*---------------------------------------------------------------------------*/

/* Explicit provider/migration factories are not part of this consumer umbrella. */

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
 * The selected platform initializes its hardware and effective-config resources.
 *
 * \note            This function is idempotent - calling it multiple times
 *                  has no additional effect after the first successful call.
 * \note            Externally serialized task/startup context with interrupts
 *                  unmasked. ISR calls return NX_ERR_CONTEXT; an existing Arch
 *                  interrupt mask returns NX_ERR_INVALID_STATE before any
 *                  platform hook, including repeated calls. The caller retains
 *                  and restores its own mask. The application serializes HAL
 *                  lifetime against device users. An unbound platform is unsupported.
 */
nx_status_t nx_hal_init(void);

typedef enum {
    NX_HAL_OFFLINE, /**< No selected-platform resources are owned. */
    NX_HAL_PARTIAL, /**< Cleanup failed; new acquisition/restart is quarantined.
                     */
    NX_HAL_READY    /**< The selected platform completed initialization. */
} nx_hal_state_t;

/** Nonblocking lifetime snapshot; use the serialized HAL lifecycle context.
 * Failed init attempts actual cleanup and returns its original error. PARTIAL
 * retains ownership and the exclusive admission fence until nx_hal_deinit()
 * settles it. Existing owners may close/recover to return borrowed resources.
 */
nx_hal_state_t nx_hal_get_state(void);

/** Cleanup result from the latest failed-init rollback or deinit attempt.
 * This is separate from the original initialization error. */
nx_status_t nx_hal_get_last_cleanup_status(void);

/**
 * \brief           Deinitialize the Nexus HAL
 * \return          NX_OK on success, error code otherwise
 *
 * This function requests selected-platform hardware cleanup at system shutdown.
 * The caller must first quiesce device users and settle all outstanding leases;
 * this entry does not automatically release device handles or application objects.
 *
 * \warning         After successful cleanup, initialize HAL before device use.
 *                  PARTIAL permits settlement/cleanup retry, not ordinary use.
 * \note            Externally serialized task/startup context with interrupts
 *                  unmasked. ISR calls return NX_ERR_CONTEXT; an existing Arch
 *                  interrupt mask returns NX_ERR_INVALID_STATE before any
 *                  platform hook, including offline calls. The caller retains
 *                  and restores its own mask. An unsupported or busy platform
 *                  retains ownership. Read-only admission failure keeps READY;
 *                  any failure after entering cleanup quarantines PARTIAL.
 *                  MCU teardown is limited to baremetal or
 *                  before scheduler start, after all Nexus devices, IRQ/DMA
 *                  resources and OSAL objects are released. Direct vendor SDK
 *                  users must quiesce their resources separately. The caller
 *                  also releases OSAL before stopping the platform time source.
 */
nx_status_t nx_hal_deinit(void);

/**
 * \brief           Check if HAL is initialized
 * \return          true only when initialization completed (READY)
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
