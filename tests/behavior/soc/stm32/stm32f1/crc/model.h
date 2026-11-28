/**
 * \file            model.h
 * \brief           F103 CRC bus side effects for the focused Native contract.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_STM32F1_CRC_MODEL_H
#define NEXUS_TEST_STM32F1_CRC_MODEL_H

#include "stm32f1xx.h"

#ifdef __cplusplus
extern "C" {
#endif
void nx_f1_crc_model_write(volatile uint32_t* reg, uint32_t value);
void nx_f1_crc_model_feed(CRC_TypeDef* reg, uint32_t value);
#ifdef __cplusplus
}
#endif

#define NX_STM32F1_WRITE(reg, value)    nx_f1_crc_model_write((reg), (value))
#define NX_STM32F1_CRC_FEED(reg, value) nx_f1_crc_model_feed((reg), (value))
#endif
