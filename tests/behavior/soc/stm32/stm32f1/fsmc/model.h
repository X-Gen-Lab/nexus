/**
 * \file            model.h
 * \brief           STM32F103ZG FSMC and GPIO MMIO boundary.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_F1_FSMC_MODEL_H
#define NEXUS_TEST_F1_FSMC_MODEL_H

#include "stm32f1xx.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern FSMC_Bank1_TypeDef f1_fsmc_registers;
extern GPIO_TypeDef f1_fsmc_gpios[4];
extern RCC_TypeDef f1_fsmc_rcc;
void f1_fsmc_write(volatile uint32_t* reg, uint32_t value);
bool f1_fsmc_irq_idle(void);
#if defined(NX_STM32F1_FSMC_LCD)
void f1_fsmc_write16(uintptr_t address, uint16_t word);
uint16_t f1_fsmc_read16(uintptr_t address);
#endif

#ifdef __cplusplus
}
#endif

#undef FSMC_Bank1
#undef RCC
#undef GPIOD
#undef GPIOE
#undef GPIOF
#undef GPIOG
#define FSMC_Bank1                       (&f1_fsmc_registers)
#define RCC                              (&f1_fsmc_rcc)
#define GPIOD                            (&f1_fsmc_gpios[0])
#define GPIOE                            (&f1_fsmc_gpios[1])
#define GPIOF                            (&f1_fsmc_gpios[2])
#define GPIOG                            (&f1_fsmc_gpios[3])
#define NX_STM32F1_WRITE(reg, value)     f1_fsmc_write((reg), (value))
#define NX_STM32F1_GPIO_BSRR(reg, value) f1_fsmc_write(&(reg)->BSRR, (value))
#define NX_STM32F1_FSMC_IRQ_IDLE()       f1_fsmc_irq_idle()
#if defined(NX_STM32F1_FSMC_LCD)
#define NX_STM32F1_FSMC_WRITE16(address, word) f1_fsmc_write16(address, word)
#define NX_STM32F1_FSMC_READ16(address)        f1_fsmc_read16(address)
#endif

#endif /* NEXUS_TEST_F1_FSMC_MODEL_H */
