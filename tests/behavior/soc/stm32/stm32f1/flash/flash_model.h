/**
 * \file            flash_model.h
 * \brief           External F1 Flash controller and clock test boundary
 * \author          Nexus Team
 */
#ifndef NX_TEST_STM32F1_FLASH_MODEL_H
#define NX_TEST_STM32F1_FLASH_MODEL_H

#include "stm32f1xx.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

FLASH_TypeDef* nx_f1_flash_model_registers(void);
volatile uint8_t* nx_f1_flash_model_memory(void);
uint32_t nx_f1_flash_model_read(const volatile uint32_t* reg);
void nx_f1_flash_model_write(volatile uint32_t* reg, uint32_t value);
void nx_f1_flash_model_store(volatile uint16_t* address, uint16_t value);
bool nx_f1_flash_model_irq_idle(void);
uint32_t nx_f1_flash_model_clock(void);
uintptr_t nx_f1_flash_model_sram_start(void);

#ifdef __cplusplus
}
#endif

#define NX_F1_FLASH_REGISTERS         (nx_f1_flash_model_registers())
#define NX_F1_FLASH_MEMORY            (nx_f1_flash_model_memory())
#define NX_F1_FLASH_READ(reg)         nx_f1_flash_model_read((reg))
#define NX_F1_FLASH_WRITE(reg, value) nx_f1_flash_model_write((reg), (value))
#define NX_F1_FLASH_STORE(address, value)                                      \
    nx_f1_flash_model_store((address), (value))
#define NX_F1_FLASH_IRQ_IDLE() nx_f1_flash_model_irq_idle()
#define NX_F1_FLASH_CLOCK()    nx_f1_flash_model_clock()
#define NX_F1_FLASH_SRAM_START (nx_f1_flash_model_sram_start())

#endif /* NX_TEST_STM32F1_FLASH_MODEL_H */
