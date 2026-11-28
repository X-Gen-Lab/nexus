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
extern ADC_TypeDef f103_adc_model_adc2;
#if defined(STM32F103xE)
extern ADC_TypeDef f103_adc_model_adc3;
#endif
extern GPIO_TypeDef f103_adc_model_gpio;
extern RCC_TypeDef f103_adc_model_rcc;
extern TIM_TypeDef f103_adc_model_tim;
extern TIM_TypeDef f103_adc_model_tim1;
extern DMA_TypeDef f103_adc_model_dma;
extern DMA_Channel_TypeDef f103_adc_model_lane;
uint32_t f103_adc_model_read(const volatile uint32_t* reg);
void f103_adc_model_write(volatile uint32_t* reg, uint32_t value);
bool f103_adc_model_irq_idle(void);
#if defined(STM32F103xE)
bool f103_adc_model_poll_irq_idle(IRQn_Type irq);
#endif
#ifdef __cplusplus
}
#endif

#undef ADC1
#undef ADC2
#if defined(STM32F103xE)
#undef ADC3
#define ADC3 (&f103_adc_model_adc3)
#endif
#undef GPIOA
#undef RCC
#undef TIM3
#undef TIM1
#undef DMA1
#undef DMA1_Channel1
#undef READ_REG
#undef READ_BIT
#undef WRITE_REG
#undef MODIFY_REG
#undef SET_BIT
#undef CLEAR_BIT
#define ADC2                                 (&f103_adc_model_adc2)
#define ADC1                                 (&f103_adc_model_adc)
#define GPIOA                                (&f103_adc_model_gpio)
#define TIM1                                 (&f103_adc_model_tim1)
#define TIM3                                 (&f103_adc_model_tim)
#define DMA1                                 (&f103_adc_model_dma)
#define DMA1_Channel1                        (&f103_adc_model_lane)
#define NX_STM32F1_ADC_TRIGGER_IRQ_IDLE(...) f103_adc_model_irq_idle()
#define RCC                                  (&f103_adc_model_rcc)
#define READ_REG(reg)                        f103_adc_model_read(&(reg))
#define READ_BIT(reg, mask)                  (READ_REG(reg) & (mask))
#define WRITE_REG(reg, value)                f103_adc_model_write(&(reg), (uint32_t)(value))
#define MODIFY_REG(reg, clear, value)                                          \
    WRITE_REG(reg, (READ_REG(reg) & ~(uint32_t)(clear)) | (value))
#define SET_BIT(reg, bits)   MODIFY_REG(reg, 0U, bits)
#define CLEAR_BIT(reg, bits) MODIFY_REG(reg, bits, 0U)
#if defined(STM32F103xE)
#define NX_STM32F1_ADC_IRQ_IDLE(irq) f103_adc_model_poll_irq_idle(irq)
#else
#define NX_STM32F1_ADC_IRQ_IDLE(irq) ((void)(irq), f103_adc_model_irq_idle())
#endif
#define NX_STM32F1_WRITE(reg, value)                                           \
    f103_adc_model_write((reg), (uint32_t)(value))
#define NX_STM32F1_GPIO_BSRR(reg, value) WRITE_REG((reg)->BSRR, value)

#endif
