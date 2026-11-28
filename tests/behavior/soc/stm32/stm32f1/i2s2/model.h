/**
 * \file            model.h
 * \brief           F103 I2S2, DMA and GPIO register model.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_F1_I2S2_MODEL_H
#define NEXUS_TEST_F1_I2S2_MODEL_H
#include "stm32f1xx.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
void nx_i2s2_model_write(volatile uint32_t* reg, uint32_t value);
void nx_i2s2_model_bsrr(GPIO_TypeDef* reg, uint32_t value);
uint32_t nx_i2s2_model_status(SPI_TypeDef* reg);
bool nx_i2s2_model_irq_enabled(IRQn_Type irq);
bool nx_i2s2_model_irq_pending(IRQn_Type irq);
bool nx_i2s2_model_irq_active(IRQn_Type irq);
uint32_t nx_i2s2_model_irq_priority(IRQn_Type irq);
void nx_i2s2_model_irq_priority_set(IRQn_Type irq, uint32_t value);
void nx_i2s2_model_irq_enable(IRQn_Type irq);
void nx_i2s2_model_irq_disable(IRQn_Type irq);
void nx_i2s2_model_irq_clear(IRQn_Type irq);
#ifdef __cplusplus
}
#endif
#define NX_STM32F1_IRQ_ENABLED(irq)  nx_i2s2_model_irq_enabled(irq)
#define NX_STM32F1_IRQ_PENDING(irq)  nx_i2s2_model_irq_pending(irq)
#define NX_STM32F1_IRQ_ACTIVE(irq)   nx_i2s2_model_irq_active(irq)
#define NX_STM32F1_IRQ_PRIORITY(irq) nx_i2s2_model_irq_priority(irq)
#define NX_STM32F1_IRQ_SET_PRIORITY(irq, value)                                \
    nx_i2s2_model_irq_priority_set((irq), (value))
#define NX_STM32F1_IRQ_ENABLE(irq)       nx_i2s2_model_irq_enable(irq)
#define NX_STM32F1_IRQ_DISABLE(irq)      nx_i2s2_model_irq_disable(irq)
#define NX_STM32F1_IRQ_CLEAR(irq)        nx_i2s2_model_irq_clear(irq)
#define NX_STM32F1_WRITE(reg, value)     nx_i2s2_model_write((reg), (value))
#define NX_STM32F1_GPIO_BSRR(reg, value) nx_i2s2_model_bsrr((reg), (value))
#define NX_STM32F1_SPI_STATUS(reg)       nx_i2s2_model_status(reg)
#endif
