/**
 * \file            native_platform.h
 * \brief           Native Platform Main Header
 * \author          Nexus Team
 * \version         2.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Main header file for the Native platform. This header
 *                  provides a unified interface to all peripheral test
 *                  helpers by including modular peripheral-specific headers.
 *                  The Native platform serves as a host-based simulation
 *                  environment for testing Nexus HAL implementations.
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
#include "hal/nx_status.h"

/*---------------------------------------------------------------------------*/
/* Native Platform Test Helpers                                              */
/*---------------------------------------------------------------------------*/

#include "native_adc_test.h"
#include "native_gpio_test.h"
#include "native_i2c_test.h"
#include "native_rtc_test.h"
#include "native_spi_test.h"
#include "native_timer_test.h"
#include "native_uart_test.h"
#include "native_usb_test.h"
#include "native_watchdog_test.h"

#endif /* NATIVE_PLATFORM_H */
