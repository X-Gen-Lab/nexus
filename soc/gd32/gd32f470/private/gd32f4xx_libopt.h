/**
 * \file            gd32f4xx_libopt.h
 * \brief           Private GD32F470 SDK include selection
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32_NEXTGEN_LIBOPT_H
#define NEXUS_GD32_NEXTGEN_LIBOPT_H

#include "gd32f4xx_adc.h"
#include "gd32f4xx_dbg.h"
#include "gd32f4xx_dma.h"
#include "gd32f4xx_exti.h"
#include "gd32f4xx_fmc.h"
#include "gd32f4xx_fwdgt.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_i2c.h"
#include "gd32f4xx_pmu.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_spi.h"
#include "gd32f4xx_syscfg.h"
#include "gd32f4xx_timer.h"
#include "gd32f4xx_usart.h"

#ifdef NX_GD32_HOST_DBG_BASE
/* The host fake CMSIS header supplies this model-only address relocation.
 * No production target declares it and the official SDK remains unchanged.
 */
#undef DBG_BASE
#define DBG_BASE NX_GD32_HOST_DBG_BASE
#endif

#endif
