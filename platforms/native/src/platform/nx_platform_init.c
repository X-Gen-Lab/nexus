/**
 * \file            nx_platform_init.c
 * \brief           Native Platform Initialization and Deinitialization
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-17
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This file implements platform-specific initialization and
 *                  cleanup for the Native platform. The Native platform is
 *                  used for host-based testing and simulation.
 */

#include "hal/provider/nx_device_provider.h"
#include "hal/nx_status.h"
#include "nexus_config.h"
#include "hal/resource/nx_dma_manager.h"
#include "hal/resource/nx_isr_manager.h"
#include <stdbool.h>
#include "native_platform.h"

/*---------------------------------------------------------------------------*/
/* External Functions                                                        */
/*---------------------------------------------------------------------------*/

/* The common HAL owns admission and serializes this private hook. All cached
 * providers are stopped through production lifecycle methods, including legacy
 * peripherals; shutdown never constructs a device or calls test fixtures. */
/*---------------------------------------------------------------------------*/
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

static bool platform_initialized = false;

/* A host linker does not enumerate nx_device sections. Register every
 * descriptor from this build's effective configuration before factories are
 * exposed. This is production startup, independent of test fixtures. */
#define REGISTER_NATIVE_DEVICE(type, index)                                   \
    do {                                                                     \
        extern const nx_device_t NX_CONCAT(type, index);                      \
        nx_status_t status = nx_device_register(&NX_CONCAT(type, index));     \
        if (status != NX_OK) return status;                                   \
    } while (0);
#define REGISTER_NATIVE_UART(index) REGISTER_NATIVE_DEVICE(NX_UART, index)
#define REGISTER_NATIVE_SPI(index) REGISTER_NATIVE_DEVICE(NX_SPI, index)
#define REGISTER_NATIVE_I2C(index) REGISTER_NATIVE_DEVICE(NX_I2C, index)
#define REGISTER_NATIVE_ADC(index) REGISTER_NATIVE_DEVICE(NX_ADC, index)
#define REGISTER_NATIVE_ADC_BUFFER(index) REGISTER_NATIVE_DEVICE(NX_ADC_BUFFER, index)
#define REGISTER_NATIVE_CRC(index) REGISTER_NATIVE_DEVICE(NX_CRC, index)
#define REGISTER_NATIVE_DAC(index) REGISTER_NATIVE_DEVICE(NX_DAC, index)
#define REGISTER_NATIVE_INTERNAL_FLASH(index) REGISTER_NATIVE_DEVICE(NX_INTERNAL_FLASH, index)
#define REGISTER_NATIVE_RTC(index) REGISTER_NATIVE_DEVICE(NX_RTC, index)
#define REGISTER_NATIVE_TIMER(index) REGISTER_NATIVE_DEVICE(NX_TIMER, index)
#define REGISTER_NATIVE_SDIO(index) REGISTER_NATIVE_DEVICE(NX_SDIO, index)
#define REGISTER_NATIVE_USB(index) REGISTER_NATIVE_DEVICE(NX_USB, index)
#define REGISTER_NATIVE_WATCHDOG(index) REGISTER_NATIVE_DEVICE(NX_WATCHDOG, index)
#define REGISTER_NATIVE_OPTION_BYTES(index) REGISTER_NATIVE_DEVICE(NX_OPTION_BYTES, index)
#define REGISTER_NATIVE_GPIO(port, pin) REGISTER_NATIVE_DEVICE(NX_GPIO, port##pin)

