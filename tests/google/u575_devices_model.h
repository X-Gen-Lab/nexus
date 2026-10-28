/**
 * \file            u575_devices_model.h
 * \brief           U575 device MMIO extension leaves for real production C
 * \author          Nexus Team
 */
#ifndef NX_U575_DEVICES_MODEL_H
#define NX_U575_DEVICES_MODEL_H
#include "../../drivers/i2c/i2c_v2/i2c_v2.h"
#include "u575_model.h"
#ifdef __cplusplus
extern "C" {
#endif
extern TIM_TypeDef* g_u575_pwm;
void nx_u575_devices_reset(void);
extern nx_i2c_v2_registers_t* g_u575_i2c;
extern nx_i2c_v2_registers_t g_u575_i2c_registers;
extern bool g_u575_i2c_berr;
extern bool g_u575_hsi_hold_on;
extern bool g_u575_hsi_hold_off;
extern SPI_TypeDef* g_u575_spi;
extern bool g_u575_spi_hold;
extern uint32_t g_u575_spi_reset_gap;
void nx_u575_spi_poll(void* port);
#define NX_U575_SPI_POLL(port) nx_u575_spi_poll(port)
extern EXTI_TypeDef g_u575_exti;
extern ADC_TypeDef g_u575_adc_registers;
extern ADC_Common_TypeDef g_u575_adc_common_registers;
extern ADC_TypeDef* g_u575_adc;
extern ADC_Common_TypeDef* g_u575_adc_common;
#undef ADC4
#define ADC4 (&g_u575_adc_registers)
extern bool g_u575_adc_hold;
extern bool g_u575_adc_refuse_calibration;
extern FLASH_TypeDef* g_u575_physical_flash;
extern volatile uint8_t* g_u575_flash_memory;
extern uint32_t g_u575_flash_words;
extern uint32_t g_u575_flash_polls;
extern uint32_t g_u575_flash_erase;
extern bool g_u575_flash_ignore_start;
extern bool g_u575_flash_hold_eop_clear;
extern bool g_u575_flash_omit_eop;
void nx_u575_flash_poll(void* port);
void nx_u575_flash_program32(volatile uint8_t* address, uint32_t value);
#define NX_U575_FLASH_POLL(port) nx_u575_flash_poll(port)
#define NX_U575_FLASH_PROGRAM32(address, value)                                \
    nx_u575_flash_program32((address), (value))
void nx_u575_adc_poll(void* port);
#define NX_U575_ADC_POLL(port) nx_u575_adc_poll(port)
#undef EXTI
#define EXTI (&g_u575_exti)
#undef GPIOA
#define GPIOA NX_U575_GPIOA
#undef GPIOB
#define GPIOB (&g_u575_model.extra_gpio[0])
#undef GPIOC
#define GPIOC (&g_u575_model.gpio)
#undef RCC
#define RCC (&g_u575_model.rcc)
#undef FLASH
#define FLASH (&g_u575_model.flash)
#undef SPI1
#define SPI1 g_u575_spi
#undef TIM3
#define TIM3 g_u575_pwm
#undef I2C1
#define I2C1 ((I2C_TypeDef*)&g_u575_i2c_registers)
#undef ADC4_COMMON
#define ADC4_COMMON (&g_u575_adc_common_registers)
void nx_u575_spi_write8(volatile uint32_t* reg, uint8_t value);
uint8_t nx_u575_spi_read8(volatile uint32_t* reg);
#define NX_U575_SPI_WRITE8(reg, value) nx_u575_spi_write8((reg), (value))
#define NX_U575_SPI_READ8(reg)         nx_u575_spi_read8(reg)
uint32_t nx_u575_devices_read(volatile uint32_t* reg);
void nx_u575_devices_write(volatile uint32_t* reg, uint32_t value);
#undef NX_U575_WRITE
#define NX_U575_WRITE(reg, value) nx_u575_devices_write((reg), (value))
#undef NX_U575_READ
#define NX_U575_READ(reg) nx_u575_devices_read(reg)
#undef NX_I2C_V2_READ
#undef NX_I2C_V2_WRITE
#define NX_I2C_V2_READ(reg)         NX_U575_READ(reg)
#define NX_I2C_V2_WRITE(reg, value) NX_U575_WRITE((reg), (value))
#ifdef __cplusplus
}
#endif
#endif
