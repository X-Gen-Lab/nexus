/**
 * \file            i2c_official_leaf_model.h
 * \brief           Default I2C data access with external W1C/reset behavior
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_I2C_OFFICIAL_LEAF_MODEL_H
#define NX_I2C_OFFICIAL_LEAF_MODEL_H
#if defined(STM32F103xB)
#ifdef __cplusplus
#include "stm32f1xx.h"
#else
#include "stm32f1xx_ll_i2c.h"
#endif
#elif defined(STM32F407xx)
#ifdef __cplusplus
#include "stm32f4xx.h"
#else
#include "stm32f4xx_ll_i2c.h"
#endif
#else
#error "The I2C leaf probe needs one reviewed STM32 part"
#endif
#ifdef __cplusplus
extern "C" {
#endif
uint8_t nx_i2c_leaf_official_receive(I2C_TypeDef* registers);
void nx_i2c_leaf_official_transmit(I2C_TypeDef* registers, uint8_t value);
#include "i2c_v1_async.h"
void nx_i2c_leaf_official_clear_address(I2C_TypeDef* registers);
#ifdef __cplusplus
}
#endif
#endif
