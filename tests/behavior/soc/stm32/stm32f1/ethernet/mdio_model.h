/**
 * \file            mdio_model.h
 * \brief           Observe actual F107 MDIO and GPIO production writes.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_MDIO_TEST_MODEL_H
#define NEXUS_STM32F1_MDIO_TEST_MODEL_H

#include "stm32f1xx.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_test_f107_write(volatile uint32_t* reg, uint32_t value);
void nx_test_f107_bsrr(GPIO_TypeDef* reg, uint32_t value);
#ifdef __cplusplus
}
#endif
#define NX_STM32F1_WRITE(reg, value)     nx_test_f107_write(reg, value)
#define NX_STM32F1_GPIO_BSRR(reg, value) nx_test_f107_bsrr(reg, value)
#endif
