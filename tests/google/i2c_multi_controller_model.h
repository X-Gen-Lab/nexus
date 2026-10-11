/**
 * \file            i2c_multi_controller_model.h
 * \brief           Actual DATA and selected DMA W1C hardware boundaries
 * \author          Nexus Team
 */
#ifndef NEXUS_I2C_MULTI_CONTROLLER_MODEL_H
#define NEXUS_I2C_MULTI_CONTROLLER_MODEL_H
#include "i2c_async_register_model.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Apply the written W1C mask to its actual DMA status register. */
void nx_i2c_multi_dma_ack(volatile uint32_t* clear, uint32_t mask);
#ifdef __cplusplus
}
#endif
#define NX_I2C_V1_DMA_ACK(clear, mask) nx_i2c_multi_dma_ack((clear), (mask))
#endif
