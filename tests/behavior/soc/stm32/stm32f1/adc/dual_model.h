/**
 * \file            dual_model.h
 * \brief           F103ZE regular simultaneous ADC and DMA register model.
 * \author          Nexus Team
 */
#ifndef NX_STM32F1_ADC_DUAL_TEST_MODEL_H
#define NX_STM32F1_ADC_DUAL_TEST_MODEL_H

#include "stm32f1xx.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
extern ADC_TypeDef dual_model_master;
extern ADC_TypeDef dual_model_slave;
extern GPIO_TypeDef dual_model_gpio;
extern RCC_TypeDef dual_model_rcc;
extern DMA_TypeDef dual_model_dma;
extern DMA_Channel_TypeDef dual_model_lane;
void dual_model_write(volatile uint32_t* reg, uint32_t value);
bool dual_model_irq_idle(void);
#ifdef __cplusplus
}
#endif
#undef ADC1
#undef ADC2
#undef GPIOA
#undef RCC
#undef DMA1
#undef DMA1_Channel1
#undef WRITE_REG
#undef MODIFY_REG
#undef SET_BIT
#undef CLEAR_BIT
#define ADC1                  (&dual_model_master)
#define ADC2                  (&dual_model_slave)
#define GPIOA                 (&dual_model_gpio)
#define RCC                   (&dual_model_rcc)
#define DMA1                  (&dual_model_dma)
#define DMA1_Channel1         (&dual_model_lane)
#define WRITE_REG(reg, value) dual_model_write(&(reg), (uint32_t)(value))
#define MODIFY_REG(reg, clear, value)                                          \
    WRITE_REG(reg, (((reg) & ~(uint32_t)(clear)) | (value)))
#define SET_BIT(reg, bits)               MODIFY_REG(reg, 0U, bits)
#define CLEAR_BIT(reg, bits)             MODIFY_REG(reg, bits, 0U)
#define NX_STM32F1_WRITE(reg, value)     dual_model_write((reg), (uint32_t)(value))
#define NX_STM32F1_GPIO_BSRR(reg, value) WRITE_REG((reg)->BSRR, value)
#define NX_STM32F1_ADC_DUAL_IRQS_IDLE()  dual_model_irq_idle()
#endif