static nx_status_t register_effective_devices(void) {
    NX_TRAVERSE_EACH_INSTANCE(REGISTER_NATIVE_GPIO, NX_GPIO)
    NX_TRAVERSE_EACH_INSTANCE(REGISTER_NATIVE_UART, NX_UART)
    NX_TRAVERSE_EACH_INSTANCE(REGISTER_NATIVE_SPI, NX_SPI)
    NX_TRAVERSE_EACH_INSTANCE(REGISTER_NATIVE_I2C, NX_I2C)
    NX_TRAVERSE_EACH_INSTANCE(REGISTER_NATIVE_ADC, NX_ADC)
    NX_TRAVERSE_EACH_INSTANCE(REGISTER_NATIVE_ADC_BUFFER, NX_ADC_BUFFER)
    NX_TRAVERSE_EACH_INSTANCE(REGISTER_NATIVE_CRC, NX_CRC)
    NX_TRAVERSE_EACH_INSTANCE(REGISTER_NATIVE_DAC, NX_DAC)
    NX_TRAVERSE_EACH_INSTANCE(REGISTER_NATIVE_INTERNAL_FLASH, NX_INTERNAL_FLASH)
    NX_TRAVERSE_EACH_INSTANCE(REGISTER_NATIVE_RTC, NX_RTC)
    NX_TRAVERSE_EACH_INSTANCE(REGISTER_NATIVE_TIMER, NX_TIMER)
    NX_TRAVERSE_EACH_INSTANCE(REGISTER_NATIVE_SDIO, NX_SDIO)
    NX_TRAVERSE_EACH_INSTANCE(REGISTER_NATIVE_USB, NX_USB)
    NX_TRAVERSE_EACH_INSTANCE(REGISTER_NATIVE_WATCHDOG, NX_WATCHDOG)
    NX_TRAVERSE_EACH_INSTANCE(REGISTER_NATIVE_OPTION_BYTES, NX_OPTION_BYTES)
    return NX_OK;
}
#undef REGISTER_NATIVE_DEVICE
#undef REGISTER_NATIVE_GPIO
#undef REGISTER_NATIVE_UART
#undef REGISTER_NATIVE_SPI
#undef REGISTER_NATIVE_I2C
#undef REGISTER_NATIVE_ADC
#undef REGISTER_NATIVE_ADC_BUFFER
#undef REGISTER_NATIVE_CRC
#undef REGISTER_NATIVE_DAC
#undef REGISTER_NATIVE_INTERNAL_FLASH
#undef REGISTER_NATIVE_RTC
#undef REGISTER_NATIVE_TIMER
#undef REGISTER_NATIVE_SDIO
#undef REGISTER_NATIVE_USB
#undef REGISTER_NATIVE_WATCHDOG
#undef REGISTER_NATIVE_OPTION_BYTES

/*---------------------------------------------------------------------------*/
/* Public Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize the Native platform
 * \return          NX_OK on success, error code otherwise
 *
 * \details         This function initializes all necessary system resources
 *                  for the Native platform. Since this is a simulation
 *                  platform running on the host, minimal initialization is
 *                  required. The function primarily ensures that all
 *                  resource managers are ready for use.
 */
nx_status_t nx_platform_init(void) {
    /* Check if already initialized */
    if (platform_initialized) {
        return NX_OK;
    }

    nx_status_t status = register_effective_devices();
    if (status != NX_OK) return status;

    /* Initialize resource managers */
    /* DMA and ISR managers use static initialization, so they are */
    /* ready to use immediately. No explicit initialization needed. */

    /* Initialize peripheral-specific resources */
    /* Most peripheral initialization is done on-demand when devices */
    /* are accessed through the factory functions. This ensures that */
    /* only the peripherals actually used by the application are */
    /* initialized, reducing overhead. */

    /* Mark as initialized */
    platform_initialized = true;

    return NX_OK;
}

/**
 * \brief           Deinitialize the Native platform
 * \return          NX_OK on success, error code otherwise
 *
 * \details         This function cleans up all platform resources and
 *                  prepares for shutdown. Cached factory handles survive platform re-init. Active I/O
 *                  lifecycles and queues are returned to a clean state. This includes resetting all peripheral
 *                  states to ensure a clean environment for subsequent
 *                  initialization or testing.
 */
nx_status_t nx_platform_deinit(void) {
    /* Check if not initialized */
    if (!platform_initialized) {
        return NX_OK;
    }

    /* The caller serializes platform lifetime against applications. Cached
     * factory objects remain valid across re-init and are not allocated here. */
    nx_status_t result=nx_native_resources_idle();
    if (result!=NX_OK) return result;
    result=nx_device_provider_stop_all();
    if (result!=NX_OK) return result;
    result=nx_device_provider_quiescence_check();
    if (result!=NX_OK) return result;

    /* Mark as not initialized */
    platform_initialized = false;

    return NX_OK;
}

/* Private HAL hook: called with the common registry admission fence held. */
nx_status_t nx_platform_shutdown_check(void) {
    return nx_native_resources_idle();
}

nx_status_t nx_platform_init_check(void) {
    return nx_native_resources_idle();
}

/**
 * \brief           Check if platform is initialized
 * \return          true if initialized, false otherwise
 *
 * \details         This function can be used to query the platform
 *                  initialization state.
 */
bool nx_platform_is_initialized(void) {
    return platform_initialized;
}
