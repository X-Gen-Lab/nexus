/**
 * \file            g474_acquisition_factory_model.h
 * \brief           External addresses for the real acquisition factory
 * \author          Nexus Team
 */
#ifndef NX_G474_ACQUISITION_FACTORY_MODEL_H
#define NX_G474_ACQUISITION_FACTORY_MODEL_H
#include "g474_model.h"
#include "stm32g474_acquisition.h"
#ifdef __cplusplus
extern "C" {
#endif
extern ADC_TypeDef g_g474_acquisition_adc[2];
extern ADC_Common_TypeDef g_g474_acquisition_common;
extern TIM_TypeDef g_g474_acquisition_timer;
extern DMA_TypeDef g_g474_acquisition_dma;
extern DMA_Channel_TypeDef g_g474_acquisition_channel;
extern DMAMUX_Channel_TypeDef g_g474_acquisition_mux;
extern I2C_TypeDef g_g474_acquisition_i2c;
extern SYSCFG_TypeDef g_g474_acquisition_syscfg;
void nx_g474_acquisition_factory_write(volatile uint32_t* reg, uint32_t value);
void nx_g474_acquisition_factory_poll(nx_stm32g474_adc_state_t* port);
#ifdef __cplusplus
}
#endif
#undef RCC
#undef FLASH
#undef PWR
#undef TIM2
#undef GPIOA
#undef GPIOB
#undef ADC1
#undef ADC2
#undef ADC12_COMMON
#undef TIM6
#undef DMA1
#undef DMA1_Channel1
#undef DMAMUX1_Channel0
#undef SYSCFG
#undef I2C1
#define RCC              (&g_g474_model.rcc)
#define FLASH            (&g_g474_model.flash)
#define PWR              (&g_g474_model.power)
#define TIM2             (&g_g474_model.timer)
#define GPIOA            (&g_g474_model.gpio)
#define GPIOB            (&g_g474_model.gpio_b)
#define ADC1             (&g_g474_acquisition_adc[0])
#define ADC2             (&g_g474_acquisition_adc[1])
#define ADC12_COMMON     (&g_g474_acquisition_common)
#define TIM6             (&g_g474_acquisition_timer)
#define DMA1             (&g_g474_acquisition_dma)
#define DMA1_Channel1    (&g_g474_acquisition_channel)
#define DMAMUX1_Channel0 (&g_g474_acquisition_mux)
#define SYSCFG           (&g_g474_acquisition_syscfg)
#define I2C1             (&g_g474_acquisition_i2c)
#undef NX_G474_WRITE
#undef NX_G474_ADC_POLL
#define NX_G474_WRITE(reg, value)   nx_g474_acquisition_factory_write(reg, value)
#define NX_G474_ADC_POLL(port)      nx_g474_acquisition_factory_poll(port)
#define NX_G474_DMAMUX_SELECT(mux)  NX_G474_WRITE(&(mux)->CCR, 5U)
#define NX_I2C_V2_WRITE(reg, value) NX_G474_WRITE(reg, value)
#endif
