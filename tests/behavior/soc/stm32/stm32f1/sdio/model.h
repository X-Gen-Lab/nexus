/**
 * \file            model.h
 * \brief           Side-effectful F103 SDIO and RCC register model
 * \author          Nexus Team
 */
#ifndef NX_TEST_F1_SDIO_MODEL_H
#define NX_TEST_F1_SDIO_MODEL_H

#include "stm32f1xx.h"

#ifdef __cplusplus
extern "C" {
#endif
extern RCC_TypeDef nx_sdio_rcc;
extern SDIO_TypeDef nx_sdio_registers;
extern GPIO_TypeDef nx_sdio_gpio_c;
extern GPIO_TypeDef nx_sdio_gpio_d;
void nx_sdio_model_bsrr(GPIO_TypeDef* gpio, uint32_t value);
void nx_sdio_model_write(volatile uint32_t* reg, uint32_t value);
uint32_t nx_sdio_model_fifo(void);
#ifdef __cplusplus
}
#endif

#undef RCC
#undef SDIO
#undef GPIOC
#undef GPIOD
#define GPIOC                             (&nx_sdio_gpio_c)
#define GPIOD                             (&nx_sdio_gpio_d)
#define NX_STM32F1_GPIO_BSRR(gpio, value) nx_sdio_model_bsrr((gpio), (value))
#define RCC                               (&nx_sdio_rcc)
#define SDIO                              (&nx_sdio_registers)
#define NX_STM32F1_WRITE(reg, value)      nx_sdio_model_write(reg, value)
#define NX_STM32F1_SDIO_FIFO_READ()       nx_sdio_model_fifo()
#endif
