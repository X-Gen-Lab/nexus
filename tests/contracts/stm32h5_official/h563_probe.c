/**
 * \file            h563_probe.c
 * \brief           Private H563 Device/Core and HAL/LL declaration probe
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h5xx_hal.h"
#include "stm32h5xx_ll_bus.h"
#include "stm32h5xx_ll_gpio.h"
#include "stm32h5xx_ll_pwr.h"
#include "stm32h5xx_ll_rcc.h"
#include "stm32h5xx_ll_system.h"
#include <stddef.h>

#ifdef __cplusplus
#define NX_H5_ASSERT static_assert
#else
#define NX_H5_ASSERT _Static_assert
#endif

NX_H5_ASSERT(__CORTEX_M == 33U && __NVIC_PRIO_BITS == 4U,
             "Selected H563 device/Core declarations changed");
NX_H5_ASSERT(I3C1_EV_IRQn == 123 && I3C1_ER_IRQn == 124,
             "Selected H563 I3C IRQ declarations changed");
NX_H5_ASSERT(offsetof(I3C_TypeDef, RDR) == 0x10U &&
                 offsetof(I3C_TypeDef, TDWR) == 0x1CU,
             "Selected H563 I3C byte/word FIFO declarations changed");
NX_H5_ASSERT(sizeof(((I3C_TypeDef*)0)->DEVRX) / sizeof(uint32_t) == 4U,
             "Selected H563 IBI characteristic slot declarations changed");

uint32_t nx_h563_official_compile_probe(I3C_HandleTypeDef* handle,
                                        I3C_TypeDef* registers);

/** \brief Compile official APIs; this object is never a firmware provider. */
uint32_t nx_h563_official_compile_probe(I3C_HandleTypeDef* handle,
                                        I3C_TypeDef* registers) {
    LL_I3C_EnableArbitrationHeader(registers);
    LL_I3C_TransmitData8(registers, LL_I3C_ReceiveData8(registers));
    return (uint32_t)HAL_I3C_GetState(handle);
}
