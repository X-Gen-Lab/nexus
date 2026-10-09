/**
 * \file            native_platform.h
 * \brief           Native Platform Main Header
 * \author          Nexus Team
 * \version         2.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Main header file for the Native platform. This header
 *                  provides access to HAL interfaces. The Native platform
 *                  serves as a host-based simulation environment for testing
 *                  Nexus HAL implementations.
 *
 *                  To use devices, include hal/nx_factory.h and use the
 *                  nx_factory_*() functions to get device instances.
 */

#ifndef NATIVE_PLATFORM_H
#define NATIVE_PLATFORM_H

/*---------------------------------------------------------------------------*/
/* HAL Interface Includes                                                    */
/*---------------------------------------------------------------------------*/

#include "hal/interface/nx_adc.h"
#include "hal/interface/nx_gpio.h"
#include "hal/interface/nx_i2c.h"
#include "hal/interface/nx_rtc.h"
#include "hal/interface/nx_spi.h"
#include "hal/interface/nx_timer.h"
#include "hal/interface/nx_uart.h"
#include "hal/interface/nx_usb.h"
#include "hal/interface/nx_watchdog.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"

/** Private global teardown admission: allocated DMA and connected/active IRQ
 * owners require caller settlement/release before HAL can stop the platform. */
nx_status_t nx_native_dma_idle(void);
nx_status_t nx_native_resources_idle(void);

#endif /* NATIVE_PLATFORM_H */
