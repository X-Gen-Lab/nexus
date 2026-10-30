/**
 * \file            stm32u575_i2c_port.h
 * \brief           Private official LL I2C data access for the shared engine
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32U575_I2C_PORT_H
#define NX_STM32U575_I2C_PORT_H
#include "stm32u5xx_ll_i2c.h"
#include <stddef.h>
_Static_assert(offsetof(I2C_TypeDef, RXDR) ==
                       offsetof(nx_i2c_v2_registers_t, rxdr) &&
                   offsetof(I2C_TypeDef, TXDR) ==
                       offsetof(nx_i2c_v2_registers_t, txdr),
               "Selected LL data access uses the reviewed V2 offsets");
_Static_assert(I2C_RXDR_RXDATA == UINT32_C(0xff) &&
                   I2C_TXDR_TXDATA == UINT32_C(0xff),
               "Selected LL data fields contain exactly eight bits");
#ifndef NX_I2C_V2_RECEIVE_DATA
#define NX_I2C_V2_RECEIVE_DATA(registers)                                      \
    LL_I2C_ReceiveData8((I2C_TypeDef*)(registers))
#endif
#ifndef NX_I2C_V2_TRANSMIT_DATA
#define NX_I2C_V2_TRANSMIT_DATA(registers, value)                              \
    LL_I2C_TransmitData8((I2C_TypeDef*)(registers), (value))
#endif
#endif
