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
#include <stdbool.h>
#include <stddef.h>
_Static_assert(offsetof(I2C_TypeDef, DR) == 4U * sizeof(uint32_t),
               "Selected LL receive uses the reviewed V1 DATA offset");
_Static_assert(I2C_DR_DR == UINT32_C(0xff),
               "Selected LL receive consumes exactly eight DATA bits");
/* The selected SDK includes the complete controller register metadata. */
#define NX_I2C_V1_REGISTER_BYTES sizeof(I2C_TypeDef)

#ifndef NX_I2C_V1_READ_DATA
#define NX_I2C_V1_READ_DATA(state)                                             \
    LL_I2C_ReceiveData8((I2C_TypeDef*)(state)->registers)
#endif

/* I2C3's reviewed SCL/SDA span two GPIO banks. Derive this fixed physical
 * tuple from controller identity instead of adding mutable pointer fields. */
static inline bool nx_stm32_i2c_lines_high(const I2C_TypeDef* registers,
                                           const volatile uint32_t* inputs,
                                           uint32_t mask) {
    return registers == I2C3 ? (GPIOA->IDR & (1U << 8U)) != 0U &&
                                   (GPIOC->IDR & (1U << 9U)) != 0U
                             : (*inputs & mask) == mask;
}
#ifndef NX_I2C_V1_LINES_HIGH
#define NX_I2C_V1_LINES_HIGH(state)                                            \
    nx_stm32_i2c_lines_high((I2C_TypeDef*)(state)->registers,                  \
                            (state)->line_inputs, (state)->line_mask)
#endif
#endif
