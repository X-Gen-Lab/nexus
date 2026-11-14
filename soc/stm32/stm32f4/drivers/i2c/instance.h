/**
 * \file            instance.h
 * \brief           Private STM32F4 polling I2C controller and endpoint state
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_I2C_INSTANCE_H
#define NX_STM32F4_I2C_INSTANCE_H

#include "nexus/io/i2c.h"
#include "private/model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief I2C owns the selected immutable line facts; other ports need no IP.
 */
typedef struct nx_i2c_v1_lines nx_i2c_v1_lines_t;

typedef struct {
    I2C_TypeDef* registers;
    const nx_i2c_v1_lines_t* lines;
    uint32_t peripheral_mhz;
    uint32_t rate_hz;
    bool active;
    bool fault;
    bool initialized;
} nx_stm32_i2c_state_t;
typedef struct {
    nx_stm32_i2c_state_t* port;
    uint16_t address;
    uint8_t address_bits;
} nx_stm32_i2c_endpoint_state_t;

/** \brief Initialize standard-mode I2C at a fixed 42 MHz peripheral clock. */
nx_result_t nx_stm32_i2c_initialize(nx_stm32_i2c_state_t* port);
extern const nx_i2c_ops_t nx_stm32_i2c_ops;
nx_result_t nx_stm32_i2c_recover(void* context);
extern const nx_i2c_endpoint_ops_t nx_stm32_i2c_endpoint_ops;
nx_result_t nx_stm32_i2c_endpoint_transaction(void* context,
                                              nx_i2c_message_t* messages,
                                              size_t count,
                                              nx_time_us_t deadline,
                                              size_t* transferred);

#ifdef __cplusplus
}
#endif
#endif /* NX_STM32F4_I2C_INSTANCE_H */
