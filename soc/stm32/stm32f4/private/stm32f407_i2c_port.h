/**
 * \file            stm32f407_i2c_port.h
 * \brief           Private official LL I2C data access for the shared engine
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32F407_I2C_PORT_H
#define NX_STM32F407_I2C_PORT_H
#include "stm32f4xx_ll_i2c.h"
#include <stddef.h>
_Static_assert(offsetof(I2C_TypeDef, DR) == 4U * sizeof(uint32_t),
               "Selected LL receive uses the reviewed V1 DATA offset");
_Static_assert(I2C_DR_DR == UINT32_C(0xff),
               "Selected LL receive consumes exactly eight DATA bits");
#ifndef NX_I2C_V1_READ_DATA
#define NX_I2C_V1_READ_DATA(state)                                             \
    LL_I2C_ReceiveData8((I2C_TypeDef*)(state)->registers)
#endif
#endif
