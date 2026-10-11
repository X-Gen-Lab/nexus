/**
 * \file            FreeRTOSConfig.h
 * \brief           Exact test clock facts for optional SoC timer bindings
 * \author          Nexus Team
 */
#ifndef NX_TIMEBASE_SOC_TEST_CONFIG_H
#define NX_TIMEBASE_SOC_TEST_CONFIG_H
#include "../../contracts/os_freertos_runtime/FreeRTOSConfig.h"
#ifdef NX_TIMEBASE_GD_MODEL
#undef configCPU_CLOCK_HZ
#define configCPU_CLOCK_HZ 200000000u
#endif
#define configUSE_TICKLESS_IDLE 2
#endif /* NX_TIMEBASE_SOC_TEST_CONFIG_H */
