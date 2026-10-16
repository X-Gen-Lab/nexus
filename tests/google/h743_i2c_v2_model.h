/**
 * \file            h743_i2c_v2_model.h
 * \brief           External V2 register handshake model boundaries
 * \author          Nexus Team
 */
#ifndef NX_H743_I2C_V2_MODEL_H
#define NX_H743_I2C_V2_MODEL_H
#include "h743_model.h"
#include "i2c_v2.h"
#undef GPIOB
#define GPIOB (&g_h743_model.gpio_b)
#ifdef __cplusplus
extern "C" {
#endif
void nx_test_i2c_v2_write(volatile uint32_t* reg, uint32_t value);
void nx_test_i2c_v2_poll(nx_i2c_v2_state_t* port);
#ifdef __cplusplus
}
#endif
#undef NX_I2C_V2_WRITE
#undef NX_I2C_V2_POLL
#define NX_I2C_V2_WRITE(reg, value) nx_test_i2c_v2_write(reg, value)
#define NX_I2C_V2_POLL(port)        nx_test_i2c_v2_poll(port)
#endif
