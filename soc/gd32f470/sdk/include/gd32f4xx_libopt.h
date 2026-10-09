#ifndef NEXUS_GD32F4XX_LIBOPT_H
#define NEXUS_GD32F4XX_LIBOPT_H
#include "nexus_config.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_gpio.h"
#ifdef NX_CONFIG_GD32_UART_ENABLE
#include "gd32f4xx_usart.h"
#endif
#ifdef NX_CONFIG_GD32_SPI_ENABLE
#include "gd32f4xx_spi.h"
#endif
#include "gd32f4xx_fmc.h"
#include "gd32f4xx_dbg.h" /* Identity register definitions; no DBG functions. */
#include "gd32f4xx_pmu.h" /* Clock regulator registers; no PMU functions. */
#include "gd32f4xx_timer.h"
#endif
