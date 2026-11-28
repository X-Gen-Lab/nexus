/**
 * \file            model.h
 * \brief           F103 ADC, GPIO and RCC register-bus model
 * \author          Nexus Team
 */
#ifndef NX_STM32F1_ADC_TEST_MODEL_H
#define NX_STM32F1_ADC_TEST_MODEL_H

#include "stm32f1xx.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
extern ADC_TypeDef f103_adc_model_adc;
extern GPIO_TypeDef f103_adc_model_gpio;
extern RCC_TypeDef f103_adc_model_rcc;
uint32_t f103_adc_model_read(const volatile uint32_t* reg);
void f103_adc_model_write(volatile uint32_t* reg, uint32_t value);
bool f103_adc_model_irq_idle(void);
#ifdef __cplusplus
}
#endif

#undef ADC1
#undef GPIOA
#undef RCC
#undef READ_REG
#undef READ_BIT
#undef WRITE_REG
#undef MODIFY_REG
#undef SET_BIT
#undef CLEAR_BIT
#define ADC1                  (&f103_adc_model_adc)
#define GPIOA                 (&f103_adc_model_gpio)
#define RCC                   (&f103_adc_model_rcc)
#define READ_REG(reg)         f103_adc_model_read(&(reg))
#define READ_BIT(reg, mask)   (READ_REG(reg) & (mask))
#define WRITE_REG(reg, value) f103_adc_model_write(&(reg), (uint32_t)(value))
#define MODIFY_REG(reg, clear, value)                                          \
    WRITE_REG(reg, (READ_REG(reg) & ~(uint32_t)(clear)) | (value))
#define SET_BIT(reg, bits)        MODIFY_REG(reg, 0U, bits)
#define CLEAR_BIT(reg, bits)      MODIFY_REG(reg, bits, 0U)
#define NX_STM32F1_ADC_IRQ_IDLE() f103_adc_model_irq_idle()
#define NX_STM32F1_WRITE(reg, value)                                           \
    f103_adc_model_write((reg), (uint32_t)(value))
#define NX_STM32F1_GPIO_BSRR(reg, value) WRITE_REG((reg)->BSRR, value)

#endif
