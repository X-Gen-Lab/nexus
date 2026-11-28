/**
 * \file            instance.h
 * \brief           Private STM32F103xB I2C1 and fixed-address endpoint
 * \author          Nexus Team
 */
#ifndef NX_STM32F1_I2C_INSTANCE_H
#define NX_STM32F1_I2C_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "nexus/io/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Sole PB6/PB7 default-route poller, initialized to zero. */
typedef struct {
    nx_stm32f1_pads_t pads;
    uint8_t effects;
} nx_stm32f1_i2c_t;

typedef struct {
    nx_stm32f1_i2c_t* port;
    uint16_t address;
    uint8_t address_bits;
} nx_stm32f1_i2c_endpoint_t;

extern const nx_i2c_ops_t nx_stm32f1_i2c_ops;
extern const nx_i2c_endpoint_ops_t nx_stm32f1_i2c_endpoint_ops;

/** \brief Acquire reset controller and default-route pads under HSI8.
 * \note Privileged unmasked Thread. A partially acquired owner must close.
 */
nx_result_t nx_stm32f1_i2c_prepare(nx_stm32f1_i2c_t* state, uint32_t clock_hz,
                                   uint32_t max_hz);

/** \brief Restore reversible gates and pads after CPU/bus settlement.
 * \note Privileged unmasked Thread. A failed release retains the owner.
 */
nx_result_t nx_stm32f1_i2c_close(nx_stm32f1_i2c_t* state);

#ifdef __cplusplus
}
#endif
#endif
