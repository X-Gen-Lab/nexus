/**
 * \file            i2c_async_register_model.h
 * \brief           External hardware DATA/ADDR/DMA disable boundaries
 * \author          Nexus Team
 */
#ifndef NEXUS_I2C_ASYNC_REGISTER_MODEL_H
#define NEXUS_I2C_ASYNC_REGISTER_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint8_t nx_i2c_v1_model_read(void* state);
void nx_i2c_v1_model_write(void* state, uint32_t value);
void nx_i2c_v1_model_address_cleared(void* state);
void nx_i2c_v1_model_disable(volatile uint32_t* control, uint32_t mask);
bool nx_i2c_v1_model_privileged(void);
#ifdef __cplusplus
}
#endif
#define NX_I2C_V1_READ_DATA(state)         nx_i2c_v1_model_read(state)
#define nx_arch_is_privileged              nx_i2c_v1_model_privileged
#define NX_I2C_V1_WRITE_DATA(state, value) nx_i2c_v1_model_write(state, value)
#define NX_I2C_V1_ADDRESS_CLEARED(state)   nx_i2c_v1_model_address_cleared(state)
#define NX_I2C_V1_DMA_DISABLE(control, mask)                                   \
    nx_i2c_v1_model_disable(control, mask)
#endif
