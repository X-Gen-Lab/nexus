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

#include "hal/nx_status.h"
#include "hal/resource/nx_dma_manager.h"
#include "hal/resource/nx_isr_manager.h"
#include <stdbool.h>

/*---------------------------------------------------------------------------*/
/* External Functions                                                        */
/*---------------------------------------------------------------------------*/

/* Reset functions for peripheral modules */
extern void native_gpio_reset_all(void);
extern void native_uart_reset_all(void);
extern void native_spi_reset_all(void);
extern void native_i2c_reset_all(void);

/*---------------------------------------------------------------------------*/
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

static bool platform_initialized = false;

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
 *                  prepares for shutdown. All allocated resources should
 *                  be freed and the platform should be returned to a
 *                  clean state. This includes resetting all peripheral
 *                  states to ensure a clean environment for subsequent
 *                  initialization or testing.
 */
nx_status_t nx_platform_deinit(void) {
    /* Check if not initialized */
    if (!platform_initialized) {
        return NX_OK;
    }

    /* Clean up peripheral-specific resources */
    /* Reset all peripheral states to ensure clean shutdown */
    /* These functions are provided by each peripheral module */
    /* and reset their internal state to initial values */

    /* Reset GPIO states */
    native_gpio_reset_all();

    /* Reset UART states */
    native_uart_reset_all();

    /* Reset SPI states */
    native_spi_reset_all();

    /* Reset I2C states */
    native_i2c_reset_all();

    /* Note: DMA and ISR managers maintain their own state */
    /* and will be cleaned up when channels/handles are released */
    /* by the peripheral drivers during their deinitialization */

    /* Mark as not initialized */
    platform_initialized = false;

    return NX_OK;
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
