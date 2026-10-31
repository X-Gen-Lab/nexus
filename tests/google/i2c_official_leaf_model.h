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
#elif defined(STM32G474xx)
#ifdef __cplusplus
#include "stm32g4xx.h"
#else
#include "stm32g4xx_ll_i2c.h"
#endif
#elif defined(STM32H743xx)
#ifdef __cplusplus
/* Host C++ uses the real register declarations without unused Arm-only
 * helpers that cast 64-bit host pointers to the 32-bit CPU address type.
 * The actual C LL probe and firmware retain the complete CMSIS header. */
#define ARM_ARMV7M_CACHEL1_H
#include "stm32h7xx.h"
#else
#include "stm32h7xx_ll_i2c.h"
#endif
#elif defined(STM32U575xx)
#ifdef __cplusplus
#include "stm32u5xx.h"
#else
#include "stm32u5xx_ll_i2c.h"
#endif
#else
#error "The I2C leaf probe needs one reviewed STM32 part"
#endif
#ifdef __cplusplus
extern "C" {
#endif
uint8_t nx_i2c_leaf_official_receive(I2C_TypeDef* registers);
void nx_i2c_leaf_official_transmit(I2C_TypeDef* registers, uint8_t value);
#if defined(NX_I2C_LEAF_V1)
#include "i2c_v1_async.h"
void nx_i2c_leaf_official_clear_address(I2C_TypeDef* registers);
#else
#include "i2c_v2.h"
/** \brief Capture the production default before wrapping W1C/reset only.
 * \note Data accesses retain their original default expressions. The model
 * never replaces an LL inline body or supplies a received byte callback.
 */
static inline void nx_i2c_leaf_default_write(volatile uint32_t* reg,
                                             uint32_t value) {
    NX_I2C_V2_WRITE(reg, value);
}
void nx_i2c_leaf_hardware_write(volatile uint32_t* reg, uint32_t value);
void nx_i2c_leaf_hardware_poll(nx_i2c_v2_state_t* state);
#undef NX_I2C_V2_WRITE
#undef NX_I2C_V2_POLL
#define NX_I2C_V2_WRITE(reg, value) nx_i2c_leaf_hardware_write(reg, value)
#define NX_I2C_V2_POLL(state)       nx_i2c_leaf_hardware_poll(state)
#endif
#ifdef __cplusplus
}
#endif
#endif
