/**
 * \file            i2c_register_model.h
 *
 * \brief           Host-only I2C hardware event and DATA read boundaries
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_I2C_REGISTER_MODEL_H
#define NEXUS_I2C_REGISTER_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Advance hardware flags without replacing the production algorithm. */
void nx_i2c_model_poll(void* port);
/** \brief Consume the next byte from the modeled two-register receive pipe. */
uint8_t nx_i2c_model_read(void* port);
/** \brief Observe the mandatory status read sequence and ACK/POS ordering. */
void nx_i2c_model_address_cleared(void* port);
#ifdef __cplusplus
}
#endif
#define NX_STM32_IO_POLL(kind, port)       nx_i2c_model_poll(port)
#define NX_STM32_I2C_READ_DATA(port)       nx_i2c_model_read(port)
#define NX_STM32_I2C_ADDRESS_CLEARED(port) nx_i2c_model_address_cleared(port)
#define NX_GD32_I2C_POLL(port)             nx_i2c_model_poll(port)
#define NX_GD32_I2C_READ_DATA(port)        nx_i2c_model_read(port)
#define NX_GD32_I2C_ADDRESS_CLEARED(port)  nx_i2c_model_address_cleared(port)
#endif
